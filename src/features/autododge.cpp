#include "autododge.h"
#include "../core/commands.h"
#include "../core/scan.h"
#include "../utils/walls.h"
#include <cmath>
#include <cstdint>
#include <cstring>
int move = 0;

namespace
{
int segment_bbox_misses_circle(int x1, int y1, int x2, int y2, int cx, int cy, int r)
{
    int lox = x1 < x2 ? x1 : x2;
    int hix = x1 < x2 ? x2 : x1;
    int loy = y1 < y2 ? y1 : y2;
    int hiy = y1 < y2 ? y2 : y1;
    return static_cast<int>(hix < cx - r || lox > cx + r || hiy < cy - r || loy > cy + r);
}

float fast_len(float x, float y)
{
    return sqrtf(x * x + y * y);
}

void vector_normalize(int* x, int* y, int scale)
{
    if (!x || !y)
    {
        return;
    }
    float fx  = static_cast<float>(*x);
    float fy  = static_cast<float>(*y);
    float len = sqrtf(fx * fx + fy * fy);
    if (len < 1.0f)
    {
        return;
    }
    float s = static_cast<float>(scale) / len;
    *x      = static_cast<int>(fx * s + (fx >= 0.0f ? 0.5f : -0.5f));
    *y      = static_cast<int>(fy * s + (fy >= 0.0f ? 0.5f : -0.5f));
}

constexpr int   ENEMY_MAX          = 4;
int             enemy_n            = 0;
float           enemy_x[ENEMY_MAX] = {0};
float           enemy_y[ENEMY_MAX] = {0};
float           enemy_d[ENEMY_MAX] = {0};
constexpr int   SIM_STEPS          = 32;
constexpr int   MOVE_DELAY_TICKS   = 2;
constexpr int   HOLD_TICKS         = 3;
constexpr float SWITCH_GAIN        = 1.4f;
constexpr float SWITCH_ALIGN       = 0.9f;
constexpr float TARGET_LERP        = 0.5f;
constexpr float WEIGHT_STEP        = 4096.0f;
constexpr float WEIGHT_POS         = 900.0f;
constexpr float WEIGHT_AWAY        = 0.06f;
constexpr float WEIGHT_AXIS        = 2600.0f;
constexpr float AXIS_INTO          = 0.35f;
constexpr float WEIGHT_APPROACH    = 2000.0f;
constexpr float COMMIT_BONUS       = 3.0f;
constexpr float WEIGHT_EDGE        = 1600.0f;
constexpr float TICK_MS_SEC        = 50.0f * 0.001f;
constexpr float SIM_T              = static_cast<float>(SIM_STEPS) * TICK_MS_SEC;

struct Span
{
    float tile, reach, edge, pad, close, approach, far;
};

Span span = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

void cache_span()
{
    span.tile     = wall_logic_units(1);
    span.reach    = span.tile;
    span.edge     = span.tile;
    span.pad      = span.tile * 0.2f;
    span.close    = span.tile * 4.0f;
    span.approach = span.tile * 8.0f;
    span.far      = span.tile * 16.0f;
}

constexpr int      THREAT_MAX   = 32;
constexpr int      PROJ_SLOTS   = 48;
constexpr uint64_t PROJ_TTL_MS  = 50 * 20;
constexpr uint64_t PROJ_GONE_MS = 150;

struct Threat
{
    float   x, y, vx, vy, rad;
    int32_t pid;
};

struct Proj
{
    uint64_t  seen;
    uint64_t  move_ms;
    int32_t   pid;
    float     x, y, ux, uy, speed, rad, flown;
    uintptr_t data;
    bool      vel;
};

Threat    threats[THREAT_MAX];
int       threat_n = 0;
Proj      projs[PROJ_SLOTS];
uintptr_t char_vt               = 0;
float     self_speed            = 1.0f;
float     hitbox                = 0.0f;
uintptr_t enemy_data[ENEMY_MAX] = {0};
int       enemy_data_n          = 0;

struct FrameGeom
{
    float w, h, em, step, far;
};

FrameGeom frame_geom;

void cache_frame_geom()
{
    frame_geom.w    = wall_logic_units(wall_cache_width());
    frame_geom.h    = wall_logic_units(wall_cache_height());
    frame_geom.step = span.tile;
    frame_geom.em   = hitbox + span.pad;
    frame_geom.far  = self_speed * SIM_T;
}

int32_t projectile_radius(uintptr_t data)
{
    int32_t radius = 0;
    if (!data) return 0;
    if (!read_int(data + LOGICPROJECTILEDATA__GETRADIUS, &radius)) return 0;
    return radius;
}

int32_t projectile_speed(uintptr_t data)
{
    int32_t speed = 0;
    if (!data) return 0;
    if (!read_int(data + LOGICPROJECTILEDATA__GETSPEED, &speed)) return 0;
    return speed;
}

uintptr_t cc_data = 0;
int32_t   cc_r    = 0;

int32_t character_collision_radius(uintptr_t data)
{
    if (!data) return 0;
    if (data == cc_data) return cc_r;
    uintptr_t slot = callable(LOGICCHARACTERDATA__GETCOLLISIONRADIUS);
    if (!slot) return 0;
    cc_data = data;
    cc_r    = reinterpret_cast<int32_t (*)(uintptr_t)>(slot)(data);
    return cc_r;
}

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
    st->pid     = pid;
    st->seen    = now;
    st->move_ms = now;
    st->data    = dataPtr;
    st->x       = px;
    st->y       = py;
    st->speed   = 0.0f;
    st->rad     = static_cast<float>(projectile_radius(dataPtr));
    st->flown   = 0.0f;
    st->vel     = false;
    st->ux      = 0.0f;
    st->uy      = 0.0f;
}

