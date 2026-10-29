#include "autododge.h"
#include "../core/commands.h"
#include "../core/scan.h"
#include "../core/scstring.h"
#include "../utils/walls.h"
#include <cmath>
#include <cstdint>
#include <cstring>
int active = 0, in_move = 0;
int ad_human = 0;
float ad_dir_x = 1.0f, ad_dir_y = 0.0f;
int ad_hold_ms = 0, ad_urgent = 0;
int ad_ind_n = 0;
int ad_pass_own = 0;
uint64_t ad_qv[16] = {0};
uint16_t ad_qc[16] = {0};
uint32_t ad_proj_sample = 0, ad_proj_move = 0;
int32_t ad_target_x = INT32_MAX, ad_target_y = INT32_MAX;
int ad_best_dir = -1;
int ad_char_n = 0, ad_proj_n = 0, ad_enemy_n = 0, ad_threat_n = 0;
int ad_threat_pid[AD_REPORT_MAX] = {0};
float ad_threat_x[AD_REPORT_MAX] = {0}, ad_threat_y[AD_REPORT_MAX] = {0};
float ad_threat_vx[AD_REPORT_MAX] = {0}, ad_threat_vy[AD_REPORT_MAX] = {0};
float ad_threat_r[AD_REPORT_MAX] = {0}, ad_threat_d[AD_REPORT_MAX] = {0};
float ad_threat_ttc[AD_REPORT_MAX] = {0}, ad_threat_ca[AD_REPORT_MAX] = {0};
char ad_threat_name[AD_REPORT_MAX][40] = {{0}};
int32_t ad_pj_pid[AD_REPORT_MAX] = {0};
float ad_pj_x[AD_REPORT_MAX] = {0}, ad_pj_y[AD_REPORT_MAX] = {0};
float ad_pj_vx[AD_REPORT_MAX] = {0}, ad_pj_vy[AD_REPORT_MAX] = {0};
float ad_pj_sp[AD_REPORT_MAX] = {0}, ad_pj_r[AD_REPORT_MAX] = {0};
int ad_pj_t0[AD_REPORT_MAX] = {0}, ad_pj_t1[AD_REPORT_MAX] = {0};
int ad_pj_vel[AD_REPORT_MAX] = {0};
int ad_shot_tick = 0;
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
constexpr float REACH = 300.0f;
constexpr int DIRS = 16;
constexpr int SIM_STEPS = 32;
constexpr float ZONE_FAR = 5000.0f;
constexpr uint64_t CALM_MS = 0;
constexpr uint64_t SEND_MS = ad::TICK_MS;
constexpr int MOVE_DELAY_TICKS = 2;
constexpr uint64_t TURN_HOLD_MS = 0;
constexpr int HOLD_DANGER_STEPS = (int)(TURN_HOLD_MS * 2 / ad::TICK_MS);
constexpr float AD_SAFETY = 40.0f;
constexpr uint64_t HUMAN_MS = 200;
constexpr float WEIGHT_STEP = 4096.0f;
constexpr float WEIGHT_POS = 900.0f;
constexpr float WEIGHT_AWAY = 0.06f;
constexpr float WEIGHT_AXIS = 2600.0f;
constexpr float AXIS_INTO = 0.35f;
constexpr float WEIGHT_APPROACH = 2000.0f;
constexpr float APPROACH_R = 2400.0f;
constexpr float HOT_CLEAR = 420.0f;
constexpr float CLOSE_R = 1200.0f;
constexpr float COMMIT_BONUS = 3.0f;
constexpr float BORDER_PAD = 60.0f;
constexpr float EDGE_ZONE = 300.0f;
constexpr float WEIGHT_EDGE = 1600.0f;
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
float own_hitbox = 0.0f;
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
    uint8_t data[PROJDATA_BLOCK_SIZE];
    int32_t radius = 0;
    if (!read_bytes(dataPtr, data, sizeof(data))) return;
    st->homing = false;
    st->ttype = 0;
    memcpy(&radius, data + PROJECTILEDATA_RADIUS_OFF, sizeof(radius));
    uintptr_t cfg = 0;
    memcpy(&cfg, data + PROJDATA_CFG_OFF, sizeof(cfg));
    if (cfg && pointer_plausible(cfg))
    {
        uint8_t cb[PROJDATA_CFG_BLOCK_SIZE];
        if (read_bytes(cfg, cb, sizeof(cb)))
        {
            int32_t cr = 0, ct = 0;
            uint8_t ch = 0;
            memcpy(&cr, cb + PROJDATA_RADIUS_OFF, sizeof(cr));
            memcpy(&ct, cb + PROJDATA_TTYPE_OFF, sizeof(ct));
            memcpy(&ch, cb + PROJDATA_HOMING_OFF, sizeof(ch));
            if (cr > 0 && cr <= 3000) radius = cr;
            st->homing = ch != 0;
            st->ttype = (int16_t)ct;
        }
    }
    if (radius < 0 || radius > 3000) radius = 60;
    st->pid = pid;
    st->seen = now;
    st->x = px;
    st->y = py;
    st->speed = 0.0f;
    st->rad = (float)radius;
    if (st->homing) st->rad += 80.0f;
    st->vel = false;
    st->ux = 0.0f;
    st->uy = 0.0f;
    st->flags = 0;
    if (data[PROJECTILEDATA_PASSES_ENV_OFF]) st->flags |= B_IND;
    if (data[PROJECTILEDATA_PIERCES_ENV_OFF] ||
        data[PROJECTILEDATA_PIERCES_ENV_BUTTER_OFF])
        st->flags |= B_PIERCE;
    if (data[PROJECTILEDATA_ISBOUNCING_OFF]) st->flags |= B_BOUNCE;
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
}

