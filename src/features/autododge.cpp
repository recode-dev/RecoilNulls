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
int ad_human = 0;
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
constexpr float REACH = OWN_HITBOX * 2.5f;
constexpr int DIRS = 16;
constexpr int SIM_STEPS = 32;
constexpr uint64_t CALM_MS = 145;
constexpr uint64_t SEND_MS = 100;
constexpr uint64_t TURN_HOLD_MS = 150;
constexpr uint64_t HUMAN_MS = 200;
constexpr float WEIGHT_STEP = 4096.0f;
constexpr float WEIGHT_POS = 900.0f;
constexpr float TICK_MS_SEC = (float)ad::TICK_MS * 0.001f;
constexpr float SIM_T = (float)SIM_STEPS * TICK_MS_SEC;
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
    if (vv > 1e-6f)
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

static void add_char(const uint8_t* obj, int32_t own_gid, float mx, float my)
{
    ad_char_n++;
    int32_t team, hp, gid, xi, yi, aim;
    memcpy(&team, obj + OBJ_TEAM_OFF, sizeof(team));
    memcpy(&hp, obj + OBJ_HP_OFF, sizeof(hp));
    memcpy(&gid, obj + OBJ_GLOBALID_OFF, sizeof(gid));
    memcpy(&xi, obj + OBJ_X_OFF, sizeof(xi));
    memcpy(&yi, obj + OBJ_Y_OFF, sizeof(yi));
    memcpy(&aim, obj + OBJ_ATTACK_ANGLE_OFF, sizeof(aim));
    if (team == own_team_b || hp <= 0 || gid <= 0 || gid == own_gid) return;
    if (ad_enemy_n >= AD_ENEMY_MAX) return;
    int k = ad_enemy_n++;
    float ex = (float)xi - mx, ey = (float)yi - my;
    ad_enemy_pid[k] = gid;
    ad_enemy_x[k] = (float)xi;
    ad_enemy_y[k] = (float)yi;
    ad_enemy_hp[k] = hp;
    ad_enemy_d[k] = sqrtf(ex * ex + ey * ey);
    ad_enemy_aim[k] = aim;
}

static void add_proj(const uint8_t* obj,
                     uintptr_t dataPtr,
                     uint64_t now,
                     float mx,
                     float my,
                     float zone2)
{
    int32_t pid, team, xi, yi, dead;
    memcpy(&pid, obj + OBJ_GLOBALID_OFF, sizeof(pid));
    memcpy(&team, obj + OBJ_TEAM_OFF, sizeof(team));
    memcpy(&xi, obj + OBJ_X_OFF, sizeof(xi));
    memcpy(&yi, obj + OBJ_Y_OFF, sizeof(yi));
    memcpy(&dead, obj + OBJ_DEADFLAG_OFF, sizeof(dead));
    if (!pid || team == own_team_b || dead) return;
    ad_proj_n++;
    float px = (float)xi, py = (float)yi;
    Proj* st = find_proj(pid);
    if (st->pid != pid)
    {
        init_proj(st, pid, dataPtr, px, py, now);
        if (st->pid != pid) return;
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
    float dx = px - mx, dy = py - my;
    if (!st->vel) return;
    float vx = st->ux * st->speed, vy = st->uy * st->speed;
    if (dx * dx + dy * dy > zone2) return;
    if (st->flags & B_BEAM)
    {
        ad_beam_n++;
        return;
    }
    if (st->flags & B_IND) ad_ind_n++;
    if (!(st->flags & (B_IND | B_PIERCE)) &&
        !wall_los(px, py, mx, my, WALL_BLOCKS_PROJECTILES))
        return;
    float vv = vx * vx + vy * vy;
    if (vv <= 0.0f) return;
    float rad = st->rad + OWN_HITBOX;
    float tt = -(dx * vx + dy * vy) / vv;
    if (tt < 0.0f && dx * dx + dy * dy > rad * rad) return;
    for (int k = 0; k < threat_n; k++)
    {
        if (fabsf(threats[k].x - px) < 1.0f &&
            fabsf(threats[k].y - py) < 1.0f &&
            fabsf(threats[k].vx - vx) < 1.0f &&
            fabsf(threats[k].vy - vy) < 1.0f)
            return;
    }
    if (threat_n >= THREAT_MAX) return;
    Threat& t = threats[threat_n++];
    t.x = px, t.y = py, t.vx = vx, t.vy = vy, t.pid = pid;
    t.rad = rad;
    t.tt = tt;
    memcpy(t.name, st->name, sizeof(t.name));
    t.name[sizeof(t.name) - 1] = '\0';
}

static uintptr_t obj_data[COUNT_MAX];
static uintptr_t obj_rva[COUNT_MAX];

void collect_world(uint64_t now, int32_t own_gid, float mx, float my)
{
    float zone = self_speed * 6.0f, zone2 = zone * zone;
    for (int i = 0; i < obj_count; i++)
    {
        obj_data[i] = 0;
        obj_rva[i] = 0;
        if (!obj_valid[i]) continue;
        const uint8_t* obj = obj_block[i];
        uintptr_t dataPtr = 0;
        memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));
        if (!dataPtr || !pointer_plausible(dataPtr)) continue;
        uintptr_t vt = 0;
        if (!read_ptr(dataPtr, &vt) || vt < base) continue;
        obj_data[i] = dataPtr;
        obj_rva[i] = vt - base;
        if (char_vt && obj_rva[i] == char_vt) add_char(obj, own_gid, mx, my);
    }
    for (int i = 0; i < obj_count; i++)
    {
        if (obj_rva[i] == VTABLE_PROJ_DATA_RVA)
            add_proj(obj_block[i], obj_data[i], now, mx, my, zone2);
    }
}

