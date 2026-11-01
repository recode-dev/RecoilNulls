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
constexpr float DEG2RAD     = 0.01745329252f;

struct Span
{
    float tile, pad, far;
};

Span span = {0.0f, 0.0f, 0.0f};

void cache_span()
{
    span.tile = wall_logic_units(1);
    span.pad  = span.tile * 0.2f;
    float wu  = wall_logic_units(wall_cache_width());
    float hu  = wall_logic_units(wall_cache_height());
    span.far  = sqrtf(wu * wu + hu * hu);
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
    int32_t   pid;
    uintptr_t data;
    float     x, y;
    float     lx, ly;
    float     ux, uy;
    float     seg0;
    uint8_t   active;
    uint8_t   seen_now;
};

ProjTrack proj_track[TRACK_MAX] = {};
int       track_next            = 0;

constexpr int SHOOTER_MAX = 16;

struct Shooter
{
    float   x, y;
    int32_t angle;
};

Shooter shooters[SHOOTER_MAX] = {};
int     shooter_n             = 0;

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
    if (tiles < 1 || tiles > 3000) return;
    float     range = static_cast<float>(tiles) * CASTING_RANGE_CELL;
    uintptr_t proj  = 0;
    if (!read_ptr(sd + LOGICSKILLDATA__PROJECTILE, &proj) || !pointer_plausible(proj)) return;
    learn_flight_range(proj, range);
}

void learn_character_ranges(uintptr_t charData)
{
    if (!charData || !pointer_plausible(charData)) return;
    uintptr_t skill = 0;
    if (read_ptr(charData + LOGICCHARACTERDATA__WEAPONSKILL, &skill)) learn_skill_range(skill);
    if (read_ptr(charData + LOGICCHARACTERDATA__ULTISKILL, &skill)) learn_skill_range(skill);
}

bool shooter_direction(float px, float py, float& ux, float& uy)
{
    float   best2 = 160000.0f;
    int32_t angle = -1;
    for (int i = 0; i < shooter_n; i++)
    {
        if (shooters[i].angle < 0 || shooters[i].angle > 511) continue;
        float dx = shooters[i].x - px;
        float dy = shooters[i].y - py;
        float d2 = dx * dx + dy * dy;
        if (d2 < best2)
        {
            best2 = d2;
            angle = shooters[i].angle;
        }
    }
    if (angle < 0) return false;
    float rad = static_cast<float>(angle) * DEG2RAD;
    ux        = cosf(rad);
    uy        = sinf(rad);
    return true;
}

void begin_projectile_frame()
{
    shooter_n = 0;
    for (auto& i : proj_track)
        i.seen_now = 0;
}