static void add_char(const uint8_t* obj, int32_t own_gid, float mx, float my)
{
    ad_char_n++;
    int32_t team, hp, gid, xi, yi, aim;
    memcpy(&team, obj + LOGICCHARACTER_TEAM_OFF, sizeof(team));
    memcpy(&hp, obj + LOGICCHARACTER_HP_OFF, sizeof(hp));
    memcpy(&gid, obj + LOGICCHARACTER_GLOBALID_OFF, sizeof(gid));
    memcpy(&xi, obj + LOGICCHARACTER_X_OFF, sizeof(xi));
    memcpy(&yi, obj + LOGICCHARACTER_Y_OFF, sizeof(yi));
    memcpy(&aim, obj + LOGICCHARACTER_ATTACK_ANGLE_OFF, sizeof(aim));
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

static float
seg_dist(float px, float py, float ax, float ay, float bx, float by)
{
    float vx = bx - ax, vy = by - ay;
    float wx = px - ax, wy = py - ay;
    float vv = vx * vx + vy * vy;
    float t = 0.0f;
    if (vv > 1e-6f)
    {
        t = (wx * vx + wy * vy) / vv;
        if (t < 0.0f)
            t = 0.0f;
        else if (t > 1.0f)
            t = 1.0f;
    }
    float dx = wx - vx * t, dy = wy - vy * t;
    return sqrtf(dx * dx + dy * dy);
}

static uintptr_t proj_data(uintptr_t objPtr)
{
    if (!objPtr) return 0;
    for (uintptr_t off = 0x8; off < 0x200; off += sizeof(uintptr_t))
    {
        uintptr_t v = 0, vt = 0;
        if (!read_ptr(objPtr + off, &v)) break;
        if (!v || !pointer_plausible(v)) continue;
        if (!read_ptr(v, &vt) || vt < base) continue;
        if (vt - base == VTABLE_PROJECTILEDATA_RVA) return v;
    }
    return 0;
}

static bool proj_shaped(const uint8_t* obj,
                        int32_t* tick0,
                        int32_t* tick1,
                        float* px,
                        float* py,
                        float* sx,
                        float* sy,
                        float* ex,
                        float* ey)
{
    int32_t t0 = 0, t1 = 0, xi = 0, yi = 0;
    int32_t ax = 0, ay = 0, bx = 0, by = 0;
    memcpy(&t0, obj + PROJSERVER_STARTTICK_OFF, sizeof(t0));
    memcpy(&t1, obj + PROJSERVER_ENDTICK_OFF, sizeof(t1));
    if (t0 <= 0 || t1 < t0 || t1 - t0 > 400) return false;
    float w = (float)wall_cache_w() * wall_units(1);
    float h = (float)wall_cache_h() * wall_units(1);
    if (w <= 0.0f || h <= 0.0f) return false;
    memcpy(&xi, obj + PROJSERVER_X_OFF, sizeof(xi));
    memcpy(&yi, obj + PROJSERVER_Y_OFF, sizeof(yi));
    if ((float)xi < -300.0f || (float)yi < -300.0f) return false;
    if ((float)xi > w + 300.0f || (float)yi > h + 300.0f) return false;
    memcpy(&ax, obj + PROJSERVER_STARTX_OFF, sizeof(ax));
    memcpy(&ay, obj + PROJSERVER_STARTY_OFF, sizeof(ay));
    memcpy(&bx, obj + PROJSERVER_ENDX_OFF, sizeof(bx));
    memcpy(&by, obj + PROJSERVER_ENDY_OFF, sizeof(by));
    if (ax < -100000 || ay < -100000 || ax > 100000 || ay > 100000)
        return false;
    if (bx < -100000 || by < -100000 || bx > 100000 || by > 100000)
        return false;
    if (ax == bx && ay == by) return false;
    if (seg_dist(
            (float)xi, (float)yi, (float)ax, (float)ay, (float)bx, (float)by) >
        600.0f)
        return false;
    *tick0 = t0;
    *tick1 = t1;
    *px = (float)xi;
    *py = (float)yi;
    *sx = (float)ax;
    *sy = (float)ay;
    *ex = (float)bx;
    *ey = (float)by;
    return true;
}

static void add_proj(const uint8_t* obj,
                     uintptr_t objPtr,
                     uint64_t now,
                     float mx,
                     float my,
                     float zone2,
                     int32_t tk0,
                     int32_t tk1,
                     float px,
                     float py,
                     float sx,
                     float sy,
                     float ex,
                     float ey)
{
    int32_t pid = 0;
    memcpy(&pid, obj + LOGICCHARACTER_GLOBALID_OFF, sizeof(pid));
    int32_t key = pid;
    if (key <= 0)
        key = (int32_t)(((objPtr >> 4) ^ (objPtr >> 20)) & 0x7fffffff);
    if (key <= 0) return;
    uintptr_t owner = 0;
    if (char_vt && read_ptr(objPtr + PROJSERVER_OWNER_OFF, &owner) && owner &&
        pointer_plausible(owner))
    {
        uintptr_t odata = 0, ovt = 0;
        int32_t oteam = 0;
        if (read_ptr(owner + LOGICGAMEOBJECT_DATA_OFF, &odata) && odata &&
            pointer_plausible(odata) && read_ptr(odata, &ovt) && ovt >= base &&
            ovt - base == char_vt &&
            read_int(owner + LOGICCHARACTER_TEAM_OFF, &oteam) &&
            oteam == own_team_b)
            return;
    }
    ad_proj_n++;
    Proj* st = find_proj(key);
    if (st->pid != key)
    {
        init_proj(st, key, proj_data(objPtr), px, py, now);
        if (st->pid != key) return;
    }
    st->seen = now;
    float step_x = px - st->x, step_y = py - st->y;
    float step = sqrtf(step_x * step_x + step_y * step_y);
    ad_proj_sample++;
    if (step > 1.0f) ad_proj_move++;
    float vps = 0.0f, vux = 0.0f, vuy = 0.0f;
    bool have = false;
    int32_t ticks = tk1 - tk0;
    if (ticks > 0 && ticks <= 400)
    {
        float ddx = ex - sx, ddy = ey - sy;
        float len = sqrtf(ddx * ddx + ddy * ddy);
        float cand = len / ((float)ticks * TICK_MS_SEC);
        if (len > 0.5f && cand > 50.0f && cand < 20000.0f)
        {
            vps = cand;
            vux = ddx / len;
            vuy = ddy / len;
            have = true;
        }
    }
    if (have)
    {
        st->ux = vux;
        st->uy = vuy;
        st->speed = vps;
        st->vel = true;
    }
    st->x = px, st->y = py;
    if (ad_proj_n <= AD_REPORT_MAX)
    {
        int k = ad_proj_n - 1;
        ad_pj_pid[k] = key;
        ad_pj_x[k] = px;
        ad_pj_y[k] = py;
        ad_pj_vx[k] = st->vel ? st->ux * st->speed : 0.0f;
        ad_pj_vy[k] = st->vel ? st->uy * st->speed : 0.0f;
        ad_pj_sp[k] = st->vel ? st->speed : 0.0f;
        ad_pj_r[k] = st->rad;
        ad_pj_t0[k] = tk0;
        ad_pj_t1[k] = tk1;
        ad_pj_vel[k] = st->vel ? 1 : 0;
    }
    float dx = px - mx, dy = py - my;
    if (!st->vel) return;
    float vx = st->ux * st->speed, vy = st->uy * st->speed;
    if (dx * dx + dy * dy > zone2) return;
    if (st->flags & B_IND) ad_ind_n++;
    if (!(st->flags & (B_IND | B_PIERCE)) && !wall_los(px, py, mx, my)) return;
    float vv = vx * vx + vy * vy;
    if (vv <= 0.0f) return;
    float rad = st->rad + own_hitbox + AD_SAFETY;
    if (st->flags & B_BOUNCE) rad += own_hitbox * 0.5f;
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
    t.x = px, t.y = py, t.vx = vx, t.vy = vy, t.pid = key;
    t.rad = rad;
    t.tt = tt;
    memcpy(t.name, st->name, sizeof(t.name));
    t.name[sizeof(t.name) - 1] = '\0';
}

static uintptr_t obj_data[COUNT_MAX];
static uintptr_t obj_rva[COUNT_MAX];

static uintptr_t proj_list_off = 0;

int ad_proj_list_off = 0;

static int proj_list(uintptr_t mgr, uintptr_t off, uintptr_t* items)
{
    uintptr_t p = 0;
    int32_t n = 0;
    if (!read_ptr(mgr + off, &p) || !p || !pointer_plausible(p)) return -1;
    if (!read_int(mgr + off + 0xc, &n) || n < 0 || n > 256) return -1;
    *items = p;
    return n;
}

void collect_world(uint64_t now, int32_t own_gid, float mx, float my)
{
    float zone = self_speed * 6.0f;
    if (zone < ZONE_FAR) zone = ZONE_FAR;
    float zone2 = zone * zone;
    uintptr_t mgr = manager_ptr;
    for (int i = 0; i < obj_count; i++)
    {
        obj_data[i] = 0;
        obj_rva[i] = 0;
        if (!obj_valid[i]) continue;
        const uint8_t* obj = obj_block[i];
        uintptr_t dataPtr = 0;
        memcpy(&dataPtr, obj + LOGICGAMEOBJECT_DATA_OFF, sizeof(dataPtr));
        uintptr_t vt = 0;
        if (dataPtr && pointer_plausible(dataPtr) && read_ptr(dataPtr, &vt) &&
            vt >= base)
        {
            obj_data[i] = dataPtr;
            obj_rva[i] = vt - base;
        }
        if (char_vt && obj_rva[i] == char_vt)
        {
            add_char(obj, own_gid, mx, my);
            continue;
        }
        int32_t t0 = 0, t1 = 0;
        float px = 0.0f, py = 0.0f, sx = 0.0f, sy = 0.0f, ex = 0.0f, ey = 0.0f;
        {
            int32_t nt0 = 0, nt1 = 0;
            memcpy(&nt0, obj + PROJSERVER_STARTTICK_OFF, sizeof(nt0));
            memcpy(&nt1, obj + PROJSERVER_ENDTICK_OFF, sizeof(nt1));
            if (nt0 > 0 && nt1 >= nt0 && nt1 - nt0 <= 400) ad_shot_tick++;
        }
        if (!proj_shaped(obj, &t0, &t1, &px, &py, &sx, &sy, &ex, &ey)) continue;
        add_proj(obj,
                 obj_snapshot[i],
                 now,
                 mx,
                 my,
                 zone2,
                 t0,
                 t1,
                 px,
                 py,
                 sx,
                 sy,
                 ex,
                 ey);
    }
    if (mgr && !proj_list_off)
    {
        static const uintptr_t cand[] = {
            0x40, 0x10, 0x20, 0x30, 0x50, 0x38, 0x48, 0x18, 0x28, 0x58};
        for (unsigned c = 0; c < sizeof(cand) / sizeof(cand[0]); c++)
        {
            uintptr_t items = 0;
            int n = proj_list(mgr, cand[c], &items);
            if (n <= 0) continue;
            uintptr_t first = 0;
            uint8_t buf[OBJ_BLOCK_SIZE];
            if (!read_ptr(items, &first) || !first) continue;
            if (!read_bytes(first, buf, sizeof(buf))) continue;
            int32_t t0 = 0, t1 = 0;
            float px = 0.0f, py = 0.0f, sx = 0.0f, sy = 0.0f, ex = 0.0f,
                  ey = 0.0f;
            if (!proj_shaped(buf, &t0, &t1, &px, &py, &sx, &sy, &ex, &ey))
                continue;
            proj_list_off = cand[c];
            break;
        }
        ad_proj_list_off = (int)proj_list_off;
    }
    if (mgr && proj_list_off)
    {
        uintptr_t items = 0;
        int n = proj_list(mgr, proj_list_off, &items);
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++)
        {
            uintptr_t op = 0;
            if (!read_ptr(items + (uintptr_t)i * sizeof(uintptr_t), &op) || !op)
                continue;
            uint8_t buf[OBJ_BLOCK_SIZE];
            if (!read_bytes(op, buf, sizeof(buf))) continue;
            int32_t nt0 = 0, nt1 = 0;
            memcpy(&nt0, buf + PROJSERVER_STARTTICK_OFF, sizeof(nt0));
            memcpy(&nt1, buf + PROJSERVER_ENDTICK_OFF, sizeof(nt1));
            if (nt0 > 0 && nt1 >= nt0 && nt1 - nt0 <= 400) ad_shot_tick++;
            int32_t t0 = 0, t1 = 0;
            float px = 0.0f, py = 0.0f, sx = 0.0f, sy = 0.0f, ex = 0.0f,
                  ey = 0.0f;
            if (!proj_shaped(buf, &t0, &t1, &px, &py, &sx, &sy, &ex, &ey))
                continue;
            add_proj(
                buf, op, now, mx, my, zone2, t0, t1, px, py, sx, sy, ex, ey);
        }
    }
}

