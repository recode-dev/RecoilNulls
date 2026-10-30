#include "autododge.h"
#include "../core/commands.h"
#include "../core/scan.h"
#include "../utils/walls.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{

int deg_from_rad(float r)
{
    return static_cast<int>(r * 57.29577951f + (r < 0.0f ? -0.5f : 0.5f));
}

int math_distance_square_to_line(int ax, int ay, int bx, int by, int px, int py)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICMATH__GETDISTANCESQUARETOLINE);
    }
    if (!slot)
    {
        return 0;
    }
    int a[2] = {ax, ay};
    int b[2] = {bx, by};
    int p[2] = {px, py};
    return reinterpret_cast<int (*)(const int*, const int*, const int*)>(slot)(
        a, b, p);
}

float math_distance_to_line(
    float ax, float ay, float bx, float by, float px, float py)
{
    int sq = math_distance_square_to_line(static_cast<int>(ax + 0.5f),
                                          static_cast<int>(ay + 0.5f),
                                          static_cast<int>(bx + 0.5f),
                                          static_cast<int>(by + 0.5f),
                                          static_cast<int>(px + 0.5f),
                                          static_cast<int>(py + 0.5f));
    static uintptr_t sqrt_slot = 0;
    if (sq < 1)
    {
        sq = 1;
    }
    if (!sqrt_slot)
    {
        sqrt_slot = callable_at_rva(LOGICMATH__SQRT);
    }
    if (!sqrt_slot)
    {
        return 0.0f;
    }
    return static_cast<float>(reinterpret_cast<int (*)(int)>(sqrt_slot)(sq));
}

int32_t object_x(uintptr_t object)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICGAMEOBJECTCLIENT__GETX);
    }
    if (!slot || !object)
    {
        return 0;
    }
    return reinterpret_cast<int32_t (*)(uintptr_t)>(slot)(object);
}

int32_t object_y(uintptr_t object)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICGAMEOBJECTCLIENT__GETY);
    }
    if (!slot || !object)
    {
        return 0;
    }
    return reinterpret_cast<int32_t (*)(uintptr_t)>(slot)(object);
}

void vector_normalize(int* x, int* y, int scale)
{
    static uintptr_t slot = 0;
    int              v[2] = {0, 0};
    if (!slot)
    {
        slot = callable_at_rva(LOGICVECTOR2__NORMALIZE);
    }
    if (!slot || !x || !y)
    {
        return;
    }
    v[0] = *x;
    v[1] = *y;
    reinterpret_cast<void (*)(int*, int)>(slot)(v, scale);
    *x = v[0];
    *y = v[1];
}

void vector_rotate(int* x, int* y, int degrees)
{
    static uintptr_t slot = 0;
    int              v[2] = {0, 0};
    if (!slot)
    {
        slot = callable_at_rva(LOGICVECTOR2__ROTATE);
    }
    if (!slot || !x || !y)
    {
        return;
    }
    v[0] = *x;
    v[1] = *y;
    reinterpret_cast<void (*)(int*, int)>(slot)(v, degrees);
    *x = v[0];
    *y = v[1];
}

void dir_from_angle(float radians, float& ox, float& oy)
{
    int vx = 1024, vy = 0;
    vector_rotate(&vx, &vy, deg_from_rad(radians));
    ox = static_cast<float>(vx) * (1.0f / 1024.0f);
    oy = static_cast<float>(vy) * (1.0f / 1024.0f);
}
}  // namespace

int   in_move     = 0;
float dodge_dir_x = 1.0f, dodge_dir_y = 0.0f;
int   enemy_n            = 0;
float enemy_x[ENEMY_MAX] = {0};
float enemy_y[ENEMY_MAX] = {0};
float enemy_d[ENEMY_MAX] = {0};
int   windup_n           = 0;

namespace
{
constexpr float    REACH             = 300.0f;
constexpr int      DIRS              = 16;
constexpr int      SIM_STEPS         = 32;
constexpr float    ZONE_FAR          = 5000.0f;
constexpr uint64_t SEND_MS           = ad::TICK_MS;
constexpr int      MOVE_DELAY_TICKS  = 2;
constexpr float    SAFETY            = 40.0f;
constexpr float    PROJECTILE_RADIUS = 60.0f;
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
constexpr float    TICK_MS_SEC       = static_cast<float>(ad::TICK_MS) * 0.001f;
constexpr float    SIM_T = static_cast<float>(SIM_STEPS) * TICK_MS_SEC;

}  // namespace