void sweep_dead_projectiles()
{
    for (auto& s : proj_track)
    {
        if (!s.pid || s.seen_now) continue;
        float flown = fast_len(s.lx - s.x, s.ly - s.y);
        learn_flight_range(s.data, flown);
        s.pid    = 0;
        s.data   = 0;
        s.active = 0;
    }
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
    if (shooter_n < SHOOTER_MAX)
    {
        int32_t xi = 0, yi = 0, angle = 0;
        memcpy(&xi, obj + LOGICGAMEOBJECTCLIENT__GETX, sizeof(xi));
        memcpy(&yi, obj + LOGICGAMEOBJECTCLIENT__GETY, sizeof(yi));
        memcpy(&angle, obj + LOGICCHARACTERCLIENT__ATTACKANGLE, sizeof(angle));
        Shooter& s = shooters[shooter_n++];
        s.x        = static_cast<float>(xi);
        s.y        = static_cast<float>(yi);
        s.angle    = angle;
    }
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
        slot->ux     = 0.0f;
        slot->uy     = 0.0f;
        slot->seg0   = 0.0f;
        slot->active = 0;
    }
    slot->seen_now = 1;
    slot->lx       = px;
    slot->ly       = py;
    if (spawn)
    {
        float   dx = 0.0f, dy = 0.0f, seg = 0.0f;
        int32_t tx = 0, ty = 0;
        memcpy(&tx, obj + LOGICPROJECTILECLIENT__TARGETX, sizeof(tx));
        memcpy(&ty, obj + LOGICPROJECTILECLIENT__TARGETY, sizeof(ty));
        if (tx != 0 || ty != 0)
        {
            dx           = static_cast<float>(tx) - px;
            dy           = static_cast<float>(ty) - py;
            float travel = fast_len(dx, dy);
            if (travel < 1.0f) return;
            float inv = 1.0f / travel;
            dx *= inv;
            dy *= inv;
            seg = travel;
        }
        else
        {
            if (!shooter_direction(px, py, dx, dy)) return;
            seg = sp * SIM_T;
        }
        slot->ux      = dx;
        slot->uy      = dy;
        slot->seg0    = seg;
        float learned = learned_flight_range(dataPtr);
        if (learned > 0.0f)
        {
            float remain = learned + info.radius;
            if (seg > remain) seg = remain;
        }
        float rx = px - mx, ry = py - my;
        if (rx * rx + ry * ry > zone2) return;
        if ((mx - px) * slot->ux + (my - py) * slot->uy < 0.0f) return;
        if (!info.ignores_walls)
        {
            float capped = wall_clear_run(px, py, slot->ux, slot->uy, seg, 0.0f);
            if (capped < seg) seg = capped;
        }
        if (seg <= 0.0f) return;
        float   rad  = info.radius + hitbox;
        int32_t sx1  = static_cast<int>(px);
        int32_t sy1  = static_cast<int>(py);
        int32_t sx2  = static_cast<int>(px + slot->ux * seg);
        int32_t sy2  = static_cast<int>(py + slot->uy * seg);
        int32_t scx  = static_cast<int>(mx);
        int32_t scy  = static_cast<int>(my);
        int32_t srad = static_cast<int>(rad + 0.5f);
        if (!game_segment_hits_circle(sx1, sy1, sx2, sy2, scx, scy, srad, nullptr)) return;
        slot->active = 1;
    }
    else
    {
        if (!slot->active) return;
        float flown   = fast_len(px - slot->x, py - slot->y);
        float seg     = slot->seg0 - flown;
        float learned = learned_flight_range(dataPtr);
        if (learned > 0.0f)
        {
            float remain = learned + info.radius - flown;
            if (remain < seg) seg = remain;
        }
        float rad   = info.radius + hitbox;
        float reach = seg + rad;
        float rdx   = px - mx;
        float rdy   = py - my;
        if (seg <= 0.0f || rdx * rdx + rdy * rdy > reach * reach)
        {
            slot->active = 0;
            return;
        }
    }
    if (threat_n >= THREAT_MAX) return;
    Threat& t = threats[threat_n++];
    t.x = px, t.y = py, t.vx = slot->ux * sp, t.vy = slot->uy * sp, t.pid = pid;
    t.rad = info.radius + hitbox;
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
        if (vt == base + static_cast<uintptr_t>(LOGICPROJECTILEDATA__VTABLE)) continue;
        character_entry(obj, dataPtr, own_gid);
    }
    for (int i = 0; i < obj_count; i++)
    {
        if (!valid[i]) continue;
        const uint8_t* obj     = block[i];
        uintptr_t      dataPtr = 0;
        memcpy(&dataPtr, obj + LOGICGAMEOBJECTCLIENT__GETDATA, sizeof(dataPtr));
        if (!dataPtr || !pointer_plausible(dataPtr)) continue;
        uintptr_t vt = 0;
        if (!read_ptr(dataPtr, &vt) || vt < base) continue;
        if (vt != base + static_cast<uintptr_t>(LOGICPROJECTILEDATA__VTABLE)) continue;
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
    shooter_n  = 0;
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