constexpr int RANGE_SLOTS = 32;

struct RangeLearn
{
    uintptr_t data;
    float     range;
};

RangeLearn range_learn[RANGE_SLOTS];
uint32_t   range_next = 0;

float learned_flight_range(uintptr_t data)
{
    if (!data) return 0.0f;
    for (auto& i : range_learn)
    {
        if (i.data == data) return i.range;
    }
    return 0.0f;
}

void learn_flight_range(uintptr_t data, float flown)
{
    if (!data || !std::isfinite(flown)) return;
    float tile = wall_logic_units(1);
    if (tile <= 0.0f || flown < tile) return;
    for (auto& i : range_learn)
    {
        if (i.data == data)
        {
            if (flown > i.range) i.range = flown;
            return;
        }
    }
    RangeLearn& slot = range_learn[range_next % RANGE_SLOTS];
    range_next++;
    slot.data  = data;
    slot.range = flown;
}

void character_entry(const uint8_t* obj, int32_t own_gid, float mx, float my)
{
    int32_t hp = 0, gid = 0, xi = 0, yi = 0;
    memcpy(&hp, obj + LOGICGAMEOBJECTCLIENT__HP, sizeof(hp));
    memcpy(&gid, obj + LOGICGAMEOBJECTCLIENT__GETGLOBALID, sizeof(gid));
    memcpy(&xi, obj + LOGICGAMEOBJECTCLIENT__GETX, sizeof(xi));
    memcpy(&yi, obj + LOGICGAMEOBJECTCLIENT__GETY, sizeof(yi));
    if (hp <= 0 || gid <= 0 || gid == own_gid) return;
    if (is_owned_team_object(obj)) return;
    if (enemy_n >= ENEMY_MAX) return;
    int k        = enemy_n++;
    enemy_x[k]   = static_cast<float>(xi);
    enemy_y[k]   = static_cast<float>(yi);
    enemy_d[k]   = fast_len(static_cast<float>(xi) - mx, static_cast<float>(yi) - my);
    uintptr_t cd = 0;
    memcpy(&cd, obj + LOGICGAMEOBJECTCLIENT__GETDATA, sizeof(cd));
    if (cd && pointer_plausible(cd))
    {
        int  lim  = enemy_data_n < ENEMY_MAX ? enemy_data_n : ENEMY_MAX;
        bool have = false;
        for (int i = 0; i < lim; i++)
        {
            if (enemy_data[i] == cd)
            {
                have = true;
                break;
            }
        }
        if (!have) enemy_data[enemy_data_n++ % ENEMY_MAX] = cd;
    }
}

