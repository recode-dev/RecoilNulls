#include "autododge.h"
#include "../core/commands.h"
#include "../core/scan.h"
#include "../utils/walls.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
constexpr float TARGET_LERP   = 0.5f;
constexpr int   SIM_T_NUM     = 16;
constexpr int   SIM_T_DEN     = 10;
constexpr int   RANGE_UNIT    = 100;
constexpr int   STEER_SCALE   = 4096;
constexpr float STEER_SCALE_F = 4096.0f;

struct Span
{
    int32_t tile;
    int32_t pad;
};

Span span = {0, 0};

void cache_span()
{
    span.tile = wall_logic_units(1);
    span.pad  = span.tile / 5;
}

constexpr int THREAT_MAX = 32;

constexpr int RANGE_MEMO_SLOTS = 256;

struct RangeMemo
{
    uintptr_t data;
    int32_t   range;
};

RangeMemo range_memo[RANGE_MEMO_SLOTS] = {};

constexpr int TRACK_MAX = 32;

struct ProjTrack
{
    int32_t   pid;
    uintptr_t data;
    int32_t   x, y;
    int32_t   lx, ly;
    int32_t   dx, dy;
    int32_t   seg0;
    uint8_t   active;
    uint8_t   seen_now;
};

ProjTrack proj_track[TRACK_MAX] = {};
int       track_next            = 0;

struct Threat
{
    int32_t x, y, vx, vy, rad;
    int32_t pid;
};

Threat  threats[THREAT_MAX];
int     threat_n = 0;
int32_t hitbox   = 0;

struct FrameGeom
{
    float w, h, em;
};

FrameGeom frame_geom;

void cache_frame_geom()
{
    frame_geom.w  = static_cast<float>(wall_logic_units(wall_cache_width()));
    frame_geom.h  = static_cast<float>(wall_logic_units(wall_cache_height()));
    frame_geom.em = static_cast<float>(hitbox + span.pad);
}

int body_subtiles(int32_t radius)
{
    return radius > WALL_BODY_SUBTILE_UNITS ? 2 : 1;
}

constexpr int PROJ_MEMO_SLOTS = 64;

struct ProjMemo
{
    uintptr_t data;
    int32_t   speed;
    int32_t   radius;
    uint8_t   ignores_walls;
};

ProjMemo proj_memo[PROJ_MEMO_SLOTS] = {};

bool proj_info(uintptr_t data, ProjMemo* out)
{
    ProjMemo& entry = proj_memo[(data >> 4) & (PROJ_MEMO_SLOTS - 1)];
    if (entry.data == data)
    {
        *out = entry;
        return true;
    }
    int32_t speed  = 0;
    int32_t radius = 0;
    uint8_t pass   = 0;
    uint8_t pierce = 0;
    uint8_t butter = 0;
    if (!read_int(data + LOGICPROJECTILEDATA__GETSPEED, &speed)) return false;
    if (!read_int(data + LOGICPROJECTILEDATA__GETRADIUS, &radius)) return false;
    if (!read_bytes(data + LOGICPROJECTILEDATA__PASSESENVIRONMENT, &pass, 1)) return false;
    if (!read_bytes(data + LOGICPROJECTILEDATA__PIERCESENVIRONMENT, &pierce, 1)) return false;
    if (!read_bytes(data + LOGICPROJECTILEDATA__PIERCESENVIRONMENTLIKEBUTTER, &butter, 1))
        return false;
    entry.data          = data;
    entry.speed         = speed;
    entry.radius        = radius;
    entry.ignores_walls = (pass || pierce || butter) ? 1 : 0;
    *out                = entry;
    return true;
}

uintptr_t cc_data = 0;
int32_t   cc_r    = 0;
uintptr_t cc_fn   = 0;

int32_t character_collision_radius(uintptr_t data)
{
    if (data == cc_data) return cc_r;
    if (!cc_fn)
    {
        cc_fn = callable(LOGICCHARACTERDATA__GETCOLLISIONRADIUS);
        if (!cc_fn) return 0;
    }
    cc_data = data;
    cc_r    = reinterpret_cast<int32_t (*)(uintptr_t)>(cc_fn)(data);
    return cc_r;
}