int32_t projectile_radius(uintptr_t data)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICPROJECTILEDATA__GETRADIUS);
    }
    if (!slot || !data)
    {
        return 0;
    }
    return reinterpret_cast<int32_t (*)(uintptr_t)>(slot)(data);
}

int32_t projectile_speed(uintptr_t data)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICPROJECTILEDATA__GETSPEED);
    }
    if (!slot || !data)
    {
        return 0;
    }
    return reinterpret_cast<int32_t (*)(uintptr_t)>(slot)(data);
}

int32_t character_collision_radius(uintptr_t data)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICCHARACTER__GETCOLLISIONRADIUS);
    }
    if (!slot || !data)
    {
        return 0;
    }
    return reinterpret_cast<int32_t (*)(uintptr_t)>(slot)(data);
}

namespace ad
{
struct FrameGeom
{
    float w, h, em, step, far;
};
static FrameGeom frame_geom;
static int32_t   own_team_value = 0;
static int       own_team_valid = 0;

static void cache_frame_geom()
{
    frame_geom.w    = wall_logic_units(wall_cache_width());
    frame_geom.h    = wall_logic_units(wall_cache_height());
    frame_geom.step = wall_logic_units(1);
    frame_geom.em   = own_hitbox + BORDER_PAD;
    frame_geom.far  = self_speed * SIM_T;
    own_team_valid  = 0;
    uintptr_t data  = 0;
    if (own_elem && read_ptr(own_elem + LOGICGAMEOBJECT__DATA, &data) &&
        pointer_plausible(data))
    {
        own_team_valid =
            read_int(data + LOGICGAMEOBJECTDATA__TEAM, &own_team_value) ? 1 : 0;
    }
}

Threat    threats[THREAT_MAX];
int       threat_n = 0;
Proj      projs[PROJ_SLOTS];
uintptr_t char_vt    = 0;
float     self_speed = 1.0f;
float     own_hitbox = 0.0f;
}  // namespace ad