float world_extent_x = 0.0f;
float world_extent_y = 0.0f;

void refresh_world_extents()
{
    float unit     = wall_logic_units(1);
    world_extent_x = static_cast<float>(wall_cache_width()) * unit;
    world_extent_y = static_cast<float>(wall_cache_height()) * unit;
}

bool read_projectile_position(const uint8_t* obj, float* px, float* py)
{
    float w = world_extent_x;
    float h = world_extent_y;
    if (w <= 0.0f || h <= 0.0f) return false;
    int32_t xi = 0;
    int32_t yi = 0;
    memcpy(&xi, obj + LOGICGAMEOBJECTCLIENT__GETX, sizeof(xi));
    memcpy(&yi, obj + LOGICGAMEOBJECTCLIENT__GETY, sizeof(yi));
    if (static_cast<float>(xi) >= -300.0f && static_cast<float>(yi) >= -300.0f &&
        static_cast<float>(xi) <= w + 300.0f && static_cast<float>(yi) <= h + 300.0f)
    {
        *px = static_cast<float>(xi);
        *py = static_cast<float>(yi);
        return true;
    }
    return false;
}

// v36: LogicSkillData::getProjectile() = *(this + 0x58) — single LogicProjectileData*.
// v69 field proven by find_offsets (score 1.000): 0x9b4e14 ldr x0,[x0,#0x58];ret.
bool skill_projectile_is(uintptr_t sd, uintptr_t dataPtr)
{
    uintptr_t proj = 0;
    return read_ptr(sd + LOGICSKILLDATA__PROJECTILE, &proj) && proj == dataPtr;
}

int32_t skill_casting_range_or_zero(uintptr_t sd)
{
    int32_t tiles = game_skill_casting_range(sd);
    return tiles >= 1 && tiles <= 30 ? tiles : 0;
}

void learn_skill_range(uintptr_t sd, uintptr_t dataPtr)
{
    int32_t tiles = skill_casting_range_or_zero(sd);
    if (!tiles) return;
    float rad = static_cast<float>(projectile_radius(dataPtr));
    learn_flight_range(dataPtr, static_cast<float>(tiles + 1) * 100.0f + 2.0f * rad);
}

void resolve_flight_range(uintptr_t dataPtr)
{
    if (!dataPtr) return;
    int lim = enemy_data_n < ENEMY_MAX ? enemy_data_n : ENEMY_MAX;
    for (int i = 0; i < lim; i++)
    {
        for (int s = 0; s < 2; s++)
        {
            uintptr_t sd = s ? game_character_ulti_skill(enemy_data[i])
                             : game_character_weapon_skill(enemy_data[i]);
            if (!sd || !pointer_plausible(sd)) continue;
            if (!skill_projectile_is(sd, dataPtr)) continue;
            learn_skill_range(sd, dataPtr);
            if (learned_flight_range(dataPtr) > 0.0f) return;
        }
    }
}

// Pre-cast ranges from enemy weapon/ulti skills so the FIRST shot of each
// projectile type already has a known flight range.
void warm_flight_ranges()
{
    int lim = enemy_data_n < ENEMY_MAX ? enemy_data_n : ENEMY_MAX;
    for (int i = 0; i < lim; i++)
    {
        for (int s = 0; s < 2; s++)
        {
            uintptr_t sd = s ? game_character_ulti_skill(enemy_data[i])
                             : game_character_weapon_skill(enemy_data[i]);
            if (!sd || !pointer_plausible(sd)) continue;
            uintptr_t proj = 0;
            if (!read_ptr(sd + LOGICSKILLDATA__PROJECTILE, &proj) || !proj) continue;
            if (learned_flight_range(proj) > 0.0f) continue;
            learn_skill_range(sd, proj);
        }
    }
}

