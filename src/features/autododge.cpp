#include "autododge.h"
#include "../core/commands.h"
#include "../core/gamemath.h"
#include "../core/scan.h"
#include "../core/scstring.h"
#include "../utils/walls.h"
#include <cmath>
#include <cstdint>
#include <cstring>
int      active = 0, in_move = 0;
int      ad_human = 0;
float    ad_dir_x = 1.0f, ad_dir_y = 0.0f;
int      ad_hold_ms = 0, ad_urgent = 0;
int      ad_ind_n       = 0;
int      ad_pass_own    = 0;
uint32_t ad_proj_sample = 0, ad_proj_move = 0;
int32_t  ad_target_x = INT32_MAX, ad_target_y = INT32_MAX;
int      ad_best_dir = -1;
int      ad_char_n = 0, ad_proj_n = 0, ad_enemy_n = 0, ad_threat_n = 0;
int      ad_threat_pid[AD_REPORT_MAX] = {0};
float    ad_threat_x[AD_REPORT_MAX] = {0}, ad_threat_y[AD_REPORT_MAX] = {0};
float    ad_threat_vx[AD_REPORT_MAX] = {0}, ad_threat_vy[AD_REPORT_MAX] = {0};
float    ad_threat_r[AD_REPORT_MAX] = {0}, ad_threat_d[AD_REPORT_MAX] = {0};
float    ad_threat_ttc[AD_REPORT_MAX] = {0}, ad_threat_ca[AD_REPORT_MAX] = {0};
char     ad_threat_name[AD_REPORT_MAX][40] = {{0}};
int32_t  ad_pj_pid[AD_REPORT_MAX]          = {0};
float    ad_pj_x[AD_REPORT_MAX] = {0}, ad_pj_y[AD_REPORT_MAX] = {0};
float    ad_pj_vx[AD_REPORT_MAX] = {0}, ad_pj_vy[AD_REPORT_MAX] = {0};
float    ad_pj_sp[AD_REPORT_MAX] = {0}, ad_pj_r[AD_REPORT_MAX] = {0};
int      ad_pj_vel[AD_REPORT_MAX]           = {0};
int      ad_windup_n                        = 0;
int      ad_enemy_charge_ms[AD_ENEMY_MAX]   = {0};
int      ad_enemy_charge_mode[AD_ENEMY_MAX] = {0};
int   ad_hit_dmg = 0, ad_hit_act = 0, ad_hit_n = 0, ad_hit_x = 0, ad_hit_y = 0;
int   ad_hit_picked            = 0;
float ad_hit_ca[AD_REPORT_MAX] = {0}, ad_hit_ttc[AD_REPORT_MAX] = {0};
float ad_hit_r[AD_REPORT_MAX]    = {0};
int   ad_enemy_pid[AD_ENEMY_MAX] = {0};
float ad_enemy_x[AD_ENEMY_MAX] = {0}, ad_enemy_y[AD_ENEMY_MAX] = {0};
int   ad_enemy_hp[AD_ENEMY_MAX]  = {0};
float ad_enemy_d[AD_ENEMY_MAX]   = {0};
int   ad_enemy_aim[AD_ENEMY_MAX] = {0};
namespace
{
constexpr float    REACH             = 300.0f;
constexpr int      DIRS              = 16;
constexpr int      SIM_STEPS         = 32;
constexpr float    ZONE_FAR          = 5000.0f;
constexpr uint64_t CALM_MS           = 0;
constexpr uint64_t SEND_MS           = ad::TICK_MS;
constexpr int      MOVE_DELAY_TICKS  = 2;
constexpr uint64_t TURN_HOLD_MS      = 0;
constexpr int      HOLD_DANGER_STEPS = (int) (TURN_HOLD_MS * 2 / ad::TICK_MS);
constexpr float    AD_SAFETY         = 40.0f;
constexpr float    PROJECTILE_RADIUS = 60.0f;
constexpr uint64_t HUMAN_MS          = 200;
constexpr float    WEIGHT_STEP       = 4096.0f;
constexpr float    WEIGHT_POS        = 900.0f;
constexpr float    WEIGHT_AWAY       = 0.06f;
constexpr float    WEIGHT_AXIS       = 2600.0f;
constexpr float    AXIS_INTO         = 0.35f;
constexpr float    WEIGHT_APPROACH   = 2000.0f;
constexpr float    APPROACH_R        = 2400.0f;
constexpr float    THREAT_HORIZON    = 0.6f;
constexpr float    CLOSE_R           = 1200.0f;
constexpr float    COMMIT_BONUS      = 3.0f;
constexpr float    BORDER_PAD        = 60.0f;
constexpr float    EDGE_ZONE         = 300.0f;
constexpr float    WEIGHT_EDGE       = 1600.0f;
constexpr float    TICK_MS_SEC       = (float) ad::TICK_MS * 0.001f;
constexpr float    SIM_T             = (float) SIM_STEPS * TICK_MS_SEC;
}
namespace ad
{
Threat    threats[THREAT_MAX];
int       threat_n = 0;
Proj      projs[PROJ_SLOTS];
uintptr_t char_vt    = 0;
float     self_speed = 1.0f;
float     own_hitbox = 0.0f;
int       pend_dmg   = 0;
float     compute_threat_clearance(
        float mx, float my, float dx, float dy, const Threat& t)
{
    float sx  = t.vx - dx * self_speed, sy = t.vy - dy * self_speed;
    float far = SIM_T * 1.5f;
    return lm_distance_to_line(
               t.x, t.y, t.x + sx * far, t.y + sy * far, mx, my) -
           t.rad;
}
}
namespace ad
{
Proj* find_projectile_entry(int32_t pid)
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

void init_projectile_state(
    Proj* st, int32_t pid, uintptr_t dataPtr, float px, float py, uint64_t now)
{
    st->pid       = pid;
    st->seen      = now;
    st->data      = dataPtr;
    st->x         = px;
    st->y         = py;
    st->speed     = 0.0f;
    st->rad       = PROJECTILE_RADIUS;
    st->homing    = false;
    st->ttype     = 0;
    st->vel       = false;
    st->ux        = 0.0f;
    st->uy        = 0.0f;
    st->flags     = 0;
    st->name[0]   = '\0';
    uintptr_t str = 0;
    if (addr_dataname)
    {
        using name_fn = uintptr_t (*)(uintptr_t);
        str           = reinterpret_cast<name_fn>(addr_dataname)(dataPtr);
    }
    if (str)
        read_scstring(
            str, SCSTRING_DEFAULT_MAX_LENGTH, st->name, sizeof(st->name));
}

static void add_character_entry(
    const uint8_t* obj, uintptr_t objPtr, int32_t own_gid, float mx, float my)
{
    ad_char_n++;
    int32_t team, hp, gid, xi, yi, aim;
    memcpy(&team, obj + LOGICCHARACTER__TEAM, sizeof(team));
    memcpy(&hp, obj + LOGICCHARACTER__HITPOINTS, sizeof(hp));
    memcpy(&gid, obj + LOGICCHARACTER__GLOBALID, sizeof(gid));
    memcpy(&xi, obj + LOGICCHARACTER__X, sizeof(xi));
    memcpy(&yi, obj + LOGICCHARACTER__Y, sizeof(yi));
    memcpy(&aim, obj + LOGICCHARACTER__ATTACKANGLE, sizeof(aim));
    if (team == own_team_b || hp <= 0 || gid <= 0 || gid == own_gid) return;
    if (ad_enemy_n >= AD_ENEMY_MAX) return;
    int   k  = ad_enemy_n++;
    float ex = (float) xi - mx, ey = (float) yi - my;
    ad_enemy_pid[k]    = gid;
    ad_enemy_x[k]      = (float) xi;
    ad_enemy_y[k]      = (float) yi;
    ad_enemy_hp[k]     = hp;
    ad_enemy_d[k]      = lm_hypot(ex, ey);
    ad_enemy_aim[k]    = aim;
    int32_t charge     = 0;
    int32_t chargeMode = 0;
    read_int(objPtr + LOGICCHARACTER__CURRENTCHARGEUPMILLISECONDS, &charge);
    read_int(objPtr + LOGICCHARACTER__CHARGEUPMODE, &chargeMode);
    ad_enemy_charge_ms[k]   = charge;
    ad_enemy_charge_mode[k] = chargeMode;
    if (chargeMode == 1 && charge > 0 && ad_enemy_d[k] <= CLOSE_R)
        ad_windup_n++;
    else
    {
        ad_enemy_charge_ms[k]   = 0;
        ad_enemy_charge_mode[k] = 0;
    }
}

static uintptr_t find_projectile_data_asset(uintptr_t objPtr)
{
    if (!objPtr) return 0;
    for (uintptr_t off = LOGICGAMEOBJECT__DATA;
         off < LOGICGAMEOBJECT__DATASCANSPAN;
         off += sizeof(uintptr_t))
    {
        uintptr_t v = 0, vt = 0;
        if (!read_ptr(objPtr + off, &v)) break;
        if (!v || !pointer_plausible(v)) continue;
        if (!read_ptr(v, &vt) || vt < base) continue;
        if (vt - base == LOGICPROJECTILEDATA__VTABLE) return v;
    }
    return 0;
}

static float world_extent_x = 0.0f;
static float world_extent_y = 0.0f;

static void refresh_world_extents()
{
    float unit     = wall_logic_units(1);
    world_extent_x = static_cast<float>(wall_cache_width()) * unit;
    world_extent_y = static_cast<float>(wall_cache_height()) * unit;
}

static bool read_projectile_position(const uint8_t* obj, float* px, float* py)
{
    float w = world_extent_x;
    float h = world_extent_y;
    if (w <= 0.0f || h <= 0.0f) return false;
    int32_t xi = 0, yi = 0;
    memcpy(&xi, obj + LOGICCHARACTER__X, sizeof(xi));
    memcpy(&yi, obj + LOGICCHARACTER__Y, sizeof(yi));
    if ((float) xi >= -300.0f && (float) yi >= -300.0f &&
        (float) xi <= w + 300.0f && (float) yi <= h + 300.0f)
    {
        *px = (float) xi;
        *py = (float) yi;
        return true;
    }
    return false;
}

static void add_projectile_threat(const uint8_t* obj,
                                  uintptr_t      objPtr,
                                  uint64_t       now,
                                  float          mx,
                                  float          my,
                                  float          zone2,
                                  float          px,
                                  float          py)
{
    int32_t pid = 0;
    memcpy(&pid, obj + LOGICCHARACTER__GLOBALID, sizeof(pid));
    int32_t key = pid;
    if (key <= 0)
        key = (int32_t) (((objPtr >> 4) ^ (objPtr >> 20)) & 0x7fffffff);
    if (key <= 0) return;
    ad_proj_n++;
    Proj* st = find_projectile_entry(key);
    if (st->pid != key)
    {
        init_projectile_state(
            st, key, find_projectile_data_asset(objPtr), px, py, now);
        if (st->pid != key) return;
    }
    st->seen     = now;
    float step_x = px - st->x, step_y = py - st->y;
    float step = lm_hypot(step_x, step_y);
    ad_proj_sample++;
    if (step > 1.0f) ad_proj_move++;
    int32_t dataSpeed = 0;
    if (st->data &&
        read_int(st->data + LOGICPROJECTILEDATA__SPEED, &dataSpeed) &&
        dataSpeed > 50 && dataSpeed < 20000)
    {
        if (step > 0.5f)
        {
            st->ux    = step_x / step;
            st->uy    = step_y / step;
            st->speed = (float) dataSpeed;
            st->vel   = true;
        }
    }
    else
    {
        st->vel = false;
    }
    st->x = px, st->y = py;
    if (ad_proj_n <= AD_REPORT_MAX)
    {
        int k        = ad_proj_n - 1;
        ad_pj_pid[k] = key;
        ad_pj_x[k]   = px;
        ad_pj_y[k]   = py;
        ad_pj_vx[k]  = st->vel ? st->ux * st->speed : 0.0f;
        ad_pj_vy[k]  = st->vel ? st->uy * st->speed : 0.0f;
        ad_pj_sp[k]  = st->vel ? st->speed : 0.0f;
        ad_pj_r[k]   = st->rad;
        ad_pj_vel[k] = st->vel ? 1 : 0;
    }
    float dx = px - mx, dy = py - my;
    if (!st->vel) return;
    float vx = st->ux * st->speed, vy = st->uy * st->speed;
    if (dx * dx + dy * dy > zone2) return;
    if (st->flags & B_IND) ad_ind_n++;
    if (!(st->flags & (B_IND | B_PIERCE)) &&
        !wall_line_of_sight_clear(px, py, mx, my))
        return;
    float vv = vx * vx + vy * vy;
    if (vv <= 0.0f) return;
    float rad = st->rad + own_hitbox + AD_SAFETY;
    if (st->flags & B_BOUNCE) rad += own_hitbox * 0.5f;
    float   dist    = lm_hypot(dx, dy);
    int32_t arrival = game_projectile_travel_ticks(
        st->data, (int) (dist + 0.5f), (int) (st->speed + 0.5f));
    if (arrival < 1) return;
    float tt = (float) arrival * TICK_MS_SEC;
    if (tt > THREAT_HORIZON) return;
    float ttc = -(dx * vx + dy * vy) / vv;
    if (ttc < 0.0f)
    {
        if (dist > rad) return;
        ttc = 0.0f;
    }
    float miss_x = dx + vx * ttc, miss_y = dy + vy * ttc;
    if (miss_x * miss_x + miss_y * miss_y > rad * rad) return;
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
    t.x = px, t.y = py, t.vx = vx, t.vy = vy, t.pid = key;
    t.rad = rad;
    t.tt  = tt;
    memcpy(t.name, st->name, sizeof(t.name));
    t.name[sizeof(t.name) - 1] = '\0';
}

static uintptr_t obj_data[COUNT_MAX];
static uintptr_t obj_rva[COUNT_MAX];

void collect_logic_objects(uint64_t now, int32_t own_gid, float mx, float my)
{
    refresh_world_extents();
    float zone = self_speed * 6.0f;
    if (zone < ZONE_FAR) zone = ZONE_FAR;
    float zone2 = zone * zone;
    for (int i = 0; i < obj_count; i++)
    {
        obj_data[i] = 0;
        obj_rva[i]  = 0;
        if (!obj_valid[i]) continue;
        const uint8_t* obj     = obj_block[i];
        uintptr_t      dataPtr = 0;
        memcpy(&dataPtr, obj + LOGICGAMEOBJECT__DATA, sizeof(dataPtr));
        uintptr_t vt = 0;
        if (dataPtr && pointer_plausible(dataPtr) && read_ptr(dataPtr, &vt) &&
            vt >= base)
        {
            obj_data[i] = dataPtr;
            obj_rva[i]  = vt - base;
        }
        if (char_vt && obj_rva[i] == char_vt)
        {
            add_character_entry(obj, obj_snapshot[i], own_gid, mx, my);
            continue;
        }
        if (obj_rva[i] != LOGICPROJECTILEDATA__VTABLE) continue;
        float px = 0.0f, py = 0.0f;
        if (!read_projectile_position(obj, &px, &py)) continue;
        add_projectile_threat(obj, obj_snapshot[i], now, mx, my, zone2, px, py);
    }
}

int find_live_projectile(uintptr_t* objOut,
                         uintptr_t* dataOut,
                         float*     px,
                         float*     py)
{
    refresh_world_extents();
    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i] || !obj_snapshot[i]) continue;
        if (obj_rva[i] != LOGICPROJECTILEDATA__VTABLE) continue;
        float x = 0.0f, y = 0.0f;
        if (!read_projectile_position(obj_block[i], &x, &y)) continue;
        *objOut  = obj_snapshot[i];
        *dataOut = obj_data[i];
        *px      = x;
        *py      = y;
        return 1;
    }
    return 0;
}