int32_t learned_flight_range(uintptr_t data)
{
    if (!data) return 0;
    int i = static_cast<int>((data >> 4) & (RANGE_MEMO_SLOTS - 1));
    for (int n = 0; n < RANGE_MEMO_SLOTS; n++)
    {
        const RangeMemo& e = range_memo[i];
        if (e.data == data) return e.range;
        if (!e.data) return 0;
        i = (i + 1) & (RANGE_MEMO_SLOTS - 1);
    }
    return 0;
}

void learn_flight_range(uintptr_t data, int32_t range)
{
    if (!data || range <= 0) return;
    int i = static_cast<int>((data >> 4) & (RANGE_MEMO_SLOTS - 1));
    for (int n = 0; n < RANGE_MEMO_SLOTS; n++)
    {
        RangeMemo& e = range_memo[i];
        if (e.data == data)
        {
            if (range > e.range) e.range = range;
            return;
        }
        if (!e.data)
        {
            e.data  = data;
            e.range = range;
            return;
        }
        i = (i + 1) & (RANGE_MEMO_SLOTS - 1);
    }
}

void learn_skill_range(uintptr_t sd)
{
    if (!sd) return;
    uintptr_t row = 0;
    if (!read_ptr(sd + LOGICSKILLDATA__ROWREF, &row)) return;
    if (!row) return;
    uintptr_t table = 0;
    if (!read_ptr(row + LOGICCSVROW__TABLE, &table)) return;
    if (!table) return;
    int32_t col = 0;
    if (!read_int(base + LOGICCSVTABLE__CASTINGRANGECOLGLOBAL, &col)) return;
    if (col < 0 || col > 0xfff) return;
    uintptr_t columns = 0;
    if (!read_ptr(table + LOGICCSVTABLE__COLUMNS, &columns)) return;
    if (!columns) return;
    int32_t row_index = 0;
    if (!read_int(row + LOGICCSVROW__ROWINDEX, &row_index)) return;
    if (row_index < 0 || row_index > 0xfffff) return;
    uintptr_t column = 0;
    if (!read_ptr(columns + static_cast<uintptr_t>(col) * 8, &column)) return;
    if (!column) return;
    uintptr_t values = 0;
    if (!read_ptr(column + LOGICCSVCOLUMN__VALUES, &values)) return;
    if (!values) return;
    int32_t tiles = 0;
    if (!read_int(values + static_cast<uintptr_t>(row_index) * 4, &tiles)) return;
    if (tiles == 0x7fffffff) return;
    if (tiles < 1 || tiles > WALL_MAX_MAP_TILES) return;
    uintptr_t proj = 0;
    if (!read_ptr(sd + LOGICSKILLDATA__PROJECTILE, &proj)) return;
    if (!proj) return;
    learn_flight_range(proj, tiles * RANGE_UNIT);
}

void learn_character_ranges(uintptr_t charData)
{
    if (!charData) return;
    uintptr_t skill = 0;
    if (!read_ptr(charData + LOGICCHARACTERDATA__WEAPONSKILL, &skill)) return;
    if (skill) learn_skill_range(skill);
    if (!read_ptr(charData + LOGICCHARACTERDATA__ULTISKILL, &skill)) return;
    if (skill) learn_skill_range(skill);
}

void begin_projectile_frame()
{
    for (auto& i : proj_track)
        i.seen_now = 0;
}

void sweep_dead_projectiles()
{
    for (auto& s : proj_track)
    {
        if (!s.pid || s.seen_now) continue;
        int32_t flown = game_vec2_length(s.lx - s.x, s.ly - s.y);
        learn_flight_range(s.data, flown);
        s.pid    = 0;
        s.data   = 0;
        s.active = 0;
    }
}