void projectile_threat(const uint8_t* obj,
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
    memcpy(&pid, obj + LOGICGAMEOBJECTCLIENT__GETGLOBALID, sizeof(pid));
    int32_t key = pid;
    if (key <= 0) key = static_cast<int32_t>(((objPtr >> 4) ^ (objPtr >> 20)) & 0x7fffffff);
    if (key <= 0) return;
    Proj* st = find_projectile_entry(key);
    if (st->pid != key)
    {
        init_projectile_state(st, key, dataPtr, px, py, now);
    }
    st->seen       = now;
    float   step_x = px - st->x, step_y = py - st->y;
    float   step      = fast_len(step_x, step_y);
    int32_t dataSpeed = projectile_speed(dataPtr);
    if (dataSpeed > 50 && dataSpeed < 20000)
    {
        if (step > 0.5f)
        {
            st->ux    = step_x / step;
            st->uy    = step_y / step;
            st->speed = static_cast<float>(dataSpeed);
            st->vel   = true;
            st->flown += step;
            st->move_ms = now;
        }
        else if (st->vel && now - st->move_ms >= 200)
        {
            learn_flight_range(st->data, st->flown);
            st->vel = false;
        }
    }
    else
    {
        st->vel = false;
    }
    st->x = px, st->y = py;
    if (is_owned_team_object(obj)) return;
    float dx = px - mx, dy = py - my;
    if (!st->vel) return;
    float vx = st->ux * st->speed, vy = st->uy * st->speed;
    if (vx * vx + vy * vy <= 0.0f) return;
    if (dx * vx + dy * vy > 0.0f) return;
    if (dx * dx + dy * dy > zone2) return;
    if (!wall_line_of_sight_clear(px, py, mx, my)) return;
    float rad    = st->rad + hitbox;
    float dist   = fast_len(dx, dy);
    float remain = dist;
    float limit  = learned_flight_range(dataPtr);
    if (limit <= 0.0f)
    {
        resolve_flight_range(dataPtr);
        limit = learned_flight_range(dataPtr);
    }
    if (limit > 0.0f)
    {
        remain = limit + st->speed * 0.05f - st->flown;
        if (remain < dist) return;
    }
    // Unknown range: no gate — assume the shot reaches us and dodge.
    int32_t arrival = game_projectile_travel_ticks(
        dataPtr, static_cast<int>(remain + 0.5f), static_cast<int>(st->speed + 0.5f));
    if (arrival < 1) return;
    float tt = static_cast<float>(arrival) * TICK_MS_SEC;
    float ft = remain / st->speed;
    if (ft < tt) tt = ft;
    if (tt > SIM_T) return;
    {
        int32_t sx1    = static_cast<int>(px);
        int32_t sy1    = static_cast<int>(py);
        int32_t sx2    = static_cast<int>(px + vx * tt);
        int32_t sy2    = static_cast<int>(py + vy * tt);
        int32_t scx    = static_cast<int>(mx);
        int32_t scy    = static_cast<int>(my);
        int32_t srad   = static_cast<int>(rad + 0.5f);
        int32_t hit[2] = {0, 0};
        if (segment_bbox_misses_circle(sx1, sy1, sx2, sy2, scx, scy, srad)) return;
        if (!game_segment_hits_circle(sx1, sy1, sx2, sy2, scx, scy, srad, hit)) return;
    }
    for (int k = 0; k < threat_n; k++)
    {
        if (fabsf(threats[k].x - px) < 1.0f && fabsf(threats[k].y - py) < 1.0f &&
            fabsf(threats[k].vx - vx) < 1.0f && fabsf(threats[k].vy - vy) < 1.0f)
            return;
    }
    if (threat_n >= THREAT_MAX) return;
    Threat& t = threats[threat_n++];
    t.x = px, t.y = py, t.vx = vx, t.vy = vy, t.pid = key;
    t.rad = rad;
}

struct VtCache
{
    uintptr_t vt;
    int       verdict;
};

VtCache vt_cache[64] = {};

