#include "autododge.h"
#include "../core/commands.h"
#include "../core/scan.h"
#include "../utils/walls.h"
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
float fast_len(float x, float y)
{
    return sqrtf(x * x + y * y);
}

constexpr float TARGET_LERP = 0.5f;
constexpr float SIM_T       = 1.6f;

struct Span
{
    float tile, pad, far, margin;
};

Span span = {0.0f, 0.0f, 0.0f, 0.0f};

void cache_span()
{
    span.tile   = wall_logic_units(1);
    span.pad    = span.tile * 0.2f;
    span.far    = span.tile * 16.0f;
    span.margin = span.tile * 0.05f;
}

constexpr int THREAT_MAX = 32;

constexpr int RANGE_MEMO_SLOTS = 256;

struct RangeMemo
{
    uintptr_t data;
    float     range;
};

RangeMemo range_memo[RANGE_MEMO_SLOTS] = {};

constexpr int TRACK_MAX = 32;

struct ProjTrack
{
    int32_t pid;
    float   x, y;
    float   ax, ay;
    float   lx, ly;
    float   ux, uy;
    uint8_t has_dir;
    uint8_t seen;
};

ProjTrack proj_track[TRACK_MAX] = {};
int       track_next            = 0;

constexpr float DIR_MIN_STEP = 12.0f;

struct Threat
{
    float   x, y, vx, vy, rad;
    int32_t pid;
};

Threat threats[THREAT_MAX];
int    threat_n = 0;
float  hitbox   = 0.0f;

struct FrameGeom
{
    float w, h, em;
};

FrameGeom frame_geom;

void cache_frame_geom()
{
    frame_geom.w  = wall_logic_units(wall_cache_width());
    frame_geom.h  = wall_logic_units(wall_cache_height());
    frame_geom.em = hitbox + span.pad;
}

constexpr int PROJ_MEMO_SLOTS = 64;

struct ProjMemo
{
    uintptr_t data;
    float     speed;
    float     radius;
    uint8_t   ignores_walls;
};

ProjMemo proj_memo[PROJ_MEMO_SLOTS] = {};

const ProjMemo& proj_info(uintptr_t data)
{
    ProjMemo& entry = proj_memo[(data >> 4) & (PROJ_MEMO_SLOTS - 1)];
    if (entry.data == data) return entry;
    int32_t speed  = 0;
    int32_t radius = 0;
    uint8_t pass   = 0;
    uint8_t pierce = 0;
    uint8_t butter = 0;
    read_int(data + LOGICPROJECTILEDATA__GETSPEED, &speed);
    read_int(data + LOGICPROJECTILEDATA__GETRADIUS, &radius);
    read_bytes(data + LOGICPROJECTILEDATA__PASSESENVIRONMENT, &pass, 1);
    read_bytes(data + LOGICPROJECTILEDATA__PIERCESENVIRONMENT, &pierce, 1);
    read_bytes(data + LOGICPROJECTILEDATA__PIERCESENVIRONMENTLIKEBUTTER, &butter, 1);
    entry.data          = data;
    entry.speed         = static_cast<float>(speed);
    entry.radius        = static_cast<float>(radius);
    entry.ignores_walls = (pass || pierce || butter) ? 1 : 0;
    return entry;
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
        if (!cc_fn) __builtin_trap();
    }
    cc_data = data;
    cc_r    = reinterpret_cast<int32_t (*)(uintptr_t)>(cc_fn)(data);
    return cc_r;
}

float learned_flight_range(uintptr_t data)
{
    if (!data) return 0.0f;
    int i = static_cast<int>((data >> 4) & (RANGE_MEMO_SLOTS - 1));
    for (int n = 0; n < RANGE_MEMO_SLOTS; n++)
    {
        const RangeMemo& e = range_memo[i];
        if (e.data == data) return e.range;
        if (!e.data) return 0.0f;
        i = (i + 1) & (RANGE_MEMO_SLOTS - 1);
    }
    return 0.0f;
}

