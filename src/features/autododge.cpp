#include "autododge.h"
#include "../core/commands.h"
#include "../core/scan.h"
#include "../core/scstring.h"
#define PROJ_DATA_BLOCK_SIZE 0x1E8
#include "../utils/walls.h"
#include <cmath>
#include <cstdint>
#include <cstring>
int active = 0, in_move = 0;
int ad_beam_n = 0, ad_ind_n = 0;
int32_t ad_target_x = INT32_MAX, ad_target_y = INT32_MAX;
int ad_best_dir = -1;
int ad_char_n = 0, ad_proj_n = 0, ad_enemy_n = 0, ad_threat_n = 0;
int ad_threat_pid[AD_REPORT_MAX] = {0};
float ad_threat_x[AD_REPORT_MAX] = {0}, ad_threat_y[AD_REPORT_MAX] = {0};
float ad_threat_vx[AD_REPORT_MAX] = {0}, ad_threat_vy[AD_REPORT_MAX] = {0};
float ad_threat_r[AD_REPORT_MAX] = {0}, ad_threat_d[AD_REPORT_MAX] = {0};
float ad_threat_ttc[AD_REPORT_MAX] = {0}, ad_threat_ca[AD_REPORT_MAX] = {0};
char ad_threat_name[AD_REPORT_MAX][40] = {{0}};
int ad_hit_dmg = 0, ad_hit_act = 0, ad_hit_n = 0, ad_hit_x = 0, ad_hit_y = 0;
int ad_hit_picked = 0;
float ad_hit_ca[AD_REPORT_MAX] = {0}, ad_hit_ttc[AD_REPORT_MAX] = {0};
float ad_hit_r[AD_REPORT_MAX] = {0};
int ad_enemy_pid[AD_ENEMY_MAX] = {0};
float ad_enemy_x[AD_ENEMY_MAX] = {0}, ad_enemy_y[AD_ENEMY_MAX] = {0};
int ad_enemy_hp[AD_ENEMY_MAX] = {0};
float ad_enemy_d[AD_ENEMY_MAX] = {0};
int ad_enemy_aim[AD_ENEMY_MAX] = {0};
namespace
{
constexpr float OWN_HITBOX = 120.0f;
constexpr float VV_EPS = 1e-6f;
constexpr float REACH = OWN_HITBOX * 6.0f;
constexpr float HOLD_EPS = 6.0f;
constexpr int DIRS = 16;
constexpr float TICK_MS_SEC = (float)ad::TICK_MS * 0.001f;
}
namespace ad
{
Threat threats[THREAT_MAX];
int threat_n = 0;
Proj projs[PROJ_SLOTS];
uintptr_t char_vt = 0;
float self_speed = 1.0f;
int pend_dmg = 0;
float clear_one(float mx, float my, float dx, float dy, const Threat& t)
{
    float sx = t.vx - dx * self_speed, sy = t.vy - dy * self_speed;
    float rx = t.x - mx, ry = t.y - my;
    float vv = sx * sx + sy * sy;
    float tt = 0.0f;
    if (vv > VV_EPS)
    {
        tt = -(rx * sx + ry * sy) / vv;
        if (tt < 0.0f) tt = 0.0f;
    }
    float cx = rx + sx * tt, cy = ry + sy * tt;
    return sqrtf(cx * cx + cy * cy) - t.rad;
}
}
namespace ad
{
Proj* find_proj(int32_t pid)
{
    Proj* spare = nullptr;
    Proj* stale = &projs[0];
    for (int i = 0; i < PROJ_SLOTS; i++)
    {
        if (projs[i].pid == pid) return &projs[i];
        if (projs[i].pid == 0)
        {
            if (!spare) spare = &projs[i];
            continue;
        }
        if (projs[i].seen < stale->seen) stale = &projs[i];
    }
    return spare ? spare : stale;
}

void init_proj(
    Proj* st, int32_t pid, uintptr_t dataPtr, float px, float py, uint64_t now)
{
    uint8_t data[PROJ_DATA_BLOCK_SIZE];
    int32_t speed = 0;
    int32_t radius = 0;
    if (!read_bytes(dataPtr, data, sizeof(data))) return;
    memcpy(&speed, data + PROJECTILEDATA_SPEED_OFF, sizeof(speed));
    memcpy(&radius, data + PROJECTILEDATA_RADIUS_OFF, sizeof(radius));
    if (speed <= 0) return;
    if (radius < 0) radius = 0;
    st->pid = pid;
    st->seen = now;
    st->x = px;
    st->y = py;
    st->speed = (float)speed;
    st->rad = (float)radius;
    st->vel = false;
    st->ux = 0.0f;
    st->uy = 0.0f;
    st->flags = 0;
    if (data[PROJECTILEDATA_ISBEAM_OFF]) st->flags |= B_BEAM;
    if (data[PROJECTILEDATA_INDIRECT_OFF]) st->flags |= B_IND;
    if (data[PROJECTILEDATA_PIERCES_ENV_OFF] ||
        data[PROJECTILEDATA_PIERCES_ENV_BUTTER_OFF])
        st->flags |= B_PIERCE;
    st->name[0] = '\0';
    uintptr_t str = 0;
    if (addr_dataname)
    {
        using name_fn = uintptr_t (*)(uintptr_t);
        str = reinterpret_cast<name_fn>(addr_dataname)(dataPtr);
    }
    if (str)
        read_scstring(
            str, SCSTRING_DEFAULT_MAX_LENGTH, st->name, sizeof(st->name));
    int near = -1;
    float best = 1e30f;
    for (int i = 0; i < ad_enemy_n; i++)
    {
        float ex = ad_enemy_x[i] - px, ey = ad_enemy_y[i] - py;
        float d2 = ex * ex + ey * ey;
        if (d2 < best)
        {
            best = d2;
            near = i;
        }
    }
    if (near >= 0)
    {
        float a = (float)ad_enemy_aim[near] * 0.0174532925f;
        st->ux = cosf(a);
        st->uy = sinf(a);
        st->vel = true;
    }
}

void collect_chars(int32_t own_gid, float mx, float my)
{
    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i]) continue;
        const uint8_t* obj = obj_block[i];
        uintptr_t dataPtr = 0;
        memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));
        if (!dataPtr || !char_vt || !pointer_plausible(dataPtr)) continue;
        uintptr_t vt = 0;
        if (!read_ptr(dataPtr, &vt) || vt < base || vt - base != char_vt)
            continue;
        ad_char_n++;
        int32_t team, hp, gid, xi, yi, aim;
        memcpy(&team, obj + OBJ_TEAM_OFF, sizeof(team));
        memcpy(&hp, obj + OBJ_HP_OFF, sizeof(hp));
        memcpy(&gid, obj + OBJ_GLOBALID_OFF, sizeof(gid));
        memcpy(&xi, obj + OBJ_X_OFF, sizeof(xi));
        memcpy(&yi, obj + OBJ_Y_OFF, sizeof(yi));
        memcpy(&aim, obj + OBJ_ATTACK_ANGLE_OFF, sizeof(aim));
        if (team == own_team_b || hp <= 0 || gid <= 0 || gid == own_gid)
            continue;
        if (ad_enemy_n >= AD_ENEMY_MAX) continue;
        int k = ad_enemy_n++;
        float ex = (float)xi - mx, ey = (float)yi - my;
        ad_enemy_pid[k] = gid;
        ad_enemy_x[k] = (float)xi;
        ad_enemy_y[k] = (float)yi;
        ad_enemy_hp[k] = hp;
        ad_enemy_d[k] = sqrtf(ex * ex + ey * ey);
        ad_enemy_aim[k] = aim;
    }
}