int data_is_projectile(uintptr_t dataPtr, uintptr_t vt)
{
    if (!dataPtr || !vt || vt < base) return 0;
    VtCache& entry = vt_cache[(vt >> 3) & 63];
    if (entry.vt == vt) return entry.verdict;
    int       verdict = 0;
    uintptr_t fn      = 0;
    if (read_ptr(vt + LOGICDATA__ISPROJECTILE, &fn) && addr_executable(fn))
    {
        using fn_t = int (*)(uintptr_t);
        verdict    = reinterpret_cast<fn_t>(fn)(dataPtr);
    }
    entry.vt      = vt;
    entry.verdict = verdict;
    return verdict;
}

uintptr_t obj_rva[COUNT_MAX];

void collect_logic_objects(uint64_t now, int32_t own_gid, float mx, float my)
{
    refresh_world_extents();
    float zone  = span.far;
    float zone2 = zone * zone;
    for (int i = 0; i < obj_count; i++)
    {
        obj_rva[i] = 0;
        if (!valid[i]) continue;
        const uint8_t* obj     = block[i];
        uintptr_t      dataPtr = 0;
        memcpy(&dataPtr, obj + LOGICGAMEOBJECTCLIENT__GETDATA, sizeof(dataPtr));
        uintptr_t vt = 0;
        if (dataPtr && pointer_plausible(dataPtr) && read_ptr(dataPtr, &vt) && vt >= base)
        {
            obj_rva[i] = vt - base;
        }
        if (!obj_rva[i]) continue;
        if (char_vt && obj_rva[i] == char_vt)
        {
            character_entry(obj, own_gid, mx, my);
            continue;
        }
        if (!data_is_projectile(dataPtr, obj_rva[i] + base)) continue;
        float px = 0.0f, py = 0.0f;
        if (!read_projectile_position(obj, &px, &py)) continue;
        projectile_threat(obj, snapshot[i], dataPtr, now, mx, my, zone2, px, py);
    }
}

void evict_stale_projectiles(uint64_t now)
{
    for (auto& proj : projs)
    {
        if (!proj.pid) continue;
        if (proj.vel && now - proj.seen >= PROJ_GONE_MS)
        {
            learn_flight_range(proj.data, proj.flown);
            proj.vel = false;
        }
        if (now - proj.seen > PROJ_TTL_MS)
        {
            learn_flight_range(proj.data, proj.flown);
            proj.pid = 0;
        }
    }
}

float    commit_x = 0.0f, commit_y = 0.0f;
uint64_t sent_frame = 0;
float    move_dx = 0.0f, move_dy = 0.0f;
int      hold_left = 0;
float    tgt_x = 0.0f, tgt_y = 0.0f;
int32_t  last_tx = 0, last_ty = 0;
int      last_valid = 0;
int32_t  prev_x = 0, prev_y = 0;

float compute_edge_pull(float mx, float my, float dx, float dy)
{
    float w    = frame_geom.w;
    float h    = frame_geom.h;
    float zone = hitbox + span.edge;
    float pen  = 0.0f;
    if (mx < zone) pen -= dx * (zone - mx) / zone;
    if (mx > w - zone) pen += dx * (mx - (w - zone)) / zone;
    if (my < zone) pen -= dy * (zone - my) / zone;
    if (my > h - zone) pen += dy * (my - (h - zone)) / zone;
    return pen;
}

float compute_free_run(float mx, float my, float dx, float dy, float far)
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
    if (run <= 0.0f) return 0.0f;
    if (wall_segment_clear(mx, my, mx + dx * run, my + dy * run, hitbox))
    {
        return run;
    }
    const float step = frame_geom.step;
    float       low  = 0.0f;
    float       high = run;
    for (int i = 0; i < 8 && high - low > step * 0.5f; i++)
    {
        const float mid = 0.5f * (low + high);
        if (wall_segment_clear(mx, my, mx + dx * mid, my + dy * mid, hitbox))
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }
    return low;
}