bool read_projectile_position(const uint8_t* obj, int32_t* px, int32_t* py)
{
    int32_t xi = 0;
    int32_t yi = 0;
    memcpy(&xi, obj + LOGICGAMEOBJECTCLIENT__GETX, sizeof(xi));
    memcpy(&yi, obj + LOGICGAMEOBJECTCLIENT__GETY, sizeof(yi));
    if (xi < -span.tile || yi < -span.tile || xi > static_cast<int32_t>(frame_geom.w) + span.tile ||
        yi > static_cast<int32_t>(frame_geom.h) + span.tile)
    {
        return false;
    }
    *px = xi;
    *py = yi;
    return true;
}

void character_entry(const uint8_t* obj, uintptr_t dataPtr, int32_t own_gid)
{
    int32_t hp = 0, gid = 0;
    memcpy(&hp, obj + LOGICGAMEOBJECTCLIENT__HP, sizeof(hp));
    memcpy(&gid, obj + LOGICGAMEOBJECTCLIENT__GETGLOBALID, sizeof(gid));
    if (hp <= 0 || gid <= 0 || gid == own_gid) return;
    if (owned_team_of(dataPtr) != 0) return;
    learn_character_ranges(dataPtr);
}

void projectile_threat(
    const uint8_t* obj, uintptr_t dataPtr, int32_t mx, int32_t my, int32_t px, int32_t py)
{
    int32_t pid = 0;
    memcpy(&pid, obj + LOGICGAMEOBJECTCLIENT__GETGLOBALID, sizeof(pid));
    if (pid <= 0) return;
    if (owned_team_of(dataPtr) != 0) return;
    ProjMemo info;
    if (!proj_info(dataPtr, &info)) return;
    int32_t sp = info.speed;
    if (sp < 0) return;
    if (sp == 0) return;
    ProjTrack* slot  = nullptr;
    int        limit = track_next < TRACK_MAX ? track_next : TRACK_MAX;
    for (int i = 0; i < limit; i++)
    {
        if (proj_track[i].pid == pid)
        {
            slot = &proj_track[i];
            break;
        }
    }
    int spawn = 0;
    if (!slot)
    {
        spawn = 1;
        slot  = &proj_track[track_next % TRACK_MAX];
        track_next++;
        slot->pid    = pid;
        slot->data   = dataPtr;
        slot->x      = px;
        slot->y      = py;
        slot->lx     = px;
        slot->ly     = py;
        slot->dx     = 0;
        slot->dy     = 0;
        slot->seg0   = 0;
        slot->active = 0;
    }
    slot->seen_now = 1;
    slot->lx       = px;
    slot->ly       = py;
    if (!slot->active)
    {
        int32_t dx = 0, dy = 0, seg = 0;
        int32_t tx = 0, ty = 0;
        memcpy(&tx, obj + LOGICPROJECTILECLIENT__TARGETX, sizeof(tx));
        memcpy(&ty, obj + LOGICPROJECTILECLIENT__TARGETY, sizeof(ty));
        int have = 0;
        if (tx != 0 || ty != 0)
        {
            int32_t ddx    = tx - px;
            int32_t ddy    = ty - py;
            int32_t travel = game_vec2_length(ddx, ddy);
            if (travel >= 1)
            {
                dx   = ddx;
                dy   = ddy;
                seg  = travel;
                have = 1;
            }
        }
        if (!have && !spawn)
        {
            int32_t ddx   = px - slot->x;
            int32_t ddy   = py - slot->y;
            int32_t flown = game_vec2_length(ddx, ddy);
            if (flown >= 1)
            {
                dx   = ddx;
                dy   = ddy;
                seg  = sp * SIM_T_NUM / SIM_T_DEN;
                have = 1;
            }
        }
        if (!have) return;
        slot->dx        = dx;
        slot->dy        = dy;
        slot->seg0      = seg;
        int32_t learned = learned_flight_range(dataPtr);
        if (learned > 0)
        {
            int32_t remain = learned + info.radius;
            if (seg > remain) seg = remain;
        }
        int32_t a_dir = game_get_angle(dx, dy);
        int32_t a_me  = game_get_angle(mx - px, my - py);
        int32_t diff  = game_normalize_angle_180(a_me - a_dir);
        if (diff > 90 || diff < -90) return;
        if (!info.ignores_walls)
        {
            int32_t capped = wall_free_run(px, py, px + dx, py + dy, body_subtiles(info.radius));
            if (capped < seg) seg = capped;
        }
        if (seg <= 0) return;
        int32_t rad    = info.radius + hitbox;
        int32_t vec[2] = {dx, dy};
        game_vec2_normalize(vec, seg);
        int32_t x2 = px + vec[0];
        int32_t y2 = py + vec[1];
        if (!game_segment_hits_circle(px, py, x2, y2, mx, my, rad, nullptr)) return;
        slot->active = 1;
    }
    else
    {
        int32_t flown   = game_vec2_length(px - slot->x, py - slot->y);
        int32_t seg     = slot->seg0 - flown;
        int32_t learned = learned_flight_range(dataPtr);
        if (learned > 0)
        {
            int32_t remain = learned + info.radius - flown;
            if (remain < seg) seg = remain;
        }
        int32_t rad   = info.radius + hitbox;
        int32_t reach = seg + rad;
        if (seg <= 0 || game_distance(px, py, mx, my) > reach)
        {
            slot->active = 0;
            return;
        }
    }
    if (threat_n >= THREAT_MAX) return;
    Threat& t      = threats[threat_n++];
    t.x            = px;
    t.y            = py;
    int32_t vec[2] = {slot->dx, slot->dy};
    game_vec2_normalize(vec, sp);
    t.vx  = vec[0];
    t.vy  = vec[1];
    t.pid = pid;
    t.rad = info.radius + hitbox;
}