int ad_find_shot(uintptr_t* objOut,
                 uintptr_t* dataOut,
                 int32_t* tick0,
                 int32_t* tick1,
                 float* px,
                 float* py)
{
    uintptr_t items = 0;
    int n = 0;
    if (manager_ptr && proj_list_off)
    {
        n = proj_list(manager_ptr, proj_list_off, &items);
        if (n > 64) n = 64;
    }
    for (int i = 0; i < n; i++)
    {
        uintptr_t op = 0;
        if (!read_ptr(items + (uintptr_t)i * sizeof(uintptr_t), &op) || !op)
            continue;
        uint8_t buf[OBJ_BLOCK_SIZE];
        if (!read_bytes(op, buf, sizeof(buf))) continue;
        int32_t t0 = 0, t1 = 0;
        float x = 0.0f, y = 0.0f, sx = 0.0f, sy = 0.0f, ex = 0.0f, ey = 0.0f;
        if (!proj_shaped(buf, &t0, &t1, &x, &y, &sx, &sy, &ex, &ey)) continue;
        *objOut = op;
        *dataOut = proj_data(op);
        *tick0 = t0;
        *tick1 = t1;
        *px = x;
        *py = y;
        return 1;
    }
    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i] || !obj_snapshot[i]) continue;
        int32_t t0 = 0, t1 = 0;
        float x = 0.0f, y = 0.0f, sx = 0.0f, sy = 0.0f, ex = 0.0f, ey = 0.0f;
        if (!proj_shaped(obj_block[i], &t0, &t1, &x, &y, &sx, &sy, &ex, &ey))
            continue;
        *objOut = obj_snapshot[i];
        *dataOut = proj_data(obj_snapshot[i]);
        *tick0 = t0;
        *tick1 = t1;
        *px = x;
        *py = y;
        return 1;
    }
    return 0;
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
static float move_dx = 0.0f, move_dy = 0.0f;
static uint64_t human_ms = 0;
static int32_t prev_x = 0, prev_y = 0;
static uint32_t last_hp = 0;
static float edge_margin()
{
    return own_hitbox + BORDER_PAD;
}
static float edge_pull(float mx, float my, float dx, float dy)
{
    float w = wall_units(wall_cache_w());
    float h = wall_units(wall_cache_h());
    float zone = own_hitbox + EDGE_ZONE;
    float pen = 0.0f;
    if (mx < zone) pen -= dx * (zone - mx) / zone;
    if (mx > w - zone) pen += dx * (mx - (w - zone)) / zone;
    if (my < zone) pen -= dy * (zone - my) / zone;
    if (my > h - zone) pen += dy * (my - (h - zone)) / zone;
    return pen;
}
static float free_run(float mx, float my, float dx, float dy, float far)
{
    float run = far;
    float em = edge_margin();
    float w = wall_units(wall_cache_w());
    float h = wall_units(wall_cache_h());
    if (dx > 1e-6f)
        run = fminf(run, (w - em - mx) / dx);
    else if (dx < -1e-6f)
        run = fminf(run, (em - mx) / dx);
    if (dy > 1e-6f)
        run = fminf(run, (h - em - my) / dy);
    else if (dy < -1e-6f)
        run = fminf(run, (em - my) / dy);
    const float step = wall_units(1);
    for (float d = step; d < run; d += step)
    {
        if (wall_is_blocked_wide(mx + dx * d, my + dy * d, own_hitbox))
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
    float delay = (float)MOVE_DELAY_TICKS * TICK_MS_SEC;
    float coast = tk < delay ? tk : delay;
    float em = edge_margin();
    ax = mx + move_dx * self_speed * coast;
    ay = my + move_dy * self_speed * coast;
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
static float enemy_approach(float mx, float my, float dx, float dy)
{
    float worst = 0.0f;
    for (int i = 0; i < ad_enemy_n; i++)
    {
        float ex = ad_enemy_x[i] - mx, ey = ad_enemy_y[i] - my;
        float d = sqrtf(ex * ex + ey * ey);
        if (d < 1.0f || d > APPROACH_R) continue;
        float dot = (dx * ex + dy * ey) / d;
        if (dot <= 0.0f) continue;
        float p = dot * (1.0f - d / APPROACH_R);
        if (d < CLOSE_R && dot > 0.5f) p = 3.0f;
        if (p > worst) worst = p;
    }
    return worst;
}
static float thrust_penalty(float mx, float my, float dx, float dy)
{
    float worst = 0.0f;
    for (int i = 0; i < threat_n; i++)
    {
        const Threat& t = threats[i];
        float vv = t.vx * t.vx + t.vy * t.vy;
        if (vv < 1e-6f) continue;
        float iv = 1.0f / sqrtf(vv);
        float ux = t.vx * iv, uy = t.vy * iv;
        float rx = mx - t.x, ry = my - t.y;
        if (rx * ux + ry * uy < 0.0f) continue;
        float a = dx * ux + dy * uy;
        float p = a > 0.0f ? a : -AXIS_INTO * a;
        if (p > worst) worst = p;
    }
    return worst;
}
static float dir_value(float mx, float my, float dx, float dy, int& steps)
{
    float far = self_speed * SIM_T;
    float run = free_run(mx, my, dx, dy, far);
    float room = run >= far ? 1.0f : run / far;
    float w = wall_units(wall_cache_w()) - edge_margin();
    float h = wall_units(wall_cache_h()) - edge_margin();
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
    if (margin > own_hitbox * 3.0f) margin = own_hitbox * 3.0f;
    steps = survive;
    return (float)survive * WEIGHT_STEP + margin + WEIGHT_POS * room +
           WEIGHT_POS * WEIGHT_AWAY * nearest_away(mx, my, dx, dy) -
           WEIGHT_AXIS * thrust_penalty(mx, my, dx, dy) -
           WEIGHT_APPROACH * enemy_approach(mx, my, dx, dy) -
           WEIGHT_EDGE * edge_pull(mx, my, dx, dy) -
           own_hitbox * wall_pull(mx, my, dx, dy, own_hitbox);
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
        if (has)
            v += own_hitbox * COMMIT_BONUS * (dx * commit_x + dy * commit_y);
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
    for (int k = -6; k <= 6; k++)
    {
        float ra = a + (float)k * 0.0174533f;
        float rdx = cosf(ra), rdy = sinf(ra);
        int steps = 0;
        float v = dir_value(mx, my, rdx, rdy, steps);
        if (has)
            v += own_hitbox * COMMIT_BONUS * (rdx * commit_x + rdy * commit_y);
        if (v > best)
        {
            best = v, a = ra, best_steps = steps;
        }
    }
    float bx = cosf(a), by = sinf(a);
    uint64_t now = us() / 1000;
    ad_urgent = 0;
    if (has)
    {
        int cur = 0;
        float cur_val = dir_value(mx, my, commit_x, commit_y, cur) +
                        own_hitbox * COMMIT_BONUS;
        bool better = best_steps > cur;
        bool die_fast = better && cur > 0 && cur <= HOLD_DANGER_STEPS;
        bool walled =
            better && free_run(mx, my, commit_x, commit_y, self_speed * SIM_T) <
                          own_hitbox * 2.0f;
        bool held = now - turn_ms < TURN_HOLD_MS;
        bool gain = best > cur_val + own_hitbox * 0.5f;
        if (die_fast || walled)
        {
            ad_urgent = 1;
            turn_ms = now;
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
        ad_hold_ms = since >= TURN_HOLD_MS ? 0 : (int)(TURN_HOLD_MS - since);
    }
    ad_dir_x = bx, ad_dir_y = by;
    ad_best_dir = ((int)lroundf(atan2f(by, bx) * 1.2732395f) + 8) & 7;
    commit_x = bx, commit_y = by;
    out_x = bx, out_y = by;
}
}

static void end_dodge()
{
    ad_target_x = INT32_MAX, ad_target_y = INT32_MAX;
    ad_dir_x = 0.0f, ad_dir_y = 0.0f;
    ad_hold_ms = 0;
    ad_urgent = 0;
    active = 0;
}

static void learn_vtable()
{
    void* data = nullptr;
    void* vt = nullptr;
    if (char_vt || !base || !own_elem) return;
    if (!read_ptr(own_elem + LOGICGAMEOBJECT_DATA_OFF, &data) || !data ||
        !pointer_plausible((uintptr_t)data))
        return;
    if (!read_ptr((uintptr_t)data, &vt) || !vt) return;
    uintptr_t rva = (uintptr_t)vt - base;
    if (rva == VTABLE_PROJECTILEDATA_RVA || rva < DATACONST_RVA_LO ||
        rva >= DATACONST_RVA_LO + DATACONST_RVA_SIZE)
        return;
    char_vt = rva;
}
static void learn_hitbox()
{
    void* data = nullptr;
    int32_t r = 0;
    if (!base || !own_elem) return;
    if (!read_ptr(own_elem + LOGICGAMEOBJECT_DATA_OFF, &data) || !data ||
        !pointer_plausible((uintptr_t)data))
        return;
    if (!read_bytes(
            (uintptr_t)data + LOGICCHARACTERDATA_COLLISION_OFF, &r, sizeof(r)))
        return;
    if (r <= 0) return;
    float tile = wall_units(1);
    if (tile > 0.0f && (float)r >= tile) return;
    own_hitbox = (float)r;
}
void autododge_reset()
{
    ad_best_dir = -1;
    ad_char_n = 0;
    ad_proj_n = 0;
    ad_enemy_n = 0;
    ad_threat_n = 0;
    ad_ind_n = 0;
    threat_n = 0;
    char_vt = 0;
    own_hitbox = 0.0f;
    last_hp = 0;
    pend_dmg = 0;
    sent_ms = 0;
    calm_ms = 0;
    turn_ms = 0;
    human_ms = 0;
    ad_human = 0;
    ad_dir_x = 1.0f, ad_dir_y = 0.0f;
    commit_x = 0.0f, commit_y = 0.0f;
    memset(projs, 0, sizeof(projs));
    end_dodge();
}
void autododge()
{
    ad_best_dir = -1;
    ad_char_n = ad_proj_n = ad_enemy_n = ad_threat_n = 0;
    ad_ind_n = 0;
    ad_shot_tick = 0;
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
    learn_hitbox();
    if (!char_vt || own_hitbox <= 0.0f || wall_cache_w() <= 0 ||
        wall_cache_h() <= 0)
    {
        end_dodge();
        return;
    }
    int32_t own_gid = 0;
    read_int(own_elem + LOGICCHARACTER_GLOBALID_OFF, &own_gid);
    self_speed = own_speed > 1.0f ? own_speed : 1.0f;
    uint64_t now = us() / 1000;
    {
        void* mgr = manager();
        uint64_t w[16] = {0};
        if (mgr && read_bytes((uintptr_t)mgr, w, sizeof(w)))
        {
            for (int i = 0; i < 16; i++)
            {
                if (ad_qv[i] == w[i]) continue;
                ad_qv[i] = w[i];
                if (ad_qc[i] < 60000) ad_qc[i]++;
            }
        }
    }
    float mx = (float)own_x, my = (float)own_y;
    evict_stale(now);
    collect_world(now, own_gid, mx, my);
    if (pend_dmg) hit_report(mx, my, commit_x, commit_y);
    {
        float mvx = (float)(own_x - prev_x), mvy = (float)(own_y - prev_y);
        float mv = sqrtf(mvx * mvx + mvy * mvy);
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
    bool hot = false;
    for (int i = 0; i < threat_n; i++)
    {
        if (ad::static_clear(threats[i], mx, my) < HOT_CLEAR)
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
    ad_pass_own = wall_passable_at(mx, my);
    float fx = mx + dir_x * REACH, fy = my + dir_y * REACH;
    for (int k = 0; k < 3 && !wall_los(mx, my, fx, fy); k++)
    {
        fx = mx + (fx - mx) * 0.5f;
        fy = my + (fy - my) * 0.5f;
    }
    float em = edge_margin();
    float wu = wall_units(wall_cache_w()), hu = wall_units(wall_cache_h());
    float lo_x = em < wu * 0.5f ? em : wu * 0.5f;
    float hi_x = wu - em > wu * 0.5f ? wu - em : wu * 0.5f;
    float lo_y = em < hu * 0.5f ? em : hu * 0.5f;
    float hi_y = hu - em > hu * 0.5f ? hu - em : hu * 0.5f;
    fx = fminf(fmaxf(fx, lo_x), hi_x);
    fy = fminf(fmaxf(fy, lo_y), hi_y);
    int32_t tx = axis(fx, wall_cache_w());
    int32_t ty = axis(fy, wall_cache_h());
    pred_set(tx, ty);
    if (now - sent_ms >= SEND_MS)
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