float hit_time(const Threat& t, float px, float py, float vx, float vy, float from, float to)
{
    if (to <= from) return -1.0f;
    float ux    = vx - t.vx;
    float uy    = vy - t.vy;
    float dur   = to - from;
    int   rx    = static_cast<int>(px - vx * from - t.x);
    int   ry    = static_cast<int>(py - vy * from - t.y);
    int   ex    = rx + static_cast<int>(ux * dur);
    int   ey    = ry + static_cast<int>(uy * dur);
    int   pt[2] = {rx, ry};
    int   tr    = static_cast<int>(t.rad);
    if (segment_bbox_misses_circle(rx, ry, ex, ey, 0, 0, tr))
    {
        return -1.0f;
    }
    if (!game_segment_hits_circle(rx, ry, ex, ey, 0, 0, tr, pt))
    {
        return -1.0f;
    }
    float sp = sqrtf(ux * ux + uy * uy);
    if (sp < 1.0f) return -1.0f;
    float dxp = static_cast<float>(rx - pt[0]);
    float dyp = static_cast<float>(ry - pt[1]);
    float hit = from + sqrtf(dxp * dxp + dyp * dyp) / sp;
    return hit <= to ? hit : -1.0f;
}

float closest_approach(const Threat& t, float px, float py, float vx, float vy, float span)
{
    int ax    = static_cast<int>(px - t.x);
    int ay    = static_cast<int>(py - t.y);
    int bx    = ax + static_cast<int>((vx - t.vx) * span);
    int by    = ay + static_cast<int>((vy - t.vy) * span);
    int pt[2] = {ax, ay};
    int tr    = static_cast<int>(t.rad);
    if (segment_bbox_misses_circle(ax, ay, bx, by, 0, 0, tr))
    {
        return 1.0e30f;
    }
    if (game_segment_hits_circle(ax, ay, bx, by, 0, 0, tr, pt))
    {
        return 0.0f;
    }
    return 1.0e30f;
}

float   cost_enemy_dx[ENEMY_MAX];
float   cost_enemy_dy[ENEMY_MAX];
float   cost_enemy_dist[ENEMY_MAX];
int     cost_enemy_count = 0;
float   cost_threat_rx[THREAT_MAX];
float   cost_threat_ry[THREAT_MAX];
float   cost_threat_ux[THREAT_MAX];
float   cost_threat_uy[THREAT_MAX];
uint8_t cost_threat_moving[THREAT_MAX];
int     cost_threat_count = 0;