void collect_logic_objects(int32_t own_gid, int32_t mx, int32_t my)
{
    for (int i = 0; i < obj_count; i++)
    {
        if (!valid[i]) continue;
        const uint8_t* obj     = block[i];
        uintptr_t      dataPtr = 0;
        memcpy(&dataPtr, obj + LOGICGAMEOBJECTCLIENT__GETDATA, sizeof(dataPtr));
        if (!dataPtr) continue;
        uintptr_t vt = 0;
        if (!read_ptr(dataPtr, &vt)) continue;
        if (vt < base) continue;
        if (vt == base + static_cast<uintptr_t>(LOGICPROJECTILEDATA__VTABLE)) continue;
        character_entry(obj, dataPtr, own_gid);
    }
    for (int i = 0; i < obj_count; i++)
    {
        if (!valid[i]) continue;
        const uint8_t* obj     = block[i];
        uintptr_t      dataPtr = 0;
        memcpy(&dataPtr, obj + LOGICGAMEOBJECTCLIENT__GETDATA, sizeof(dataPtr));
        if (!dataPtr) continue;
        uintptr_t vt = 0;
        if (!read_ptr(dataPtr, &vt)) continue;
        if (vt < base) continue;
        if (vt != base + static_cast<uintptr_t>(LOGICPROJECTILEDATA__VTABLE)) continue;
        int32_t px = 0, py = 0;
        if (!read_projectile_position(obj, &px, &py)) continue;
        projectile_threat(obj, dataPtr, mx, my, px, py);
    }
}

uint64_t sent_frame = 0;
float    tgt_x = 0.0f, tgt_y = 0.0f;
int32_t  last_tx = 0, last_ty = 0;
int      last_valid = 0;

float compute_free_run(float mx, float my, float dx, float dy)
{
    int32_t sx = static_cast<int32_t>(mx);
    int32_t sy = static_cast<int32_t>(my);
    int32_t ex = sx + static_cast<int32_t>(dx * static_cast<float>(span.tile));
    int32_t ey = sy + static_cast<int32_t>(dy * static_cast<float>(span.tile));
    return static_cast<float>(wall_free_run(sx, sy, ex, ey, body_subtiles(hitbox)));
}