float static_clear(const Threat& t, float mx, float my)
{
    float rx = t.x - mx, ry = t.y - my;
    float vv = t.vx * t.vx + t.vy * t.vy;
    float tt = 0.0f;
    if (vv > 1e-6f)
    {
        tt = -(rx * t.vx + ry * t.vy) / vv;
        if (tt < 0.0f) tt = 0.0f;
    }
    float ex = rx + t.vx * tt, ey = ry + t.vy * tt;
    return sqrtf(ex * ex + ey * ey) - t.rad;
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
static uint64_t calm_ms = 0;
static uint64_t turn_ms = 0;
static float sent_x = 1.0f, sent_y = 0.0f;
static uint64_t human_ms = 0;
static int32_t prev_x = 0, prev_y = 0;
static uint32_t last_hp = 0;
static float free_run(float mx, float my, float dx, float dy, float far)
{
    float run = far;
    float w = (float)wall_cache_w() * WALL_TILE_SIZE;
    float h = (float)wall_cache_h() * WALL_TILE_SIZE;
    if (dx > 1e-6f)
        run = fminf(run, (w - OWN_HITBOX - mx) / dx);
    else if (dx < -1e-6f)
        run = fminf(run, (OWN_HITBOX - mx) / dx);
    if (dy > 1e-6f)
        run = fminf(run, (h - OWN_HITBOX - my) / dy);
    else if (dy < -1e-6f)
        run = fminf(run, (OWN_HITBOX - my) / dy);
    for (float d = WALL_TILE_SIZE; d < run; d += WALL_TILE_SIZE)
    {
        if (wall_is_blocked_wide(
                mx + dx * d, my + dy * d, OWN_HITBOX, WALL_BLOCKS_MOVEMENT))
        {
            run = d;
            break;
        }
    }
    return run > 0.0f ? run : 0.0f;
}
static void sim_pos(float mx,
                    float my,
                    float dx,
                    float dy,
                    float tk,
                    float w,
                    float h,
                    float& ax,
                    float& ay)
{
    ax = mx + dx * self_speed * tk;
    ay = my + dy * self_speed * tk;
    if (ax < OWN_HITBOX)
        ax = OWN_HITBOX;
    else if (ax > w)
        ax = w;
    if (ay < OWN_HITBOX)
        ay = OWN_HITBOX;
    else if (ay > h)
        ay = h;
}
static float nearest_away(float mx, float my, float dx, float dy)
{
    float best = 1e30f, dot = 0.0f;
    for (int i = 0; i < ad_enemy_n; i++)
    {
        float ex = ad_enemy_x[i] - mx, ey = ad_enemy_y[i] - my;
        float d = sqrtf(ex * ex + ey * ey);
        if (d < 1.0f || d >= best) continue;
        best = d;
        dot = (dx * ex + dy * ey) / d;
    }
    return best < 1e29f ? -dot : 0.0f;
}
static float dir_value(float mx, float my, float dx, float dy, int& steps)
{
    float far = self_speed * SIM_T;
    float run = free_run(mx, my, dx, dy, far);
    float room = run >= far ? 1.0f : run / far;
    float w = (float)wall_cache_w() * WALL_TILE_SIZE - OWN_HITBOX;
    float h = (float)wall_cache_h() * WALL_TILE_SIZE - OWN_HITBOX;
    float mx_step = dx * self_speed, my_step = dy * self_speed;
    int survive = SIM_STEPS + 1;
    float margin = 1e30f;
    for (int i = 0; i < threat_n; i++)
    {
        const Threat& t = threats[i];
        float r2 = t.rad * t.rad;
        int hit = 0;
        for (int k = 1; k <= SIM_STEPS && !hit; k++)
        {
            float tk = (float)k * TICK_MS_SEC;
            float ax = 0.0f, ay = 0.0f;
            sim_pos(mx, my, dx, dy, tk, w, h, ax, ay);
            float ex = t.x + t.vx * tk - ax;
            float ey = t.y + t.vy * tk - ay;
            if (ex * ex + ey * ey < r2) hit = k;
        }
        if (hit)
        {
            if (hit < survive) survive = hit;
            continue;
        }
        float rx = t.x - mx, ry = t.y - my;
        float sx = t.vx - mx_step, sy = t.vy - my_step;
        float vv = sx * sx + sy * sy;
        float tt = 0.0f;
        if (vv > 1e-6f)
        {
            tt = -(rx * sx + ry * sy) / vv;
            if (tt < 0.0f) tt = 0.0f;
            if (tt > SIM_T) tt = SIM_T;
        }
        float ax = 0.0f, ay = 0.0f;
        sim_pos(mx, my, dx, dy, tt, w, h, ax, ay);
        float ex = t.x + t.vx * tt - ax;
        float ey = t.y + t.vy * tt - ay;
        float c = sqrtf(ex * ex + ey * ey) - t.rad;
        if (c < margin) margin = c;
    }
    if (margin > 1e29f) margin = 0.0f;
    if (margin > OWN_HITBOX * 3.0f) margin = OWN_HITBOX * 3.0f;
    steps = survive;
    return (float)survive * WEIGHT_STEP + margin + WEIGHT_POS * room +
           WEIGHT_POS * 0.28f * nearest_away(mx, my, dx, dy) -
           OWN_HITBOX * wall_pull(mx, my, dx, dy, OWN_HITBOX);
}
static void choose(float mx, float my, float& out_x, float& out_y)
{
    bool has = commit_x != 0.0f || commit_y != 0.0f;
    float val[DIRS];
    int best_i = 0, best_steps = 0;
    float best = -1e30f;
    for (int i = 0; i < DIRS; i++)
    {
        float a = 6.2831853f * (float)i / (float)DIRS;
        float dx = cosf(a), dy = sinf(a);
        int steps = 0;
        float v = dir_value(mx, my, dx, dy, steps);
        if (has) v += OWN_HITBOX * (dx * commit_x + dy * commit_y);
        val[i] = v;
        if (v > best)
        {
            best = v, best_i = i, best_steps = steps;
        }
    }
    float vm = val[(best_i + DIRS - 1) % DIRS];
    float vp = val[(best_i + 1) % DIRS];
    float den = vm - 2.0f * best + vp;
    float off = 0.0f;
    if (den < -1e-6f) off = 0.5f * (vm - vp) / den;
    if (off > 0.5f) off = 0.5f;
    if (off < -0.5f) off = -0.5f;
    float a = 6.2831853f * ((float)best_i + off) / (float)DIRS;
    float bx = cosf(a), by = sinf(a);
    if (has)
    {
        int cur = 0;
        dir_value(mx, my, commit_x, commit_y, cur);
        uint64_t now = us() / 1000;
        bool urgent = (cur > 0 && cur <= 1) ||
                      free_run(mx, my, commit_x, commit_y, self_speed * SIM_T) <
                          OWN_HITBOX * 2.0f;
        bool gain = best_steps >= cur + 2;
        if (!urgent && (now - turn_ms < TURN_HOLD_MS || !gain))
        {
            bx = commit_x, by = commit_y;
        }
        else
        {
            turn_ms = now;
        }
    }
    ad_best_dir = ((int)lroundf(atan2f(by, bx) * 1.2732395f) + 8) & 7;
    commit_x = bx, commit_y = by;
    out_x = bx, out_y = by;
}
}