void prepare_direction_costs(float mx, float my)
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
        cost_threat_rx[i] = mx - t.x;
        cost_threat_ry[i] = my - t.y;
        if (vv >= 1e-6f)
        {
            float iv              = 1.0f / fast_len(t.vx, t.vy);
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

float compute_nearest_away(float dx, float dy)
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

float compute_enemy_approach(float dx, float dy)
{
    float worst = 0.0f;
    for (int i = 0; i < cost_enemy_count; i++)
    {
        float d = cost_enemy_dist[i];
        if (d < 1.0f || d > span.approach) continue;
        float dot = dx * cost_enemy_dx[i] + dy * cost_enemy_dy[i];
        if (dot <= 0.0f) continue;
        float p = dot * (1.0f - d / span.approach);
        if (d < span.close && dot > 0.5f) p = 3.0f;
        if (p > worst) worst = p;
    }
    return worst;
}

float compute_thrust_penalty(float dx, float dy)
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

float evaluate_dodge_direction(float mx, float my, float dx, float dy, int& steps)
{
    const float far     = frame_geom.far;
    float       run     = compute_free_run(mx, my, dx, dy, far);
    float       room    = run >= far ? 1.0f : run / far;
    const float delay   = static_cast<float>(MOVE_DELAY_TICKS) * TICK_MS_SEC;
    const float coast   = delay < SIM_T ? delay : SIM_T;
    float       vdx     = dx * self_speed;
    float       vdy     = dy * self_speed;
    float       vcx     = move_dx * self_speed;
    float       vcy     = move_dy * self_speed;
    float       bx      = mx + vcx * coast;
    float       by      = my + vcy * coast;
    float       margin  = 1e30f;
    int         survive = SIM_STEPS + 1;
    for (int i = 0; i < threat_n; i++)
    {
        const Threat& t   = threats[i];
        float         hit = hit_time(t, mx, my, vcx, vcy, 0.0f, coast);
        if (hit < 0.0f) hit = hit_time(t, bx, by, vdx, vdy, coast, SIM_T);
        if (hit >= 0.0f)
        {
            int k = static_cast<int>(ceilf(hit / TICK_MS_SEC));
            if (k < 1) k = 1;
            if (k < survive) survive = k;
            continue;
        }
        float c = closest_approach(t, mx, my, vdx, vdy, SIM_T);
        if (c < margin) margin = c;
    }
    if (margin > 1e29f) margin = 0.0f;
    if (margin > hitbox * 3.0f) margin = hitbox * 3.0f;
    steps = survive;
    return static_cast<float>(survive) * WEIGHT_STEP + margin + WEIGHT_POS * room +
           WEIGHT_POS * WEIGHT_AWAY * compute_nearest_away(dx, dy) -
           WEIGHT_AXIS * compute_thrust_penalty(dx, dy) -
           WEIGHT_APPROACH * compute_enemy_approach(dx, dy) -
           WEIGHT_EDGE * compute_edge_pull(mx, my, dx, dy) -
           hitbox * wall_pull(mx, my, dx, dy, hitbox);
}

void choose_dodge_direction(float mx, float my, float& out_x, float& out_y)
{
    bool has = commit_x != 0.0f || commit_y != 0.0f;
    prepare_direction_costs(mx, my);
    float cand_x[64];
    float cand_y[64];
    int   candidate_count = 0;
    for (int i = 0; i < threat_n && candidate_count + 3 < 64; i++)
    {
        const Threat& t         = threats[i];
        cand_x[candidate_count] = mx - t.x;
        cand_y[candidate_count] = my - t.y;
        candidate_count++;
        cand_x[candidate_count] = -t.vy, cand_y[candidate_count] = t.vx;
        candidate_count++;
        cand_x[candidate_count] = t.vy, cand_y[candidate_count] = -t.vx;
        candidate_count++;
    }
    if (has && candidate_count < 64)
    {
        cand_x[candidate_count] = commit_x;
        cand_y[candidate_count] = commit_y;
        candidate_count++;
    }
    float best   = -1e30f;
    float best_x = 0.0f, best_y = 0.0f;
    int   best_steps = 0;
    for (int i = 0; i < candidate_count; i++)
    {
        float dx = cand_x[i], dy = cand_y[i];
        float len = fast_len(dx, dy);
        if (len < 1.0f) continue;
        dx /= len, dy /= len;
        int   steps = 0;
        float score = evaluate_dodge_direction(mx, my, dx, dy, steps);
        if (has) score += hitbox * COMMIT_BONUS * (dx * commit_x + dy * commit_y);
        if (score > best)
        {
            best = score, best_x = dx, best_y = dy, best_steps = steps;
        }
    }
    if (has)
    {
        int   current_steps = 0;
        float current_value = evaluate_dodge_direction(mx, my, commit_x, commit_y, current_steps) +
                              hitbox * COMMIT_BONUS;
        bool better = best_steps > current_steps;
        bool walled =
            better && compute_free_run(mx, my, commit_x, commit_y, frame_geom.far) < hitbox * 2.0f;
        bool gain      = best > current_value + hitbox * SWITCH_GAIN;
        bool turned    = (best_x * commit_x + best_y * commit_y) < SWITCH_ALIGN;
        bool switch_ok = walled || (gain && turned && hold_left <= 0);
        if (!switch_ok)
        {
            best_x = commit_x;
            best_y = commit_y;
            if (hold_left > 0) hold_left--;
        }
        else
        {
            hold_left = HOLD_TICKS;
        }
    }
    else
    {
        hold_left = HOLD_TICKS;
    }
    commit_x = best_x, commit_y = best_y;
    out_x = best_x, out_y = best_y;
}

void reset_dodge_state()
{
    hold_left = 0;
    tgt_x = 0.0f, tgt_y = 0.0f;
    last_tx    = 0;
    last_ty    = 0;
    last_valid = 0;
}

void learn_character_data_vtable()
{
    void* data = nullptr;
    void* vt   = nullptr;
    if (char_vt || !base || !elem) return;
    if (!read_ptr(elem + LOGICGAMEOBJECTCLIENT__GETDATA, &data) || !data ||
        !pointer_plausible((uintptr_t) data))
        return;
    if (!read_ptr((uintptr_t) data, &vt) || !vt) return;
    if (data_is_projectile((uintptr_t) data, (uintptr_t) vt)) return;
    char_vt = (uintptr_t) vt - base;
}

void learn_own_hitbox()
{
    void*   data = nullptr;
    int32_t r    = 0;
    if (!base || !elem) return;
    if (!read_ptr(elem + LOGICGAMEOBJECTCLIENT__GETDATA, &data) || !data ||
        !pointer_plausible((uintptr_t) data))
        return;
    r = character_collision_radius((uintptr_t) data);
    if (r <= 0) return;
    float tile = wall_logic_units(1);
    if (tile > 0.0f && static_cast<float>(r) >= tile) return;
    hitbox = static_cast<float>(r);
}
}