void evict_stale_projectiles(uint64_t now)
{
    for (int i = 0; i < PROJ_SLOTS; i++)
        if (projs[i].pid && now - projs[i].seen > PROJ_TTL_MS) projs[i].pid = 0;
}
}

using namespace ad;
namespace
{
static float    commit_x = 0.0f, commit_y = 0.0f;
static uint64_t sent_ms = 0;
static uint64_t calm_ms = 0;
static uint64_t turn_ms = 0;
static float    sent_x = 1.0f, sent_y = 0.0f;
static float    move_dx = 0.0f, move_dy = 0.0f;
static uint64_t human_ms = 0;
static int32_t  prev_x = 0, prev_y = 0;
static uint32_t last_hp = 0;
static float    compute_edge_margin()
{
    return own_hitbox + BORDER_PAD;
}
static float compute_edge_pull(float mx, float my, float dx, float dy)
{
    float w    = wall_logic_units(wall_cache_width());
    float h    = wall_logic_units(wall_cache_height());
    float zone = own_hitbox + EDGE_ZONE;
    float pen  = 0.0f;
    if (mx < zone) pen -= dx * (zone - mx) / zone;
    if (mx > w - zone) pen += dx * (mx - (w - zone)) / zone;
    if (my < zone) pen -= dy * (zone - my) / zone;
    if (my > h - zone) pen += dy * (my - (h - zone)) / zone;
    return pen;
}
static float compute_free_run(float mx, float my, float dx, float dy, float far)
{
    float run = far;
    float em  = compute_edge_margin();
    float w   = wall_logic_units(wall_cache_width());
    float h   = wall_logic_units(wall_cache_height());
    if (dx > 1e-6f)
        run = fminf(run, (w - em - mx) / dx);
    else if (dx < -1e-6f)
        run = fminf(run, (em - mx) / dx);
    if (dy > 1e-6f)
        run = fminf(run, (h - em - my) / dy);
    else if (dy < -1e-6f)
        run = fminf(run, (em - my) / dy);
    const float step = wall_logic_units(1);
    for (float d = step; d < run; d += step)
    {
        if (wall_cell_blocked_wide(mx + dx * d, my + dy * d, own_hitbox))
        {
            run = d;
            break;
        }
    }
    return run > 0.0f ? run : 0.0f;
}
static void simulate_position(float  mx,
                              float  my,
                              float  dx,
                              float  dy,
                              float  tk,
                              float  w,
                              float  h,
                              float& ax,
                              float& ay)
{
    float delay = (float) MOVE_DELAY_TICKS * TICK_MS_SEC;
    float coast = tk < delay ? tk : delay;
    float em    = compute_edge_margin();
    ax          = mx + move_dx * self_speed * coast;
    ay          = my + move_dy * self_speed * coast;
    if (tk > delay)
    {
        ax += dx * self_speed * (tk - delay);
        ay += dy * self_speed * (tk - delay);
    }
    if (ax < em)
        ax = em;
    else if (ax > w)
        ax = w;
    if (ay < em)
        ay = em;
    else if (ay > h)
        ay = h;
}
static float   cost_enemy_dx[AD_ENEMY_MAX];
static float   cost_enemy_dy[AD_ENEMY_MAX];
static float   cost_enemy_dist[AD_ENEMY_MAX];
static int     cost_enemy_count = 0;
static float   cost_threat_rx[THREAT_MAX];
static float   cost_threat_ry[THREAT_MAX];
static float   cost_threat_ux[THREAT_MAX];
static float   cost_threat_uy[THREAT_MAX];
static uint8_t cost_threat_moving[THREAT_MAX];
static int     cost_threat_count = 0;

static void prepare_direction_costs(float mx, float my)
{
    cost_enemy_count = ad_enemy_n < AD_ENEMY_MAX ? ad_enemy_n : AD_ENEMY_MAX;
    for (int i = 0; i < cost_enemy_count; i++)
    {
        float ex = ad_enemy_x[i] - mx, ey = ad_enemy_y[i] - my;
        cost_enemy_dx[i]   = ex;
        cost_enemy_dy[i]   = ey;
        cost_enemy_dist[i] = lm_hypot(ex, ey);
    }
    cost_threat_count = threat_n < THREAT_MAX ? threat_n : THREAT_MAX;
    for (int i = 0; i < cost_threat_count; i++)
    {
        const Threat& t   = threats[i];
        float         vv  = t.vx * t.vx + t.vy * t.vy;
        cost_threat_rx[i] = mx - t.x;
        cost_threat_ry[i] = my - t.y;
        if (vv >= 1e-6f)
        {
            float iv              = 1.0f / lm_sqrtf(vv);
            cost_threat_ux[i]     = t.vx * iv;
            cost_threat_uy[i]     = t.vy * iv;
            cost_threat_moving[i] = 1;
        }
        else
        {
            cost_threat_ux[i]     = 0.0f;
            cost_threat_uy[i]     = 0.0f;
            cost_threat_moving[i] = 0;
        }
    }
}

static float compute_nearest_away(float mx, float my, float dx, float dy)
{
    (void) mx;
    (void) my;
    float best = 1e30f, dot = 0.0f;
    for (int i = 0; i < cost_enemy_count; i++)
    {
        float d = cost_enemy_dist[i];
        if (d < 1.0f || d >= best) continue;
        best = d;
        dot  = (dx * cost_enemy_dx[i] + dy * cost_enemy_dy[i]) / d;
    }
    return best < 1e29f ? -dot : 0.0f;
}
static float compute_enemy_approach(float mx, float my, float dx, float dy)
{
    (void) mx;
    (void) my;
    float worst = 0.0f;
    for (int i = 0; i < cost_enemy_count; i++)
    {
        float d = cost_enemy_dist[i];
        if (d < 1.0f || d > APPROACH_R) continue;
        float dot = (dx * cost_enemy_dx[i] + dy * cost_enemy_dy[i]) / d;
        if (dot <= 0.0f) continue;
        float p = dot * (1.0f - d / APPROACH_R);
        if (d < CLOSE_R && dot > 0.5f) p = 3.0f;
        if (p > worst) worst = p;
    }
    return worst;
}
static float compute_thrust_penalty(float mx, float my, float dx, float dy)
{
    (void) mx;
    (void) my;
    float worst = 0.0f;
    for (int i = 0; i < cost_threat_count; i++)
    {
        if (!cost_threat_moving[i]) continue;
        float ux = cost_threat_ux[i], uy = cost_threat_uy[i];
        float rx = cost_threat_rx[i], ry = cost_threat_ry[i];
        if (rx * ux + ry * uy < 0.0f) continue;
        float a = dx * ux + dy * uy;
        float p = a > 0.0f ? a : -AXIS_INTO * a;
        if (p > worst) worst = p;
    }
    return worst;
}
static float
evaluate_dodge_direction(float mx, float my, float dx, float dy, int& steps)
{
    float far  = self_speed * SIM_T;
    float run  = compute_free_run(mx, my, dx, dy, far);
    float room = run >= far ? 1.0f : run / far;
    float w    = wall_logic_units(wall_cache_width()) - compute_edge_margin();
    float h    = wall_logic_units(wall_cache_height()) - compute_edge_margin();
    float mx_step = dx * self_speed, my_step = dy * self_speed;
    float step_time[SIM_STEPS + 1];
    float step_x[SIM_STEPS + 1];
    float step_y[SIM_STEPS + 1];
    int   survive = SIM_STEPS + 1;
    float margin  = 1e30f;
    for (int k = 1; k <= SIM_STEPS; k++)
    {
        step_time[k] = static_cast<float>(k) * TICK_MS_SEC;
        simulate_position(
            mx, my, dx, dy, step_time[k], w, h, step_x[k], step_y[k]);
    }
    for (int i = 0; i < threat_n; i++)
    {
        const Threat& t   = threats[i];
        float         r2  = t.rad * t.rad;
        int           hit = 0;
        for (int k = 1; k <= SIM_STEPS && !hit; k++)
        {
            float tk = step_time[k];
            float ex = t.x + t.vx * tk - step_x[k];
            float ey = t.y + t.vy * tk - step_y[k];
            if (ex * ex + ey * ey < r2) hit = k;
        }
        if (hit)
        {
            if (hit < survive) survive = hit;
            continue;
        }
        float sx = t.vx - mx_step, sy = t.vy - my_step;
        float c  = lm_distance_to_line(
                      t.x, t.y, t.x + sx * SIM_T, t.y + sy * SIM_T, mx, my) -
                  t.rad;
        if (c < margin) margin = c;
    }
    if (margin > 1e29f) margin = 0.0f;
    if (margin > own_hitbox * 3.0f) margin = own_hitbox * 3.0f;
    steps = survive;
    return (float) survive * WEIGHT_STEP + margin + WEIGHT_POS * room +
           WEIGHT_POS * WEIGHT_AWAY * compute_nearest_away(mx, my, dx, dy) -
           WEIGHT_AXIS * compute_thrust_penalty(mx, my, dx, dy) -
           WEIGHT_APPROACH * compute_enemy_approach(mx, my, dx, dy) -
           WEIGHT_EDGE * compute_edge_pull(mx, my, dx, dy) -
           own_hitbox * wall_pull(mx, my, dx, dy, own_hitbox);
}
static void
choose_dodge_direction(float mx, float my, float& out_x, float& out_y)
{
    bool  has = commit_x != 0.0f || commit_y != 0.0f;
    float val[DIRS];
    prepare_direction_costs(mx, my);
    int   best_i = 0, best_steps = 0;
    float best = -1e30f;
    for (int i = 0; i < DIRS; i++)
    {
        float a  = 6.2831853f * (float) i / (float) DIRS;
        float dx = lm_cosf(a), dy = lm_sinf(a);
        int   steps = 0;
        float v     = evaluate_dodge_direction(mx, my, dx, dy, steps);
        if (has)
            v += own_hitbox * COMMIT_BONUS * (dx * commit_x + dy * commit_y);
        val[i] = v;
        if (v > best)
        {
            best = v, best_i = i, best_steps = steps;
        }
    }
    float vm  = val[(best_i + DIRS - 1) % DIRS];
    float vp  = val[(best_i + 1) % DIRS];
    float den = vm - 2.0f * best + vp;
    float off = 0.0f;
    if (den < -1e-6f) off = 0.5f * (vm - vp) / den;
    if (off > 0.5f) off = 0.5f;
    if (off < -0.5f) off = -0.5f;
    float a = 6.2831853f * ((float) best_i + off) / (float) DIRS;
    for (int k = -6; k <= 6; k++)
    {
        float ra  = a + (float) k * 0.0174533f;
        float rdx = lm_cosf(ra), rdy = lm_sinf(ra);
        int   steps = 0;
        float v     = evaluate_dodge_direction(mx, my, rdx, rdy, steps);
        if (has)
            v += own_hitbox * COMMIT_BONUS * (rdx * commit_x + rdy * commit_y);
        if (v > best)
        {
            best = v, a = ra, best_steps = steps;
        }
    }
    float    bx = lm_cosf(a), by = lm_sinf(a);
    uint64_t now = us() / 1000;
    ad_urgent    = 0;
    if (has)
    {
        int   cur = 0;
        float cur_val =
            evaluate_dodge_direction(mx, my, commit_x, commit_y, cur) +
            own_hitbox * COMMIT_BONUS;
        bool better   = best_steps > cur;
        bool die_fast = better && cur > 0 && cur <= HOLD_DANGER_STEPS;
        bool walled =
            better &&
            compute_free_run(mx, my, commit_x, commit_y, self_speed * SIM_T) <
                own_hitbox * 2.0f;
        bool held   = now - turn_ms < TURN_HOLD_MS;
        bool gain   = best > cur_val + own_hitbox * 0.5f;
        bool windup = ad_windup_n > 0;
        if (die_fast || walled || windup)
        {
            ad_urgent = 1;
            turn_ms   = now;
        }
        else if (held || !gain)
        {
            bx = commit_x, by = commit_y;
        }
        else
        {
            turn_ms = now;
        }
    }
    else
    {
        turn_ms = now;
    }
    {
        uint64_t since = now - turn_ms;
        ad_hold_ms = since >= TURN_HOLD_MS ? 0 : (int) (TURN_HOLD_MS - since);
    }
    ad_dir_x = bx, ad_dir_y = by;
    ad_best_dir =
        (lm_get_angle((int) (bx * 1000.0f), (int) (by * 1000.0f)) / 45 + 8) &
        7;
    commit_x = bx, commit_y = by;
    out_x = bx, out_y = by;
}
}