void learn_flight_range(uintptr_t data, float range)
{
    if (!data || range <= 0.0f) return;
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
    if (!sd || !pointer_plausible(sd)) return;
    constexpr float CASTING_RANGE_CELL = 100.0f;
    uintptr_t       row                = 0;
    if (!read_ptr(sd + LOGICSKILLDATA__ROWREF, &row) || !pointer_plausible(row)) return;
    uintptr_t table = 0;
    if (!read_ptr(row + LOGICCSVROW__TABLE, &table) || !pointer_plausible(table)) return;
    int32_t col = 0;
    if (!read_int(base + LOGICCSVTABLE__CASTINGRANGECOLGLOBAL, &col) || col < 0 || col > 0xfff)
        return;
    uintptr_t columns = 0;
    if (!read_ptr(table + LOGICCSVTABLE__COLUMNS, &columns) || !pointer_plausible(columns)) return;
    int32_t row_index = 0;
    if (!read_int(row + LOGICCSVROW__ROWINDEX, &row_index) || row_index < 0 || row_index > 0xfffff)
        return;
    uintptr_t column = 0;
    if (!read_ptr(columns + static_cast<uintptr_t>(col) * 8, &column) || !pointer_plausible(column))
        return;
    uintptr_t values = 0;
    if (!read_ptr(column + LOGICCSVCOLUMN__VALUES, &values) || !pointer_plausible(values)) return;
    int32_t tiles = 0;
    if (!read_int(values + static_cast<uintptr_t>(row_index) * 4, &tiles)) return;
    if (tiles == 0x7fffffff) tiles = 0;
    if (tiles < 1 || tiles > 700) return;
    float   range = static_cast<float>(tiles) * CASTING_RANGE_CELL;
    int32_t count = 0;
    if (!read_int(sd + LOGICSKILLDATA__PROJECTILECOUNT, &count) || count < 1) return;
    if (count > 8) count = 8;
    uintptr_t array = 0;
    if (!read_ptr(sd + LOGICSKILLDATA__PROJECTILEARRAY, &array) || !pointer_plausible(array))
        return;
    for (int32_t i = 0; i < count; i++)
    {
        uintptr_t proj = 0;
        if (!read_ptr(array + static_cast<uintptr_t>(i) * 8, &proj)) continue;
        if (proj && pointer_plausible(proj)) learn_flight_range(proj, range);
    }
}

void learn_character_ranges(uintptr_t charData)
{
    if (!charData || !pointer_plausible(charData)) return;
    uintptr_t skill = 0;
    if (read_ptr(charData + LOGICCHARACTERDATA__WEAPONSKILL, &skill)) learn_skill_range(skill);
    if (read_ptr(charData + LOGICCHARACTERDATA__ULTISKILL, &skill)) learn_skill_range(skill);
}

bool read_projectile_position(const uint8_t* obj, float* px, float* py)
{
    int32_t xi = 0;
    int32_t yi = 0;
    memcpy(&xi, obj + LOGICGAMEOBJECTCLIENT__GETX, sizeof(xi));
    memcpy(&yi, obj + LOGICGAMEOBJECTCLIENT__GETY, sizeof(yi));
    float x = static_cast<float>(xi);
    float y = static_cast<float>(yi);
    if (x < -span.tile || y < -span.tile || x > frame_geom.w + span.tile ||
        y > frame_geom.h + span.tile)
    {
        return false;
    }
    *px = x;
    *py = y;
    return true;
}

void character_entry(const uint8_t* obj, uintptr_t dataPtr, int32_t own_gid)
{
    int32_t hp = 0, gid = 0;
    memcpy(&hp, obj + LOGICGAMEOBJECTCLIENT__HP, sizeof(hp));
    memcpy(&gid, obj + LOGICGAMEOBJECTCLIENT__GETGLOBALID, sizeof(gid));
    if (hp <= 0 || gid <= 0 || gid == own_gid) return;
    if (is_owned_team_object(dataPtr)) return;
    learn_character_ranges(dataPtr);
}