void reset_autododge_state()
{
    enemy_n      = 0;
    threat_n     = 0;
    char_vt      = 0;
    hitbox       = 0.0f;
    cc_data      = 0;
    cc_r         = 0;
    sent_frame   = 0;
    enemy_data_n = 0;
    for (uintptr_t& ed : enemy_data)
        ed = 0;
    commit_x = 0.0f, commit_y = 0.0f;
    for (Proj& p : projs)
        p = Proj{};
    for (RangeLearn& rl : range_learn)
        rl = RangeLearn{};
    range_next = 0;
    reset_dodge_state();
}

void update_autododge()
{
    enemy_n  = 0;
    threat_n = 0;
    if (!base || !elem || dead || !coord_ok)
    {
        reset_dodge_state();
        return;
    }
    learn_character_data_vtable();
    learn_own_hitbox();
    if (!char_vt || hitbox <= 0.0f || wall_cache_width() <= 0 || wall_cache_height() <= 0)
    {
        reset_dodge_state();
        return;
    }
    int32_t own_gid = 0;
    if (own_block)
        memcpy(&own_gid, own_block + LOGICGAMEOBJECTCLIENT__GETGLOBALID, sizeof(own_gid));
    else
        read_int(elem + LOGICGAMEOBJECTCLIENT__GETGLOBALID, &own_gid);
    self_speed = own_speed > 1.0f ? own_speed : 1.0f;
    cache_span();
    cache_frame_geom();
    uint64_t now = us() / 1000;
    float    mx = static_cast<float>(own_x), my = static_cast<float>(own_y);
    evict_stale_projectiles(now);
    collect_logic_objects(now, own_gid, mx, my);
    warm_flight_ranges();
    {
        float mvx = static_cast<float>(own_x - prev_x), mvy = static_cast<float>(own_y - prev_y);
        float mv = fast_len(mvx, mvy);
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
    if (!threat_n)
    {
        reset_dodge_state();
        return;
    }
    float dir_x = commit_x, dir_y = commit_y;
    choose_dodge_direction(mx, my, dir_x, dir_y);
    float fx = mx + dir_x * span.reach, fy = my + dir_y * span.reach;
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
    if (!last_valid)
    {
        tgt_x = fx, tgt_y = fy, last_valid = 1;
    }
    else
    {
        tgt_x += (fx - tgt_x) * TARGET_LERP;
        tgt_y += (fy - tgt_y) * TARGET_LERP;
    }
    tgt_x      = fminf(fmaxf(tgt_x, lo_x), hi_x);
    tgt_y      = fminf(fmaxf(tgt_y, lo_y), hi_y);
    int32_t tx = axis(tgt_x, wall_cache_width());
    int32_t ty = axis(tgt_y, wall_cache_height());
    if (tx != last_tx || ty != last_ty)
    {
        client_prediction_move_to(tx, ty);
        last_tx = tx, last_ty = ty;
    }
    if (sent_frame != frame_index)
    {
        sent_frame = frame_index;
        enqueue_move_input(tx, ty);
    }
}
