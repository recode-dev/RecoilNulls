#include "autododge.h"
#include "adcore.h"
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
float ad_pad = 0.0f, ad_pad_excess = 0.0f;
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
constexpr int DIRS = 16;
constexpr int INPUT_TICKS = 3;
constexpr float TURN_PER_TICK = 1.5707963f;
constexpr int WALL_PROBES = 3;
}
static float pad = 0.0f;
namespace ad
{
float body_rad()
{
    return OWN_HITBOX + pad;
}
Threat threats[THREAT_MAX];
int threat_n = 0;
Proj projs[PROJ_SLOTS];
uintptr_t char_vt = 0;
float self_speed = 1.0f;
}
using namespace ad;
namespace
{
static float commit_x = 0.0f, commit_y = 0.0f;
static uint64_t sent_ms = 0;
static uint32_t last_hp = 0;
static int pend_dmg = 0;
static uint64_t last_turn_ms = 0;
static int32_t axis(float v, int tiles)
{
    float limit = (float)tiles * WALL_TILE_SIZE - 1.0f;
    if (v < 0.0f) return 0;
    if (v > limit) return (int32_t)limit;
    return (int32_t)roundf(v);
}
static void release_move()
{
    if (!coord_ok || wall_cache_w() <= 0 || wall_cache_h() <= 0)
    {
        return;
    }
    float step = self_speed * (float)INPUT_TICKS * (float)TICK_MS * 0.001f;
    enqueue(axis((float)own_x + commit_x * step, wall_cache_w()),
            axis((float)own_y + commit_y * step, wall_cache_h()));
}
static void end_dodge()
{
    if (active) release_move();
    commit_x = 0.0f, commit_y = 0.0f;
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
static float wall_pull(float mx, float my, float dx, float dy)
{
    int blocked = 0;
    for (int i = 1; i <= WALL_PROBES; i++)
    {
        float d = REACH * 0.5f * (float)i / (float)WALL_PROBES;
        blocked +=
            wall_is_blocked_wide(
                mx + dx * d, my + dy * d, OWN_HITBOX, WALL_BLOCKS_MOVEMENT)
                ? 1
                : 0;
    }
    float pin_x = (float)axis(mx + dx * REACH, wall_cache_w()) - mx;
    float pin_y = (float)axis(my + dy * REACH, wall_cache_h()) - my;
    float room = sqrtf(pin_x * pin_x + pin_y * pin_y) / REACH;
    return (float)blocked / (float)WALL_PROBES + (1.0f - room);
}
static float clear_one(float mx, float my, float dx, float dy, const Threat& t)
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
    return dir_threat(mx, my, dx, dy) - OWN_HITBOX * wall_pull(mx, my, dx, dy);
}
static int nearest_unused(
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
static void learn_pad(uint64_t now)
{
    const Proj* near = nullptr;
    for (int i = 0; i < PROJ_SLOTS; i++)
    {
        const Proj* st = &projs[i];
        if (!st->pid) continue;
        if (st->flags & (B_BEAM | B_IND)) continue;
        if (now - st->seen > 400) continue;
        if (st->min_dmin > 400.0f) continue;
        if (near == nullptr || st->min_dmin < near->min_dmin) near = st;
    }
    if (near == nullptr)
    {
        ad_pad_excess = 0.0f;
        return;
    }
    float excess = near->min_dmin - (near->rad + body_rad());
    ad_pad_excess = excess;
    if (excess <= 0.0f || excess > 60.0f) return;
    pad += excess;
    if (pad > 90.0f) pad = 90.0f;
}
static void hit_report(float mx, float my, float dx, float dy)
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
static void fill_report(float mx, float my, float dx, float dy)
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
static void choose(float mx, float my, float& out_x, float& out_y)
{
    bool has = commit_x != 0.0f || commit_y != 0.0f;
    float bx = has ? commit_x : 1.0f;
    float by = has ? commit_y : 0.0f;
    float best = -1e30f, best_threat = -1e30f;
    for (int pass = 0; pass < 2; pass++)
    {
        for (int i = 0; i < DIRS; i++)
        {
            float a = 6.2831853f * (float)i / (float)DIRS;
            float dx = cosf(a), dy = sinf(a);
            if (!pass && has && dx * commit_x + dy * commit_y < 0.0f) continue;
            float th = dir_threat(mx, my, dx, dy);
            float v = th - OWN_HITBOX * wall_pull(mx, my, dx, dy);
            if (v > best)
            {
                best = v;
                bx = dx;
                by = dy;
            }
            if (!pass && th > best_threat) best_threat = th;
        }
        if (pass == 0 && (!has || best_threat >= 0.0f)) break;
    }
    if (has)
    {
        float keep = dir_value(mx, my, commit_x, commit_y);
        if (keep + body_rad() * 0.5f >= best)
        {
            bx = commit_x;
            by = commit_y;
        }
        uint64_t now = us() / 1000;
        float dt = last_turn_ms ? (float)(now - last_turn_ms) : (float)TICK_MS;
        last_turn_ms = now;
        if (dt > (float)TICK_MS * 4.0f) dt = (float)TICK_MS * 4.0f;
        float lim = TURN_PER_TICK * dt / (float)TICK_MS;
        float pa = atan2f(commit_y, commit_x);
        float d = atan2f(by, bx) - pa;
        if (d > 3.14159265f) d -= 6.2831853f;
        if (d < -3.14159265f) d += 6.2831853f;
        if (d > lim) d = lim;
        if (d < -lim) d = -lim;
        if (d != 0.0f)
        {
            bx = cosf(pa + d);
            by = sinf(pa + d);
        }
    }
    ad_best_dir = ((int)lroundf(atan2f(by, bx) * 1.2732395f) + 8) & 7;
    commit_x = bx;
    commit_y = by;
    out_x = bx, out_y = by;
}
}
void autododge_reset()
{
    ad_best_dir = -1;
    ad_pad = pad;
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
    pad *= 0.75f;
    memset(projs, 0, sizeof(projs));
    end_dodge();
}
void autododge()
{
    ad_best_dir = -1;
    ad_pad = pad;
    ad_char_n = ad_proj_n = ad_enemy_n = ad_threat_n = 0;
    ad_beam_n = ad_ind_n = 0;
    threat_n = 0;
    if (own_hp < last_hp)
    {
        pend_dmg = (int)(last_hp - own_hp);
        learn_pad(us() / 1000);
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