void projectile_threat(
    const uint8_t* obj, uintptr_t dataPtr, float mx, float my, float zone2, float px, float py)
{
    int32_t pid = 0;
    memcpy(&pid, obj + LOGICGAMEOBJECTCLIENT__GETGLOBALID, sizeof(pid));
    if (pid <= 0) return;
    if (is_owned_team_object(dataPtr)) return;
    const ProjMemo& info = proj_info(dataPtr);
    float           sp   = info.speed;
    if (sp <= 0.0f || sp > 10000.0f) return;
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
    if (!slot)
    {
        slot = &proj_track[track_next % TRACK_MAX];
        track_next++;
        slot->pid     = pid;
        slot->x       = px;
        slot->y       = py;
        slot->ax      = px;
        slot->ay      = py;
        slot->lx      = px;
        slot->ly      = py;
        slot->ux      = 0.0f;
        slot->uy      = 0.0f;
        slot->has_dir = 0;
        slot->seen    = 0;
    }
    float step_x = px - slot->lx;
    float step_y = py - slot->ly;
    float step   = fast_len(step_x, step_y);
    if (slot->seen && step < 2.0f) return;
    slot->lx   = px;
    slot->ly   = py;
    slot->seen = 1;
    if (step >= DIR_MIN_STEP)
    {
        float inv     = 1.0f / step;
        slot->ux      = step_x * inv;
        slot->uy      = step_y * inv;
        slot->ax      = px;
        slot->ay      = py;
        slot->has_dir = 1;
    }
    int32_t tx = 0, ty = 0;
    memcpy(&tx, obj + LOGICPROJECTILECLIENT__TARGETX, sizeof(tx));
    memcpy(&ty, obj + LOGICPROJECTILECLIENT__TARGETY, sizeof(ty));
    if (slot->has_dir && (tx != 0 || ty != 0))
    {
        float ddx = static_cast<float>(tx) - px, ddy = static_cast<float>(ty) - py;
        float dl = fast_len(ddx, ddy);
        if (dl >= 1.0f)
        {
            float align = (ddx / dl) * slot->ux + (ddy / dl) * slot->uy;
            if (align < 0.6f)
            {
                tx = 0;
                ty = 0;
            }
        }
    }
    float ux = 0.0f, uy = 0.0f, seg = 0.0f;
    float flown   = fast_len(px - slot->x, py - slot->y);
    float learned = learned_flight_range(dataPtr);
    if (tx != 0 || ty != 0)
    {
        float dx = static_cast<float>(tx) - px, dy = static_cast<float>(ty) - py;
        float travel = fast_len(dx, dy);
        if (travel < 1.0f) return;
        float inv = 1.0f / travel;
        ux = dx * inv, uy = dy * inv;
        if (learned > 0.0f)
        {
            float remain = learned + info.radius - flown;
            if (remain <= 0.0f) return;
            if (travel > remain) travel = remain;
        }
        seg = travel < sp * SIM_T ? travel : sp * SIM_T;
    }
    else if (slot->has_dir)
    {
        if (learned <= 0.0f) return;
        ux = slot->ux, uy = slot->uy;
        float remain = learned + info.radius - flown;
        if (remain <= 0.0f) return;
        seg = remain < sp * SIM_T ? remain : sp * SIM_T;
    }
    else
    {
        return;
    }
    float rx = px - mx, ry = py - my;
    if (rx * rx + ry * ry > zone2) return;
    if ((mx - px) * ux + (my - py) * uy < 0.0f) return;
    if (!info.ignores_walls)
    {
        float capped = wall_clear_run(px, py, ux, uy, seg, 0.0f);
        if (capped < seg) seg = capped;
    }
    if (seg <= 0.0f) return;
    float   rad  = info.radius + hitbox + span.margin;
    int32_t sx1  = static_cast<int>(px);
    int32_t sy1  = static_cast<int>(py);
    int32_t sx2  = static_cast<int>(px + ux * seg);
    int32_t sy2  = static_cast<int>(py + uy * seg);
    int32_t scx  = static_cast<int>(mx);
    int32_t scy  = static_cast<int>(my);
    int32_t srad = static_cast<int>(rad + 0.5f);
    if (!game_segment_hits_circle(sx1, sy1, sx2, sy2, scx, scy, srad, nullptr)) return;
    if (threat_n >= THREAT_MAX) return;
    Threat& t = threats[threat_n++];
    t.x = px, t.y = py, t.vx = ux * sp, t.vy = uy * sp, t.pid = pid;
    t.rad = rad;
}