void choose_dodge_direction(int32_t mx, int32_t my, float& out_x, float& out_y)
{
    float sx = 0.0f, sy = 0.0f;
    for (int i = 0; i < threat_n; i++)
    {
        const Threat& t   = threats[i];
        int32_t       rxi = mx - t.x;
        int32_t       ryi = my - t.y;
        float         d   = static_cast<float>(game_vec2_length(rxi, ryi));
        if (d < 1.0f) d = 1.0f;
        float w  = 1.0f / (d + static_cast<float>(t.rad) + 1.0f);
        float vl = static_cast<float>(game_vec2_length(t.vx, t.vy));
        if (vl > 1.0f)
        {
            int32_t u[2] = {t.vx, t.vy};
            game_vec2_normalize(u, STEER_SCALE);
            float   ux = static_cast<float>(u[0]) / STEER_SCALE_F;
            float   uy = static_cast<float>(u[1]) / STEER_SCALE_F;
            int32_t side =
                game_normalize_angle_180(game_get_angle(rxi, ryi) - game_get_angle(t.vx, t.vy)) >= 0
                    ? 1
                    : -1;
            sx += (-uy * side - ux * 0.35f) * w;
            sy += (ux * side - uy * 0.35f) * w;
        }
        else
        {
            int32_t r[2] = {rxi, ryi};
            if (d <= 1.0f && rxi == 0 && ryi == 0)
            {
                r[0] = STEER_SCALE;
                r[1] = 0;
            }
            else
            {
                game_vec2_normalize(r, STEER_SCALE);
            }
            sx += (static_cast<float>(r[0]) / STEER_SCALE_F) * w;
            sy += (static_cast<float>(r[1]) / STEER_SCALE_F) * w;
        }
    }
    if (sx * sx + sy * sy < 1e-12f)
    {
        out_x = 0.0f;
        out_y = 0.0f;
        return;
    }
    int32_t v[2] = {static_cast<int32_t>(sx * STEER_SCALE_F),
                    static_cast<int32_t>(sy * STEER_SCALE_F)};
    game_vec2_normalize(v, STEER_SCALE);
    out_x = static_cast<float>(v[0]) / STEER_SCALE_F;
    out_y = static_cast<float>(v[1]) / STEER_SCALE_F;
}

void reset_dodge_state()
{
    tgt_x = 0.0f, tgt_y = 0.0f;
    last_tx    = 0;
    last_ty    = 0;
    last_valid = 0;
}

bool learn_own_hitbox()
{
    void* data = nullptr;
    if (!read_ptr(elem + LOGICGAMEOBJECTCLIENT__GETDATA, &data)) return false;
    int32_t r = character_collision_radius(reinterpret_cast<uintptr_t>(data));
    if (r <= 0) return false;
    hitbox = r;
    return true;
}
}

void reset_autododge_state()
{
    threat_n = 0;
    memset(range_memo, 0, sizeof(range_memo));
    track_next = 0;
    hitbox     = 0;
    cc_data    = 0;
    cc_r       = 0;
    memset(proj_memo, 0, sizeof(proj_memo));
    sent_frame = 0;
    reset_dodge_state();
}

void update_autododge()
{
    threat_n = 0;
    if (!base || !elem || dead || !coord_ok)
    {
        reset_dodge_state();
        return;
    }
    if (!learn_own_hitbox())
    {
        reset_dodge_state();
        return;
    }
    if (wall_cache_width() <= 0 || wall_cache_height() <= 0)
    {
        reset_dodge_state();
        return;
    }
    int32_t own_gid = 0;
    if (own_block)
    {
        memcpy(&own_gid, own_block + LOGICGAMEOBJECTCLIENT__GETGLOBALID, sizeof(own_gid));
    }
    cache_span();
    cache_frame_geom();
    int32_t mx = own_x, my = own_y;
    begin_projectile_frame();
    collect_logic_objects(own_gid, mx, my);
    sweep_dead_projectiles();
    if (!threat_n)
    {
        reset_dodge_state();
        return;
    }
    float dir_x = 0.0f, dir_y = 0.0f;
    choose_dodge_direction(mx, my, dir_x, dir_y);
    if (dir_x == 0.0f && dir_y == 0.0f)
    {
        reset_dodge_state();
        return;
    }
    float run = compute_free_run(static_cast<float>(mx), static_cast<float>(my), dir_x, dir_y);
    float fx  = static_cast<float>(mx) + dir_x * run;
    float fy  = static_cast<float>(my) + dir_y * run;
    float em  = frame_geom.em;
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