void collect_projs(uint64_t now, float mx, float my)
{
    float zone = self_speed * 4.0f, zone2 = zone * zone;
    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i]) continue;
        const uint8_t* obj = obj_block[i];
        uintptr_t dataPtr = 0;
        memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));
        if (!dataPtr || !pointer_plausible(dataPtr)) continue;
        uintptr_t vt = 0;
        if (!read_ptr(dataPtr, &vt) || vt < base) continue;
        if (vt - base != VTABLE_PROJ_DATA_RVA) continue;
        int32_t pid, team, xi, yi, dead;
        memcpy(&pid, obj + OBJ_GLOBALID_OFF, 4);
        memcpy(&team, obj + OBJ_TEAM_OFF, 4);
        memcpy(&xi, obj + OBJ_X_OFF, 4);
        memcpy(&yi, obj + OBJ_Y_OFF, 4);
        memcpy(&dead, obj + OBJ_DEADFLAG_OFF, 4);
        if (!pid || team == own_team_b || dead) continue;
        ad_proj_n++;
        float px = (float)xi, py = (float)yi;
        Proj* st = find_proj(pid);
        if (st->pid != pid)
        {
            init_proj(st, pid, dataPtr, px, py, now);
            if (st->pid != pid) continue;
        }
        st->seen = now;
        float step_x = px - st->x, step_y = py - st->y;
        float step = sqrtf(step_x * step_x + step_y * step_y);
        if (step > 1.0f)
        {
            st->ux = step_x / step;
            st->uy = step_y / step;
            st->vel = true;
        }
        st->x = px, st->y = py;
        px += st->ux * st->speed * TICK_MS_SEC;
        py += st->uy * st->speed * TICK_MS_SEC;
        float dx = px - mx, dy = py - my;
        if (!st->vel) continue;
        float vx = st->ux * st->speed, vy = st->uy * st->speed;
        if (dx * dx + dy * dy > zone2) continue;
        if (st->flags & B_BEAM)
        {
            ad_beam_n++;
            continue;
        }
        if (st->flags & B_IND) ad_ind_n++;
        if (!(st->flags & (B_IND | B_PIERCE)) &&
            !wall_los(px, py, mx, my, WALL_BLOCKS_PROJECTILES))
            continue;
        float dot = vx * dx + vy * dy;
        if (dot >= 0.0f) continue;
        float rad = st->rad + OWN_HITBOX;
        float vv = vx * vx + vy * vy, tt = -dot / vv;
        float qx = dx + vx * tt, qy = dy + vy * tt,
              dmin = sqrtf(qx * qx + qy * qy);
        if (dmin > rad) continue;
        if (threat_n >= THREAT_MAX) continue;
        float gap = fmaxf(rad * rad - dmin * dmin, 0.0f);
        Threat& t = threats[threat_n++];
        t.x = px, t.y = py, t.vx = vx, t.vy = vy, t.rad = rad, t.pid = pid;
        t.tt = tt - sqrtf(gap) / st->speed;
        memcpy(t.name, st->name, sizeof(t.name));
        t.name[sizeof(t.name) - 1] = '\0';
    }
}