namespace ad
{
Proj* find_projectile_entry(int32_t pid)
{
    Proj* spare = nullptr;
    Proj* stale = &projs[0];
    for (auto& proj : projs)
    {
        if (proj.pid == pid) return &proj;
        if (proj.pid == 0)
        {
            if (!spare) spare = &proj;
            continue;
        }
        if (proj.seen < stale->seen) stale = &proj;
    }
    return spare ? spare : stale;
}

void init_projectile_state(
    Proj* st, int32_t pid, uintptr_t dataPtr, float px, float py, uint64_t now)
{
    st->pid            = pid;
    st->seen           = now;
    st->data           = dataPtr;
    st->x              = px;
    st->y              = py;
    st->speed          = 0.0f;
    int32_t dataRadius = projectile_radius(dataPtr);
    st->rad =
        dataRadius > 0 ? static_cast<float>(dataRadius) : PROJECTILE_RADIUS;
    st->vel = false;
    st->ux  = 0.0f;
    st->uy  = 0.0f;
}

static int object_team(uintptr_t data, int32_t* out)
{
    if (!pointer_plausible(data)) return 0;
    return read_int(data + LOGICGAMEOBJECTDATA__TEAM, out) ? 1 : 0;
}

static int own_team_matches(int32_t team)
{
    if (own_team_b >= 0 && team == own_team_b) return 1;
    return static_cast<int>((own_team_valid != 0) && team == own_team_value);
}

static void add_character_entry(const uint8_t* obj,
                                uintptr_t      objPtr,
                                uintptr_t      dataPtr,
                                int32_t        own_gid,
                                float          mx,
                                float          my)
{
    int32_t team, hp, gid, xi, yi;
    if (!object_team(dataPtr, &team)) return;
    memcpy(&hp, obj + LOGICCHARACTER__HITPOINTS, sizeof(hp));
    memcpy(&gid, obj + LOGICCHARACTER__GLOBALID, sizeof(gid));
    xi = object_x((uintptr_t) obj);
    yi = object_y((uintptr_t) obj);
    if (own_team_matches(team) || hp <= 0 || gid <= 0 || gid == own_gid) return;
    if (enemy_n >= ENEMY_MAX) return;
    int   k  = enemy_n++;
    float ex = static_cast<float>(xi) - mx, ey = static_cast<float>(yi) - my;
    enemy_x[k] = static_cast<float>(xi);
    enemy_y[k] = static_cast<float>(yi);
    enemy_d[k] = static_cast<float>(
        game_distance(0, 0, static_cast<int>(ex), static_cast<int>(ey)));
    int32_t charge     = 0;
    int32_t chargeMode = 0;
    read_int(objPtr + LOGICCHARACTER__CURRENTCHARGEUPMILLISECONDS, &charge);
    read_int(objPtr + LOGICCHARACTER__CHARGEUPMODE, &chargeMode);
    if (chargeMode == 1 && charge > 0 && enemy_d[k] <= CLOSE_R) windup_n++;
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
    int32_t xi = object_x((uintptr_t) obj);
    int32_t yi = object_y((uintptr_t) obj);
    if (static_cast<float>(xi) >= -300.0f &&
        static_cast<float>(yi) >= -300.0f &&
        static_cast<float>(xi) <= w + 300.0f &&
        static_cast<float>(yi) <= h + 300.0f)
    {
        *px = static_cast<float>(xi);
        *py = static_cast<float>(yi);
        return true;
    }
    return false;
}

static void add_projectile_threat(const uint8_t* obj,
                                  uintptr_t      objPtr,
                                  uintptr_t      dataPtr,
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
        key =
            static_cast<int32_t>(((objPtr >> 4) ^ (objPtr >> 20)) & 0x7fffffff);
    if (key <= 0) return;
    Proj* st = find_projectile_entry(key);
    if (st->pid != key)
    {
        init_projectile_state(st, key, dataPtr, px, py, now);
        if (st->pid != key) return;
    }
    st->seen       = now;
    float   step_x = px - st->x, step_y = py - st->y;
    float   step = static_cast<float>(game_distance(static_cast<int>(st->x),
                                                  static_cast<int>(st->y),
                                                  static_cast<int>(px),
                                                  static_cast<int>(py)));
    int32_t dataSpeed = projectile_speed(st->data);
    if (dataSpeed > 50 && dataSpeed < 20000)
    {
        if (step > 0.5f)
        {
            st->ux    = step_x / step;
            st->uy    = step_y / step;
            st->speed = static_cast<float>(dataSpeed);
            st->vel   = true;
        }
    }
    else
    {
        st->vel = false;
    }
    st->x = px, st->y = py;
    int32_t team = 0;
    if (!object_team(dataPtr, &team)) return;
    if (own_team_matches(team)) return;
    float dx = px - mx, dy = py - my;
    if (!st->vel) return;
    float vx = st->ux * st->speed, vy = st->uy * st->speed;
    if (static_cast<float>(game_distance_sq(
            0, 0, static_cast<int>(dx), static_cast<int>(dy))) > zone2)
        return;
    if (!wall_line_of_sight_clear(px, py, mx, my)) return;
    if (game_distance_sq(0, 0, static_cast<int>(vx), static_cast<int>(vy)) <= 0)
        return;
    float rad  = st->rad + own_hitbox + SAFETY;
    float dist = static_cast<float>(
        game_distance(0, 0, static_cast<int>(dx), static_cast<int>(dy)));
    int32_t arrival =
        game_projectile_travel_ticks(st->data,
                                     static_cast<int>(dist + 0.5f),
                                     static_cast<int>(st->speed + 0.5f));
    if (arrival < 1) return;
    float tt = static_cast<float>(arrival) * TICK_MS_SEC;
    if (tt > THREAT_HORIZON) return;
    {
        int32_t hit[2] = {0, 0};
        if (!game_segment_hits_circle(static_cast<int>(px),
                                      static_cast<int>(py),
                                      static_cast<int>(px + vx * tt),
                                      static_cast<int>(py + vy * tt),
                                      static_cast<int>(mx),
                                      static_cast<int>(my),
                                      static_cast<int>(rad + 0.5f),
                                      hit))
            return;
    }
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
}

static uintptr_t obj_rva[COUNT_MAX];

void collect_logic_objects(uint64_t now, int32_t own_gid, float mx, float my)
{
    refresh_world_extents();
    float zone = self_speed * 6.0f;
    if (zone < ZONE_FAR) zone = ZONE_FAR;
    float zone2 = zone * zone;
    for (int i = 0; i < obj_count; i++)
    {
        obj_rva[i] = 0;
        if (!obj_valid[i]) continue;
        const uint8_t* obj     = obj_block[i];
        uintptr_t      dataPtr = 0;
        memcpy(&dataPtr, obj + LOGICGAMEOBJECT__DATA, sizeof(dataPtr));
        uintptr_t vt = 0;
        if (dataPtr && pointer_plausible(dataPtr) && read_ptr(dataPtr, &vt) &&
            vt >= base)
        {
            obj_rva[i] = vt - base;
        }
        if (char_vt && obj_rva[i] == char_vt)
        {
            add_character_entry(obj, obj_snapshot[i], dataPtr, own_gid, mx, my);
            continue;
        }
        if (obj_rva[i] != LOGICPROJECTILEDATA__VTABLE) continue;
        float px = 0.0f, py = 0.0f;
        if (!read_projectile_position(obj, &px, &py)) continue;
        add_projectile_threat(
            obj, obj_snapshot[i], dataPtr, now, mx, my, zone2, px, py);
    }
}

void evict_stale_projectiles(uint64_t now)
{
    for (auto& proj : projs)
        if (proj.pid && now - proj.seen > PROJ_TTL_MS) proj.pid = 0;
}
}  // namespace ad