void collect_logic_objects(int32_t own_gid, float mx, float my)
{
    float zone  = span.far;
    float zone2 = zone * zone;
    for (int i = 0; i < obj_count; i++)
    {
        if (!valid[i]) continue;
        const uint8_t* obj     = block[i];
        uintptr_t      dataPtr = 0;
        memcpy(&dataPtr, obj + LOGICGAMEOBJECTCLIENT__GETDATA, sizeof(dataPtr));
        if (!dataPtr || !pointer_plausible(dataPtr)) continue;
        uintptr_t vt = 0;
        if (!read_ptr(dataPtr, &vt) || vt < base) continue;
        if (vt != base + static_cast<uintptr_t>(LOGICPROJECTILEDATA__VTABLE))
        {
            character_entry(obj, dataPtr, own_gid);
            continue;
        }
        float px = 0.0f, py = 0.0f;
        if (!read_projectile_position(obj, &px, &py)) continue;
        projectile_threat(obj, dataPtr, mx, my, zone2, px, py);
    }
}

uint64_t sent_frame = 0;
float    tgt_x = 0.0f, tgt_y = 0.0f;
int32_t  last_tx = 0, last_ty = 0;
int      last_valid = 0;

float compute_free_run(float mx, float my, float dx, float dy)
{
    float run = span.tile;
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
    return wall_clear_run(mx, my, dx, dy, run, hitbox);
}

void choose_dodge_direction(float mx, float my, float& out_x, float& out_y)
{
    float sx = 0.0f, sy = 0.0f;
    for (int i = 0; i < threat_n; i++)
    {
        const Threat& t  = threats[i];
        float         rx = mx - t.x;
        float         ry = my - t.y;
        float         d  = fast_len(rx, ry);
        if (d < 1.0f)
        {
            rx = 1.0f;
            ry = 0.0f;
            d  = 1.0f;
        }
        float w  = 1.0f / (d + t.rad + 1.0f);
        float vl = fast_len(t.vx, t.vy);
        if (vl > 1.0f)
        {
            float ux   = t.vx / vl;
            float uy   = t.vy / vl;
            float side = (ux * ry - uy * rx) >= 0.0f ? 1.0f : -1.0f;
            sx += (-uy * side - ux * 0.35f) * w;
            sy += (ux * side - uy * 0.35f) * w;
        }
        else
        {
            sx += (rx / d) * w;
            sy += (ry / d) * w;
        }
    }
    float len = fast_len(sx, sy);
    if (len < 1e-6f)
    {
        out_x = 0.0f;
        out_y = 0.0f;
        return;
    }
    float dx = sx / len;
    float dy = sy / len;
    if (compute_free_run(mx, my, dx, dy) < hitbox * 2.0f)
    {
        float lx    = -dy;
        float ly    = dx;
        float left  = compute_free_run(mx, my, lx, ly);
        float right = compute_free_run(mx, my, -lx, -ly);
        if (left >= right)
        {
            dx = lx;
            dy = ly;
        }
        else
        {
            dx = -lx;
            dy = -ly;
        }
    }
    out_x = dx;
    out_y = dy;
}

void reset_dodge_state()
{
    tgt_x = 0.0f, tgt_y = 0.0f;
    last_tx    = 0;
    last_ty    = 0;
    last_valid = 0;
}

void learn_own_hitbox()
{
    void* data = nullptr;
    if (!read_ptr(elem + LOGICGAMEOBJECTCLIENT__GETDATA, &data) ||
        !pointer_plausible((uintptr_t) data))
        return;
    int32_t r = character_collision_radius((uintptr_t) data);
    if (r <= 0) return;
    float tile = wall_logic_units(1);
    if (tile > 0.0f && static_cast<float>(r) >= tile) return;
    hitbox = static_cast<float>(r);
}
}

void reset_autododge_state()
{
    threat_n = 0;
    memset(range_memo, 0, sizeof(range_memo));
    track_next = 0;
    hitbox     = 0.0f;
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
    learn_own_hitbox();
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
    float mx = static_cast<float>(own_x), my = static_cast<float>(own_y);
    collect_logic_objects(own_gid, mx, my);
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
    float run = compute_free_run(mx, my, dir_x, dir_y);
    float fx  = mx + dir_x * run;
    float fy  = my + dir_y * run;
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