void evict_stale(uint64_t now)
{
    for (int i = 0; i < PROJ_SLOTS; i++)
        if (projs[i].pid && now - projs[i].seen > PROJ_TTL_MS) projs[i].pid = 0;
}
}

using namespace ad;
namespace
{
static float commit_x = 0.0f, commit_y = 0.0f;
static uint64_t sent_ms = 0;
static uint32_t last_hp = 0;
static float dir_threat(float mx, float my, float dx, float dy)
{
    float worst = 1e30f;
    for (int i = 0; i < threat_n; i++)
    {
        float c = clear_one(mx, my, dx, dy, threats[i]);
        if (c < worst) worst = c;
    }
    return worst;
}
static float dir_value(float mx, float my, float dx, float dy)
{
    return dir_threat(mx, my, dx, dy) -
           OWN_HITBOX * wall_pull(mx, my, dx, dy, OWN_HITBOX);
}
static void choose(float mx, float my, float& out_x, float& out_y)
{
    bool has = commit_x != 0.0f || commit_y != 0.0f;
    float bx = has ? commit_x : 1.0f;
    float by = has ? commit_y : 0.0f;
    float best = -1e30f;
    for (int i = 0; i < DIRS; i++)
    {
        float a = 6.2831853f * (float)i / (float)DIRS;
        float dx = cosf(a), dy = sinf(a);
        float v = dir_value(mx, my, dx, dy);
        if (has) v += OWN_HITBOX * 0.25f * (dx * commit_x + dy * commit_y);
        if (v > best)
        {
            best = v, bx = dx, by = dy;
        }
    }
    ad_best_dir = ((int)lroundf(atan2f(by, bx) * 1.2732395f) + 8) & 7;
    commit_x = bx, commit_y = by;
    out_x = bx, out_y = by;
}
}
static void hold_move()
{
    if (!coord_ok || wall_cache_w() <= 0 || wall_cache_h() <= 0)
    {
        return;
    }
    int32_t face = 0;
    read_int(own_elem + OBJ_ATTACK_ANGLE_OFF, &face);
    const float r = (float)face * 0.017453292f;
    enqueue(axis((float)own_x + cosf(r) * HOLD_EPS, wall_cache_w()),
            axis((float)own_y + sinf(r) * HOLD_EPS, wall_cache_h()));
}

static void end_dodge()
{
    if (active)
    {
        hold_move();
    }
    ad_target_x = INT32_MAX, ad_target_y = INT32_MAX;
    active = 0;
}