using namespace ad;
namespace
{
static float    commit_x = 0.0f, commit_y = 0.0f;
static uint64_t sent_ms = 0;
static float    move_dx = 0.0f, move_dy = 0.0f;
static int32_t  prev_x = 0, prev_y = 0;

static float compute_edge_pull(float mx, float my, float dx, float dy)
{
    float w    = frame_geom.w;
    float h    = frame_geom.h;
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
    float em  = frame_geom.em;
    float w   = frame_geom.w;
    float h   = frame_geom.h;
    if (dx > 1e-6f)
        run = fminf(run, (w - em - mx) / dx);
    else if (dx < -1e-6f)
        run = fminf(run, (em - mx) / dx);
    if (dy > 1e-6f)
        run = fminf(run, (h - em - my) / dy);
    else if (dy < -1e-6f)
        run = fminf(run, (em - my) / dy);
    const float step  = frame_geom.step;
    const int   steps = step > 0.0f ? (int) (run / step) : 0;
    for (int i = 1; i <= steps; i++)
    {
        const float d = step * (float) i;
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
    constexpr float delay = static_cast<float>(MOVE_DELAY_TICKS) * TICK_MS_SEC;
    float           coast = tk < delay ? tk : delay;
    float           em    = frame_geom.em;
    ax                    = mx + move_dx * self_speed * coast;
    ay                    = my + move_dy * self_speed * coast;
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

static float cost_enemy_dx[ENEMY_MAX];
static float cost_enemy_dy[ENEMY_MAX];
static float cost_enemy_dist[ENEMY_MAX];
static int   cost_enemy_count = 0;

static float   cost_threat_r2[THREAT_MAX];
static float   cost_threat_rx[THREAT_MAX];
static float   cost_threat_ry[THREAT_MAX];
static float   cost_threat_ux[THREAT_MAX];
static float   cost_threat_uy[THREAT_MAX];
static uint8_t cost_threat_moving[THREAT_MAX];
static int     cost_threat_count = 0;

static void prepare_direction_costs(float mx, float my)
{
    cost_enemy_count = enemy_n < ENEMY_MAX ? enemy_n : ENEMY_MAX;
    for (int i = 0; i < cost_enemy_count; i++)
    {
        float ex = enemy_x[i] - mx, ey = enemy_y[i] - my;
        int   ux = static_cast<int>(ex), uy = static_cast<int>(ey);
        cost_enemy_dist[i] = enemy_d[i];
        vector_normalize(&ux, &uy, 1000);
        cost_enemy_dx[i] = ux / 1000.0f;
        cost_enemy_dy[i] = uy / 1000.0f;
    }
    cost_threat_count = threat_n < THREAT_MAX ? threat_n : THREAT_MAX;
    for (int i = 0; i < cost_threat_count; i++)
    {
        const Threat& t   = threats[i];
        float         vv  = t.vx * t.vx + t.vy * t.vy;
        cost_threat_r2[i] = t.rad * t.rad;
        cost_threat_rx[i] = mx - t.x;
        cost_threat_ry[i] = my - t.y;
        if (vv >= 1e-6f)
        {
            float iv =
                1.0f /
                static_cast<float>(game_distance(
                    0, 0, static_cast<int>(t.vx), static_cast<int>(t.vy)));
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

static float compute_nearest_away(float dx, float dy)
{
    float best = 1e30f, dot = 0.0f;
    for (int i = 0; i < cost_enemy_count; i++)
    {
        float d = cost_enemy_dist[i];
        if (d < 1.0f || d >= best) continue;
        best = d;
        dot  = dx * cost_enemy_dx[i] + dy * cost_enemy_dy[i];
    }
    return best < 1e29f ? -dot : 0.0f;
}

static float compute_enemy_approach(float dx, float dy)
{
    float worst = 0.0f;
    for (int i = 0; i < cost_enemy_count; i++)
    {
        float d = cost_enemy_dist[i];
        if (d < 1.0f || d > APPROACH_R) continue;
        float dot = dx * cost_enemy_dx[i] + dy * cost_enemy_dy[i];
        if (dot <= 0.0f) continue;
        float p = dot * (1.0f - d / APPROACH_R);
        if (d < CLOSE_R && dot > 0.5f) p = 3.0f;
        if (p > worst) worst = p;
    }
    return worst;
}

static float compute_thrust_penalty(float dx, float dy)
{
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
    const float  far     = frame_geom.far;
    float        run     = compute_free_run(mx, my, dx, dy, far);
    float        room    = run >= far ? 1.0f : run / far;
    float        w       = frame_geom.w - frame_geom.em;
    float        h       = frame_geom.h - frame_geom.em;
    float        mx_step = dx * self_speed, my_step = dy * self_speed;
    static float step_time[SIM_STEPS + 1];
    float        step_x[SIM_STEPS + 1];
    float        step_y[SIM_STEPS + 1];
    int          survive = SIM_STEPS + 1;
    float        margin  = 1e30f;
    if (step_time[1] == 0.0f)
    {
        for (int k = 1; k <= SIM_STEPS; k++)
            step_time[k] = static_cast<float>(k) * TICK_MS_SEC;
    }
    for (int k = 1; k <= SIM_STEPS; k++)
    {
        simulate_position(
            mx, my, dx, dy, step_time[k], w, h, step_x[k], step_y[k]);
    }
    for (int i = 0; i < threat_n; i++)
    {
        const Threat& t   = threats[i];
        float         r2  = cost_threat_r2[i];
        int           hit = 0;
        for (int k = 1; k <= SIM_STEPS && !hit; k++)
        {
            float tk = step_time[k];
            float ex = t.x + t.vx * tk - step_x[k];
            float ey = t.y + t.vy * tk - step_y[k];
            if (static_cast<float>(game_distance_sq(
                    0, 0, static_cast<int>(ex), static_cast<int>(ey))) < r2)
                hit = k;
        }
        if (hit)
        {
            if (hit < survive) survive = hit;
            continue;
        }
        float sx = t.vx - mx_step, sy = t.vy - my_step;
        float c = math_distance_to_line(
                      t.x, t.y, t.x + sx * SIM_T, t.y + sy * SIM_T, mx, my) -
                  t.rad;
        if (c < margin) margin = c;
    }
    if (margin > 1e29f) margin = 0.0f;
    if (margin > own_hitbox * 3.0f) margin = own_hitbox * 3.0f;
    steps = survive;
    return static_cast<float>(survive) * WEIGHT_STEP + margin +
           WEIGHT_POS * room +
           WEIGHT_POS * WEIGHT_AWAY * compute_nearest_away(dx, dy) -
           WEIGHT_AXIS * compute_thrust_penalty(dx, dy) -
           WEIGHT_APPROACH * compute_enemy_approach(dx, dy) -
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
        float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(DIRS);
        float dx = 0.0f, dy = 0.0f;
        dir_from_angle(a, dx, dy);
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
    float a = 6.2831853f * (static_cast<float>(best_i) + off) /
              static_cast<float>(DIRS);
    for (int k = -6; k <= 6; k++)
    {
        float ra  = a + static_cast<float>(k) * 0.0174533f;
        float rdx = 0.0f, rdy = 0.0f;
        dir_from_angle(ra, rdx, rdy);
        int   steps = 0;
        float v     = evaluate_dodge_direction(mx, my, rdx, rdy, steps);
        if (has)
            v += own_hitbox * COMMIT_BONUS * (rdx * commit_x + rdy * commit_y);
        if (v > best)
        {
            best = v, a = ra, best_steps = steps;
        }
    }
    float bx = 0.0f, by = 0.0f;
    dir_from_angle(a, bx, by);
    if (has)
    {
        int   cur = 0;
        float cur_val =
            evaluate_dodge_direction(mx, my, commit_x, commit_y, cur) +
            own_hitbox * COMMIT_BONUS;
        bool better = best_steps > cur;
        bool walled =
            better &&
            compute_free_run(mx, my, commit_x, commit_y, frame_geom.far) <
                own_hitbox * 2.0f;
        bool gain   = best > cur_val + own_hitbox * 0.5f;
        bool windup = windup_n > 0;
        if (!(walled || windup) && !gain)
        {
            bx = commit_x, by = commit_y;
        }
    }
    dodge_dir_x = bx, dodge_dir_y = by;
    commit_x = bx, commit_y = by;
    out_x = bx, out_y = by;
}
}  // namespace

static void reset_dodge_state()
{
    dodge_dir_x = 0.0f, dodge_dir_y = 0.0f;
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
    r = character_collision_radius((uintptr_t) data);
    if (r <= 0) return;
    float tile = wall_logic_units(1);
    if (tile > 0.0f && static_cast<float>(r) >= tile) return;
    own_hitbox = static_cast<float>(r);
}

void reset_autododge_state()
{
    enemy_n     = 0;
    windup_n    = 0;
    threat_n    = 0;
    char_vt     = 0;
    own_hitbox  = 0.0f;
    sent_ms     = 0;
    dodge_dir_x = 1.0f, dodge_dir_y = 0.0f;
    commit_x = 0.0f, commit_y = 0.0f;
    for (Proj& p : projs)
        p = Proj{};
    reset_dodge_state();
}

void update_autododge()
{
    enemy_n  = 0;
    windup_n = 0;
    threat_n = 0;
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
    self_speed = own_speed > 1.0f ? own_speed : 1.0f;
    cache_frame_geom();
    uint64_t now = us() / 1000;
    float    mx = static_cast<float>(own_x), my = static_cast<float>(own_y);
    evict_stale_projectiles(now);
    collect_logic_objects(now, own_gid, mx, my);
    {
        float mvx = static_cast<float>(own_x - prev_x),
              mvy = static_cast<float>(own_y - prev_y);
        float mv  = static_cast<float>(
            game_distance(0, 0, static_cast<int>(mvx), static_cast<int>(mvy)));
        prev_x = own_x, prev_y = own_y;
        if (mv > 1.0f)
        {
            int nx = static_cast<int>(mvx), ny = static_cast<int>(mvy);
            vector_normalize(&nx, &ny, 1000);
            move_dx = nx / 1000.0f;
            move_dy = ny / 1000.0f;
        }
        else
        {
            move_dx = 0.0f;
            move_dy = 0.0f;
        }
    }
    if (!threat_n && !windup_n)
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
    float fx = mx + dir_x * REACH, fy = my + dir_y * REACH;
    for (int k = 0; k < 3 && !wall_path_clear(mx, my, fx, fy); k++)
    {
        fx = mx + (fx - mx) * 0.5f;
        fy = my + (fy - my) * 0.5f;
    }
    float em = frame_geom.em;
    float wu = frame_geom.w, hu = frame_geom.h;
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
        enqueue_move_input(tx, ty);
    }
}