static void end_dodge()
{
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
    calm_ms = 0;
    turn_ms = 0;
    human_ms = 0;
    ad_human = 0;
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
    collect_world(now, own_gid, mx, my);
    if (pend_dmg) hit_report(mx, my, commit_x, commit_y);
    {
        float mvx = (float)(own_x - prev_x), mvy = (float)(own_y - prev_y);
        float mv = sqrtf(mvx * mvx + mvy * mvy);
        prev_x = own_x, prev_y = own_y;
        if (active && mv > 15.0f && (mvx * sent_x + mvy * sent_y) < 0.7f * mv)
            human_ms = now;
    }
    ad_human = now - human_ms < HUMAN_MS ? 1 : 0;
    bool hot = false;
    for (int i = 0; i < threat_n; i++)
    {
        if (ad::static_clear(threats[i], mx, my) < ad::OWN_HITBOX * 2.0f)
        {
            hot = true;
            break;
        }
    }
    if (!hot) threat_n = 0;
    if (threat_n)
    {
        calm_ms = now;
    }
    else if (!active || now - calm_ms >= CALM_MS)
    {
        end_dodge();
        return;
    }
    float dir_x = 1.0f, dir_y = 0.0f;
    if (commit_x != 0.0f || commit_y != 0.0f)
    {
        dir_x = commit_x, dir_y = commit_y;
    }
    if (threat_n)
    {
        choose(mx, my, dir_x, dir_y);
    }
    fill_report(mx, my, dir_x, dir_y);
    int32_t tx = axis(mx + dir_x * REACH, wall_cache_w());
    int32_t ty = axis(my + dir_y * REACH, wall_cache_h());
    float rem_x = (float)(ad_target_x - own_x);
    float rem_y = (float)(ad_target_y - own_y);
    bool spent =
        ad_target_x == INT32_MAX || rem_x * rem_x + rem_y * rem_y < REACH * REACH * 0.25f;
    bool turned = dir_x * sent_x + dir_y * sent_y < 0.94f;
    if (!ad_human && (spent || turned) && now - sent_ms >= SEND_MS)
    {
        sent_ms = now;
        sent_x = dir_x, sent_y = dir_y;
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