static void learn_vtable()
{
    void* data = nullptr;
    void* vt = nullptr;
    if (char_vt || !base || !own_elem) return;
    if (!read_ptr(own_elem + ELEM_DEF_OFF, &data) || !data ||
        !pointer_plausible((uintptr_t)data))
        return;
    if (!read_ptr((uintptr_t)data, &vt) || !vt) return;
    uintptr_t rva = (uintptr_t)vt - base;
    if (rva == VTABLE_PROJ_DATA_RVA || rva < DC_RVA_LO ||
        rva >= DC_RVA_LO + DC_RVA_SIZE)
        return;
    char_vt = rva;
}
void autododge_reset()
{
    ad_best_dir = -1;
    ad_char_n = 0;
    ad_proj_n = 0;
    ad_enemy_n = 0;
    ad_threat_n = 0;
    ad_beam_n = 0;
    ad_ind_n = 0;
    threat_n = 0;
    char_vt = 0;
    last_hp = 0;
    pend_dmg = 0;
    sent_ms = 0;
    commit_x = 0.0f, commit_y = 0.0f;
    memset(projs, 0, sizeof(projs));
    end_dodge();
}
void autododge()
{
    ad_best_dir = -1;
    ad_char_n = ad_proj_n = ad_enemy_n = ad_threat_n = 0;
    ad_beam_n = ad_ind_n = 0;
    threat_n = 0;
    if (own_hp < last_hp)
    {
        pend_dmg = (int)(last_hp - own_hp);
    }
    last_hp = own_hp;
    if (!base || !own_elem || dead || !coord_ok)
    {
        end_dodge();
        return;
    }
    learn_vtable();
    if (!char_vt || wall_cache_w() <= 0 || wall_cache_h() <= 0)
    {
        end_dodge();
        return;
    }
    int32_t own_gid = 0;
    read_int(own_elem + OBJ_GLOBALID_OFF, &own_gid);
    self_speed = own_speed > 1.0f ? own_speed : 1.0f;
    uint64_t now = us() / 1000;
    float mx = (float)own_x, my = (float)own_y;
    evict_stale(now);
    collect_chars(own_gid, mx, my);
    collect_projs(now, mx, my);
    if (pend_dmg) hit_report(mx, my, commit_x, commit_y);
    if (!threat_n)
    {
        end_dodge();
        return;
    }
    float dir_x = 1.0f, dir_y = 0.0f;
    choose(mx, my, dir_x, dir_y);
    fill_report(mx, my, dir_x, dir_y);
    int32_t tx = axis(mx + dir_x * REACH, wall_cache_w());
    int32_t ty = axis(my + dir_y * REACH, wall_cache_h());
    if (ad_target_x != tx || ad_target_y != ty || now - sent_ms >= TICK_MS)
    {
        sent_ms = now;
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    active = 1;
}
namespace ad
{
int nearest_unused(
    float mx, float my, float dx, float dy, const int* used, float* out_clear)
{
    int best = -1;
    float bc = 1e30f;
    for (int i = 0; i < threat_n; i++)
    {
        bool skip = false;
        for (int k = 0; k < AD_REPORT_MAX; k++)
            if (used[k] == i) skip = true;
        if (skip) continue;
        float c = clear_one(mx, my, dx, dy, threats[i]);
        if (c < bc)
        {
            bc = c;
            best = i;
        }
    }
    *out_clear = bc;
    return best;
}
void hit_report(float mx, float my, float dx, float dy)
{
    ad_hit_dmg = pend_dmg;
    ad_hit_act = active;
    ad_hit_n = threat_n;
    ad_hit_x = (int)mx;
    ad_hit_y = (int)my;
    ad_hit_picked = 0;
    int used[AD_REPORT_MAX] = {-1, -1, -1};
    for (int s = 0; s < AD_REPORT_MAX; s++)
    {
        float bc = 0.0f;
        int best = nearest_unused(mx, my, dx, dy, used, &bc);
        if (best < 0) break;
        used[s] = best;
        ad_hit_ca[s] = bc;
        ad_hit_ttc[s] = threats[best].tt;
        ad_hit_r[s] = threats[best].rad;
        ad_hit_picked++;
    }
    pend_dmg = 0;
}
void fill_report(float mx, float my, float dx, float dy)
{
    ad_threat_n = threat_n < AD_REPORT_MAX ? threat_n : AD_REPORT_MAX;
    int used[AD_REPORT_MAX] = {-1, -1, -1};
    for (int s = 0; s < ad_threat_n; s++)
    {
        float bc = 0.0f;
        int best = nearest_unused(mx, my, dx, dy, used, &bc);
        if (best < 0)
        {
            ad_threat_n = s;
            break;
        }
        used[s] = best;
        const Threat& t = threats[best];
        ad_threat_pid[s] = t.pid;
        ad_threat_x[s] = t.x;
        ad_threat_y[s] = t.y;
        ad_threat_vx[s] = t.vx;
        ad_threat_vy[s] = t.vy;
        ad_threat_r[s] = t.rad;
        ad_threat_ca[s] = bc;
        ad_threat_ttc[s] = t.tt;
        ad_threat_d[s] =
            sqrtf((t.x - mx) * (t.x - mx) + (t.y - my) * (t.y - my));
        memcpy(ad_threat_name[s], t.name, sizeof(ad_threat_name[s]));
        ad_threat_name[s][sizeof(ad_threat_name[s]) - 1] = '\0';
    }
}
}