static void reset_dodge_state()
{
    ad_target_x = INT32_MAX, ad_target_y = INT32_MAX;
    ad_dir_x = 0.0f, ad_dir_y = 0.0f;
    ad_hold_ms = 0;
    ad_urgent  = 0;
    active     = 0;
}

static void learn_character_data_vtable()
{
    void* data = nullptr;
    void* vt   = nullptr;
    if (char_vt || !base || !own_elem) return;
    if (!read_ptr(own_elem + LOGICGAMEOBJECT__DATA, &data) || !data ||
        !pointer_plausible((uintptr_t) data))
        return;
    if (!read_ptr((uintptr_t) data, &vt) || !vt) return;
    uintptr_t rva = (uintptr_t) vt - base;
    if (rva == LOGICPROJECTILEDATA__VTABLE) return;
    char_vt = rva;
}
static void learn_own_hitbox()
{
    void*   data = nullptr;
    int32_t r    = 0;
    if (!base || !own_elem) return;
    if (!read_ptr(own_elem + LOGICGAMEOBJECT__DATA, &data) || !data ||
        !pointer_plausible((uintptr_t) data))
        return;
    if (!read_bytes((uintptr_t) data + LOGICCHARACTER__DATACOLLISIONRADIUS,
                    &r,
                    sizeof(r)))
        return;
    if (r <= 0) return;
    float tile = wall_logic_units(1);
    if (tile > 0.0f && (float) r >= tile) return;
    own_hitbox = (float) r;
}
void reset_autododge_state()
{
    ad_best_dir = -1;
    ad_char_n   = 0;
    ad_proj_n   = 0;
    ad_enemy_n  = 0;
    ad_threat_n = 0;
    ad_ind_n    = 0;
    threat_n    = 0;
    char_vt     = 0;
    own_hitbox  = 0.0f;
    last_hp     = 0;
    pend_dmg    = 0;
    sent_ms     = 0;
    calm_ms     = 0;
    turn_ms     = 0;
    human_ms    = 0;
    ad_human    = 0;
    ad_dir_x = 1.0f, ad_dir_y = 0.0f;
    commit_x = 0.0f, commit_y = 0.0f;
    memset(projs, 0, sizeof(projs));
    reset_dodge_state();
}
void update_autododge()
{
    ad_best_dir = -1;
    ad_char_n = ad_proj_n = ad_enemy_n = ad_threat_n = 0;
    ad_ind_n                                         = 0;
    ad_windup_n                                      = 0;
    threat_n                                         = 0;
    memset(ad_enemy_charge_ms, 0, sizeof(ad_enemy_charge_ms));
    memset(ad_enemy_charge_mode, 0, sizeof(ad_enemy_charge_mode));
    if (own_hp < last_hp)
    {
        pend_dmg = (int) (last_hp - own_hp);
    }
    last_hp = own_hp;
    if (!base || !own_elem || dead || !coord_ok)
    {
        reset_dodge_state();
        return;
    }
    learn_character_data_vtable();
    learn_own_hitbox();
    if (!char_vt || own_hitbox <= 0.0f || wall_cache_width() <= 0 ||
        wall_cache_height() <= 0)
    {
        reset_dodge_state();
        return;
    }
    int32_t own_gid = 0;
    read_int(own_elem + LOGICCHARACTER__GLOBALID, &own_gid);
    self_speed   = own_speed > 1.0f ? own_speed : 1.0f;
    uint64_t now = us() / 1000;
    float    mx = (float) own_x, my = (float) own_y;
    evict_stale_projectiles(now);
    collect_logic_objects(now, own_gid, mx, my);
    if (pend_dmg) report_incoming_hit(mx, my, commit_x, commit_y);
    {
        float mvx = (float) (own_x - prev_x), mvy = (float) (own_y - prev_y);
        float mv = lm_hypot(mvx, mvy);
        prev_x = own_x, prev_y = own_y;
        if (mv > 1.0f)
        {
            move_dx = mvx / mv;
            move_dy = mvy / mv;
        }
        else
        {
            move_dx = 0.0f;
            move_dy = 0.0f;
        }
        if (active && mv > 15.0f && (mvx * sent_x + mvy * sent_y) < 0.7f * mv)
            human_ms = now;
    }
    ad_human = now - human_ms < HUMAN_MS ? 1 : 0;
    if (threat_n || ad_windup_n)
    {
        calm_ms = now;
    }
    else if (!active || now - calm_ms >= CALM_MS)
    {
        reset_dodge_state();
        return;
    }
    float dir_x = 1.0f, dir_y = 0.0f;
    if (commit_x != 0.0f || commit_y != 0.0f)
    {
        dir_x = commit_x, dir_y = commit_y;
    }
    if (threat_n)
    {
        choose_dodge_direction(mx, my, dir_x, dir_y);
    }
    fill_autododge_report(mx, my, dir_x, dir_y);
    ad_pass_own = wall_cell_is_passable(mx, my);
    float fx = mx + dir_x * REACH, fy = my + dir_y * REACH;
    for (int k = 0; k < 3 && !wall_path_clear(mx, my, fx, fy); k++)
    {
        fx = mx + (fx - mx) * 0.5f;
        fy = my + (fy - my) * 0.5f;
    }
    float em   = compute_edge_margin();
    float wu   = wall_logic_units(wall_cache_width()),
          hu   = wall_logic_units(wall_cache_height());
    float lo_x = em < wu * 0.5f ? em : wu * 0.5f;
    float hi_x = wu - em > wu * 0.5f ? wu - em : wu * 0.5f;
    float lo_y = em < hu * 0.5f ? em : hu * 0.5f;
    float hi_y = hu - em > hu * 0.5f ? hu - em : hu * 0.5f;
    fx         = fminf(fmaxf(fx, lo_x), hi_x);
    fy         = fminf(fmaxf(fy, lo_y), hi_y);
    int32_t tx = axis(fx, wall_cache_width());
    int32_t ty = axis(fy, wall_cache_height());
    set_client_prediction_move_to(tx, ty);
    if (now - sent_ms >= SEND_MS)
    {
        sent_ms = now;
        sent_x = dir_x, sent_y = dir_y;
        enqueue_move_input(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    active = 1;
}
namespace ad
{
int find_nearest_projectile(
    float mx, float my, float dx, float dy, const int* used, float* out_clear)
{
    int   best = -1;
    float bc   = 1e30f;
    for (int i = 0; i < threat_n; i++)
    {
        bool skip = false;
        for (int k = 0; k < AD_REPORT_MAX; k++)
            if (used[k] == i) skip = true;
        if (skip) continue;
        float c = compute_threat_clearance(mx, my, dx, dy, threats[i]);
        if (c < bc)
        {
            bc   = c;
            best = i;
        }
    }
    *out_clear = bc;
    return best;
}
void report_incoming_hit(float mx, float my, float dx, float dy)
{
    ad_hit_dmg              = pend_dmg;
    ad_hit_act              = active;
    ad_hit_n                = threat_n;
    ad_hit_x                = (int) mx;
    ad_hit_y                = (int) my;
    ad_hit_picked           = 0;
    int used[AD_REPORT_MAX] = {-1, -1, -1};
    for (int s = 0; s < AD_REPORT_MAX; s++)
    {
        float bc   = 0.0f;
        int   best = find_nearest_projectile(mx, my, dx, dy, used, &bc);
        if (best < 0) break;
        used[s]       = best;
        ad_hit_ca[s]  = bc;
        ad_hit_ttc[s] = threats[best].tt;
        ad_hit_r[s]   = threats[best].rad;
        ad_hit_picked++;
    }
    pend_dmg = 0;
}
void fill_autododge_report(float mx, float my, float dx, float dy)
{
    ad_threat_n = threat_n < AD_REPORT_MAX ? threat_n : AD_REPORT_MAX;
    int used[AD_REPORT_MAX] = {-1, -1, -1};
    for (int s = 0; s < ad_threat_n; s++)
    {
        float bc   = 0.0f;
        int   best = find_nearest_projectile(mx, my, dx, dy, used, &bc);
        if (best < 0)
        {
            ad_threat_n = s;
            break;
        }
        used[s]          = best;
        const Threat& t  = threats[best];
        ad_threat_pid[s] = t.pid;
        ad_threat_x[s]   = t.x;
        ad_threat_y[s]   = t.y;
        ad_threat_vx[s]  = t.vx;
        ad_threat_vy[s]  = t.vy;
        ad_threat_r[s]   = t.rad;
        ad_threat_ca[s]  = bc;
        ad_threat_ttc[s] = t.tt;
        ad_threat_d[s]   = lm_hypot(t.x - mx, t.y - my);
        memcpy(ad_threat_name[s], t.name, sizeof(ad_threat_name[s]));
        ad_threat_name[s][sizeof(ad_threat_name[s]) - 1] = '\0';
    }
}
}
