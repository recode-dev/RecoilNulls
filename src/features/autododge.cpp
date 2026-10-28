#include "autododge.h"
#include "../core/commands.h"
#include "../core/scan.h"
#include "../core/scstring.h"
#define PROJ_DATA_BLOCK_SIZE 0x1E8
#include "../utils/walls.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <unordered_map>
#include <cstdint>
#include <vector>

int active = 0, in_move = 0;
int ad_beam_n = 0;
int ad_ind_n = 0;
int32_t ad_target_x = INT32_MAX, ad_target_y = INT32_MAX;
int ad_best_dir = -1;
int ad_char_n = 0, ad_proj_n = 0, ad_enemy_n = 0, ad_threat_n = 0;
int ad_threat_pid[AD_REPORT_MAX] = {0};
float ad_threat_x[AD_REPORT_MAX] = {0}, ad_threat_y[AD_REPORT_MAX] = {0};
float ad_threat_vx[AD_REPORT_MAX] = {0}, ad_threat_vy[AD_REPORT_MAX] = {0};
float ad_threat_r[AD_REPORT_MAX] = {0}, ad_threat_d[AD_REPORT_MAX] = {0};
float ad_threat_ttc[AD_REPORT_MAX] = {0}, ad_threat_ca[AD_REPORT_MAX] = {0};
float ad_threat_score[AD_REPORT_MAX] = {0};
char ad_threat_name[AD_REPORT_MAX][40] = {{0}};
char ad_hit[256] = {0};

int ad_enemy_pid[AD_ENEMY_MAX] = {0};
float ad_enemy_x[AD_ENEMY_MAX] = {0.0f};
float ad_enemy_y[AD_ENEMY_MAX] = {0.0f};
int ad_enemy_hp[AD_ENEMY_MAX] = {0};
float ad_enemy_d[AD_ENEMY_MAX] = {0.0f};
int ad_enemy_aim[AD_ENEMY_MAX] = {0};
int ad_enemy_burst[AD_ENEMY_MAX] = {0};

constexpr float REACH = 700.0f, REACT_S = 0.18f;
constexpr uint32_t SEND_MIN_MS = 16;
constexpr float PROBE_T = 0.12f;
constexpr int PROBE_N = 6;

namespace
{
constexpr float DEG_TO_RAD = 0.0174532925f;
constexpr float OWN_HITBOX = 120.0f;
constexpr float INF_F = std::numeric_limits<float>::infinity();
constexpr float VV_EPS = 1e-6f, DT_MIN = 0.005f, DT_MAX = 0.2f;
constexpr float PIN_PENALTY = 2.0f;
constexpr float DANGER_PICK_EPS = 0.02f;
constexpr float OWN_VEL_MAX = 4000.0f;
constexpr uint64_t PROJ_CACHE_TTL_MS = 1024;
constexpr float SPEED_MIN = 1.0f, QUADRATIC_K = 2.0f;
constexpr float RICOCHET_DOT_THRESHOLD = 0.5f;
constexpr float RICOCHET_MIN_STEP = 30.0f;
constexpr int DIRS = 8;

struct Threat
{
    float x, y, vx, vy, rad, prev_x, prev_y;
    int ricochet;
    int32_t pid;
    char name[40];
};
struct ProjectileState
{
    float dir_x, dir_y, speed, radius;
    char name[128];
    float prevX, prevY;
    uint64_t lastSeen, lastTs;
    int32_t lastX, lastY, pid;
    float last_ux, last_uy;
    int32_t maxBounces, bounces;
    bool isIndirect, isBeam, hasVelocity, isRicochet;
    bool isBouncing, piercesEnv, piercesEnvButter;
};

static uintptr_t char_vt_rva = 0;
static int32_t own_gid = 0;
static std::vector<ProjectileState> proj_cache_storage;
static std::unordered_map<int32_t, size_t> proj_cache_index;

static void rebuild_projectile_cache_index()
{
    proj_cache_index.clear();
    for (size_t i = 0; i < proj_cache_storage.size(); i++)
        proj_cache_index[proj_cache_storage[i].pid] = i;
}

static std::vector<Threat> threats;
static uint64_t lastSentAt = 0, burstUntil = 0;
static uint32_t lastHpSeen = 0;
static int pendingHit = 0, pendingDmg = 0;
static float commitDirX = 0.0f, commitDirY = 0.0f;
static float CHAR_SPEED = 0.0f;
static float ad_vx = 0.0f, ad_vy = 0.0f;
static float prev_mx = 0.0f, prev_my = 0.0f;
static uint64_t prev_time = 0;

static float point_to_segment_dist(
    float px, float py, float ax, float ay, float bx, float by) noexcept
{
    float dx = bx - ax, dy = by - ay;
    float len2 = std::fma(dx, dx, dy * dy);
    if (len2 < VV_EPS)
        return sqrtf(std::fma(px - ax, px - ax, (py - ay) * (py - ay)));
    float t = std::fma(px - ax, dx, (py - ay) * dy) / len2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float cx = std::fma(dx, t, ax);
    float cy = std::fma(dy, t, ay);
    return sqrtf(std::fma(px - cx, px - cx, (py - cy) * (py - cy)));
}

static float timeToCollision(
    float mx, float my, float mvx, float mvy, const Threat& t) noexcept
{
    float relx = t.x - mx, rely = t.y - my;
    float vrx = t.vx - mvx, vry = t.vy - mvy;
    float vv = std::fma(vrx, vrx, vry * vry);
    if (vv < VV_EPS) return INF_F;
    float b = QUADRATIC_K * std::fma(relx, vrx, rely * vry);
    float c = std::fma(relx, relx, rely * rely) - t.rad * t.rad;
    float disc = std::fma(b, b, -QUADRATIC_K * QUADRATIC_K * vv * c);
    if (disc < 0.0f) return INF_F;
    float sd = sqrtf(disc);
    float t1 = (-b - sd) / (QUADRATIC_K * vv);
    float t2 = (-b + sd) / (QUADRATIC_K * vv);
    if (t1 > 0.0f) return t1;
    if (t2 > 0.0f) return t2;
    return INF_F;
}

static float timeToClosestApproach(
    const Threat& t, float mx, float my, float mvx, float mvy) noexcept
{
    float relx = t.x - mx, rely = t.y - my;
    float vrx = t.vx - mvx, vry = t.vy - mvy;
    float vv = std::fma(vrx, vrx, vry * vry);
    if (vv < VV_EPS) return -1.0f;
    return -std::fma(relx, vrx, rely * vry) / vv;
}

static bool threat_receding(
    const Threat& t, float mx, float my, float mvx, float mvy) noexcept
{
    if (fabsf(t.vx) + fabsf(t.vy) < 1.0f) return false;
    return timeToClosestApproach(t, mx, my, mvx, mvy) < 0.0f;
}

static float escape_window(const Threat& t) noexcept
{
    float speed = CHAR_SPEED > SPEED_MIN ? CHAR_SPEED : SPEED_MIN;
    return t.rad / speed + REACT_S;
}

static bool threat_soon(const Threat& t, float mx, float my) noexcept
{
    float vv = std::fma(t.vx, t.vx, t.vy * t.vy);
    if (vv < 1.0f) return false;
    float rx = mx - t.x, ry = my - t.y;
    float proj = std::fma(t.vx, rx, t.vy * ry);
    if (proj <= 0.0f) return false;
    float speed = sqrtf(vv);
    if (fabsf(t.vx * ry - t.vy * rx) / speed > t.rad + OWN_HITBOX * 0.35f)
        return false;
    return proj / vv <= escape_window(t);
}

static float wallAhead(float mx,
                       float my,
                       float dirX,
                       float dirY,
                       float speed,
                       float bodyR) noexcept
{
    float step = speed * PROBE_T;
    int blocked = 0;
    for (int s = 1; s <= PROBE_N; s++)
    {
        if (wall_is_blocked_wide(mx + dirX * step * s,
                                 my + dirY * step * s,
                                 bodyR,
                                 WALL_BLOCKS_MOVEMENT))
            blocked++;
    }
    return (float)blocked / (float)PROBE_N;
}

static int32_t clamp_axis(float value, int tiles) noexcept
{
    float limit = (float)tiles * WALL_TILE_SIZE - SPEED_MIN;
    if (value < 0.0f) return 0;
    if (value > limit) return (int32_t)limit;
    return (int32_t)roundf(value);
}

static float pinPenalty(float mx, float my, float dx, float dy) noexcept
{
    int32_t tx = clamp_axis(mx + dx * REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + dy * REACH, wall_cache_h());
    float ex = (float)tx - mx, ey = (float)ty - my;
    float room = sqrtf(std::fma(ex, ex, ey * ey));
    if (room >= REACH) return 0.0f;
    return PIN_PENALTY * (1.0f - room / REACH);
}

static void clearHeading() noexcept
{
    commitDirX = 0.0f;
    commitDirY = 0.0f;
}

static void endDodge() noexcept
{
    clearHeading();
    ad_target_x = INT32_MAX;
    ad_target_y = INT32_MAX;
    active = 0;
}

static uintptr_t call_fn(uintptr_t fn, uintptr_t a, uintptr_t b) noexcept
{
    if (!fn || !base) return 0;
    uintptr_t rva = (fn >= base) ? (fn - base) : fn;
    uintptr_t target = callable(rva);
    if (!target) return 0;
    using fn_t = uintptr_t (*)(uintptr_t, uintptr_t);
    return ((fn_t)target)(a, b);
}

static float swept_ca(float mx,
                      float my,
                      float mvx,
                      float mvy,
                      float px,
                      float py,
                      float vx,
                      float vy) noexcept
{
    float rx = px - mx, ry = py - my;
    float rvx = vx - mvx, rvy = vy - mvy;
    float vv = std::fma(rvx, rvx, rvy * rvy);
    if (vv < VV_EPS) return sqrtf(std::fma(rx, rx, ry * ry));
    float t = -std::fma(rx, rvx, ry * rvy) / vv;
    if (t < 0.0f) t = 0.0f;
    float cx = std::fma(rvx, t, rx);
    float cy = std::fma(rvy, t, ry);
    return sqrtf(std::fma(cx, cx, cy * cy));
}

static float swept_ca_segment(float mx, float my, const Threat& t) noexcept
{
    float d1 = point_to_segment_dist(mx, my, t.prev_x, t.prev_y, t.x, t.y);
    return d1;
}

static float
clearanceFor(float mx, float my, float dx, float dy, float speed) noexcept
{
    float worst = INF_F;
    for (const auto& t : threats)
    {
        if (!threat_soon(t, mx, my) &&
            !(t.ricochet != 0 && !threat_receding(t, mx, my, ad_vx, ad_vy)))
            continue;
        float rvx = t.vx - dx * speed, rvy = t.vy - dy * speed;
        float rx = t.x - mx, ry = t.y - my;
        float vv = std::fma(rvx, rvx, rvy * rvy);
        float tt = 0.0f;
        if (vv > VV_EPS)
        {
            tt = -std::fma(rx, rvx, ry * rvy) / vv;
            float h = escape_window(t);
            if (tt < 0.0f) tt = 0.0f;
            if (tt > h) tt = h;
        }
        float cx = std::fma(rvx, tt, rx);
        float cy = std::fma(rvy, tt, ry);
        float d = sqrtf(std::fma(cx, cx, cy * cy)) - t.rad;
        if (d < worst) worst = d;
    }
    return worst;
}

static void snapshot_hit(float mx, float my)
{
    int len = 0, picked = 0;
    char used[AD_REPORT_MAX] = {0};
    len = snprintf(ad_hit,
                   sizeof(ad_hit),
                   "dmg=%d act=%d n=%d p=(%.0f,%.0f)",
                   pendingDmg,
                   active,
                   static_cast<int>(threats.size()),
                   mx,
                   my);
    for (int step = 0; step < AD_REPORT_MAX; step++)
    {
        int best = -1;
        float bestCa = 1e9f;
        for (int i = 0; i < static_cast<int>(threats.size()); i++)
        {
            if (i >= AD_REPORT_MAX * 8 || used[i]) continue;
            float ca = swept_ca(mx,
                                my,
                                ad_vx,
                                ad_vy,
                                threats[i].x,
                                threats[i].y,
                                threats[i].vx,
                                threats[i].vy);
            float seg_ca = swept_ca_segment(mx, my, threats[i]);
            if (seg_ca < ca) ca = seg_ca;
            if (ca < bestCa)
            {
                bestCa = ca;
                best = i;
            }
        }
        if (best < 0 || picked >= AD_REPORT_MAX) break;
        used[best] = 1;
        picked++;
        if (len > 0 && len < static_cast<int>(sizeof(ad_hit)) - 40)
        {
            len +=
                snprintf(ad_hit + len,
                         sizeof(ad_hit) - static_cast<size_t>(len),
                         " | ca=%.0f ttc=%.2f r=%.0f",
                         bestCa,
                         timeToCollision(mx, my, ad_vx, ad_vy, threats[best]),
                         threats[best].rad);
        }
    }
    if (picked < 1 && len > 0 && len < static_cast<int>(sizeof(ad_hit)) - 8)
        snprintf(
            ad_hit + len, sizeof(ad_hit) - static_cast<size_t>(len), " | none");
}

static void learn_char_vtable()
{
    void* dataPtr = nullptr;
    void* vt = nullptr;
    if (char_vt_rva || !base || !own_elem) return;
    if (!read_ptr(own_elem + ELEM_DEF_OFF, &dataPtr) || !dataPtr ||
        !pointer_plausible((uintptr_t)dataPtr))
        return;
    if (!read_ptr((uintptr_t)dataPtr, &vt) || !vt) return;
    uintptr_t rva = (uintptr_t)vt - base;
    if (rva == 0 || rva == VTABLE_PROJ_DATA_RVA || rva < DC_RVA_LO ||
        rva >= DC_RVA_LO + DC_RVA_SIZE)
        return;
    char_vt_rva = rva;
}

static bool char_like(uintptr_t vtRva) noexcept
{
    return char_vt_rva != 0 && vtRva == char_vt_rva;
}

}

void autododge_reset()
{
    ad_best_dir = -1;
    lastHpSeen = 0;
    pendingHit = 0;
    pendingDmg = 0;
    ad_char_n = 0;
    ad_proj_n = 0;
    ad_enemy_n = 0;
    ad_threat_n = 0;
    proj_cache_storage.clear();
    proj_cache_index.clear();
    lastSentAt = 0;
    burstUntil = 0;
    commitDirX = 0.0f;
    commitDirY = 0.0f;
    ad_target_x = INT32_MAX;
    ad_target_y = INT32_MAX;
    active = 0;
    ad_vx = 0.0f;
    ad_vy = 0.0f;
    prev_mx = 0.0f;
    prev_my = 0.0f;
    prev_time = 0;
}

void autododge()
{
    ad_char_n = ad_proj_n = ad_enemy_n = ad_threat_n = 0;
    ad_beam_n = 0;
    ad_ind_n = 0;
    if (lastHpSeen && own_hp < lastHpSeen)
    {
        pendingHit = 1;
        pendingDmg = static_cast<int>(lastHpSeen - own_hp);
    }
    lastHpSeen = own_hp;
    ad_best_dir = -1;
    own_gid = 0;
    for (int i = 0; i < AD_REPORT_MAX; i++)
        ad_threat_name[i][0] = 0;
    if (!base || !own_elem || dead)
    {
        endDodge();
        return;
    }
    learn_char_vtable();
    if (!char_vt_rva)
    {
        endDodge();
        return;
    }
    read_int(own_elem + static_cast<uintptr_t>(OBJ_GLOBALID_OFF), &own_gid);

    uint64_t now = us() / 1000;
    int tw = wall_cache_w(), th = wall_cache_h();
    if (!coord_ok || tw <= 0 || th <= 0)
    {
        endDodge();
        return;
    }
    float mx = (float)own_x, my = (float)own_y;

    if (prev_time && now > prev_time)
    {
        float dt = (float)(now - prev_time) / 1000.0f;
        if (dt > DT_MIN && dt < DT_MAX && (prev_mx != 0.0f || prev_my != 0.0f))
        {
            ad_vx = (mx - prev_mx) / dt;
            ad_vy = (my - prev_my) / dt;
            float vmag = sqrtf(std::fma(ad_vx, ad_vx, ad_vy * ad_vy));
            if (vmag > OWN_VEL_MAX)
            {
                ad_vx = 0.0f;
                ad_vy = 0.0f;
            }
        }
        else
        {
            ad_vx = 0.0f;
            ad_vy = 0.0f;
        }
    }
    prev_mx = mx;
    prev_my = my;
    prev_time = now;

    CHAR_SPEED = own_speed;
    threats.clear();
    threats.reserve(32);
    float bodyR = OWN_HITBOX * 1.75f;
    float zoneSq = CHAR_SPEED * 4.0f * (CHAR_SPEED * 4.0f);

    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i]) continue;
        const uint8_t* obj = obj_block[i];
        uintptr_t dataPtr = 0;
        memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));
        if (!dataPtr || !pointer_plausible(dataPtr)) continue;

        uintptr_t dataVtable = 0;
        if (!read_bytes(dataPtr, &dataVtable, sizeof(dataVtable))) continue;
        if (!dataVtable || !pointer_plausible(dataVtable)) continue;
        uintptr_t vtRva = dataVtable - base;
        if (vtRva < DC_RVA_LO || vtRva >= DC_RVA_LO + DC_RVA_SIZE) continue;

        if (vtRva != VTABLE_PROJ_DATA_RVA)
        {
            if (char_like(vtRva))
            {
                int32_t cteam, chp, cgid, cx_i, cy_i, cang;
                memcpy(&cteam, obj + OBJ_TEAM_OFF, sizeof(cteam));
                memcpy(&chp, obj + OBJ_HP_OFF, sizeof(chp));
                memcpy(&cgid, obj + OBJ_GLOBALID_OFF, sizeof(cgid));
                memcpy(&cx_i, obj + OBJ_X_OFF, sizeof(cx_i));
                memcpy(&cy_i, obj + OBJ_Y_OFF, sizeof(cy_i));
                memcpy(&cang, obj + OBJ_ATTACK_ANGLE_OFF, sizeof(cang));
                ad_char_n++;
                if (cteam != own_team_b && chp > 0 && cgid > 0 &&
                    cgid != own_gid && ad_enemy_n < AD_ENEMY_MAX)
                {
                    float edx = (float)cx_i - mx, edy = (float)cy_i - my;
                    ad_enemy_pid[ad_enemy_n] = cgid;
                    ad_enemy_x[ad_enemy_n] = (float)cx_i;
                    ad_enemy_y[ad_enemy_n] = (float)cy_i;
                    ad_enemy_hp[ad_enemy_n] = chp;
                    ad_enemy_d[ad_enemy_n] =
                        sqrtf(std::fma(edx, edx, edy * edy));
                    ad_enemy_aim[ad_enemy_n] = cang;
                    ad_enemy_burst[ad_enemy_n] = 0;
                    if (dataPtr && pointer_plausible(dataPtr))
                    {
                        void* skillPtr = nullptr;
                        if (read_ptr(dataPtr + DATA_SKILL_OFF, &skillPtr) &&
                            pointer_plausible((uintptr_t)skillPtr))
                        {
                            int32_t nb = (int32_t)call_fn(
                                callable(RVA_SKILL_GET_BULLETS),
                                (uintptr_t)skillPtr,
                                0);
                            int32_t gap = (int32_t)call_fn(
                                callable(RVA_SKILL_GET_MS_BETWEEN),
                                (uintptr_t)skillPtr,
                                0);
                            if (nb > 1 && gap > 0 && gap < 500 && nb < 40)
                                ad_enemy_burst[ad_enemy_n] = nb * gap;
                        }
                    }
                    ad_enemy_n++;
                }
            }
            continue;
        }
        ad_proj_n++;

        int32_t pid, teamId, px_i, py_i;
        memcpy(&pid, obj + OBJ_GLOBALID_OFF, sizeof(pid));
        memcpy(&teamId, obj + OBJ_TEAM_OFF, sizeof(teamId));
        memcpy(&px_i, obj + OBJ_X_OFF, sizeof(px_i));
        memcpy(&py_i, obj + OBJ_Y_OFF, sizeof(py_i));
        float px = (float)px_i, py = (float)py_i;
        if (pid == 0 || teamId == own_team_b) continue;
        uint32_t deadFlag;
        memcpy(&deadFlag, obj + OBJ_DEADFLAG_OFF, sizeof(deadFlag));
        if (deadFlag != 0) continue;

        auto it = proj_cache_index.find(pid);
        if (it == proj_cache_index.end())
        {
            uint8_t data[PROJ_DATA_BLOCK_SIZE];
            if (!read_bytes(dataPtr, data, sizeof(data))) continue;
            ProjectileState state{};
            state.lastX = px_i;
            state.lastY = py_i;
            state.prevX = px;
            state.prevY = py;
            state.lastTs = now;
            int32_t projSpeed = 0, projRadius = 0;
            if (!read_int(dataPtr + PROJECTILEDATA_SPEED_OFF, &projSpeed))
                continue;
            memcpy(&projRadius,
                   data + PROJECTILEDATA_RADIUS_OFF,
                   sizeof(projRadius));
            if (projSpeed <= 0) continue;
            state.pid = pid;
            state.isIndirect = data[PROJECTILEDATA_INDIRECT_OFF] != 0;
            state.isBeam = data[PROJECTILEDATA_ISBEAM_OFF] != 0;
            state.isBouncing = data[PROJECTILEDATA_ISBOUNCING_OFF] != 0;
            state.piercesEnv = data[PROJECTILEDATA_PIERCES_ENV_OFF] != 0;
            state.piercesEnvButter =
                data[PROJECTILEDATA_PIERCES_ENV_BUTTER_OFF] != 0;
            memcpy(&state.maxBounces,
                   data + PROJECTILEDATA_MAX_BOUNCES_OFF,
                   sizeof(state.maxBounces));
            state.isRicochet = state.isBouncing && state.maxBounces > 0;
            state.speed = (float)projSpeed;
            state.radius = (float)projRadius;
            {
                float bestD2 = 1e30f;
                int bestK = -1;
                for (int k = 0; k < ad_enemy_n; k++)
                {
                    float ex = ad_enemy_x[k] - px, ey = ad_enemy_y[k] - py;
                    float d2 = std::fma(ex, ex, ey * ey);
                    if (d2 < bestD2)
                    {
                        bestD2 = d2;
                        bestK = k;
                    }
                }
                if (bestK >= 0)
                {
                    if (ad_enemy_burst[bestK] > 0)
                        burstUntil = now + (uint64_t)ad_enemy_burst[bestK];
                    float ang = (float)ad_enemy_aim[bestK] * DEG_TO_RAD;
                    state.dir_x = cosf(ang);
                    state.dir_y = sinf(ang);
                    state.last_ux = state.dir_x;
                    state.last_uy = state.dir_y;
                    state.hasVelocity = true;
                }
            }
            state.name[0] = '\0';
            uintptr_t nameStr = call_fn(addr_dataname, dataPtr, 0);
            if (nameStr)
                read_scstring(nameStr,
                              SCSTRING_DEFAULT_MAX_LENGTH,
                              state.name,
                              sizeof(state.name));
            proj_cache_index[pid] = proj_cache_storage.size();
            proj_cache_storage.push_back(state);
            it = proj_cache_index.find(pid);
        }
        else
        {
            if (it->second >= proj_cache_storage.size() ||
                proj_cache_storage[it->second].pid != pid)
            {
                rebuild_projectile_cache_index();
                it = proj_cache_index.find(pid);
            }
            if (it == proj_cache_index.end()) continue;
            ProjectileState& state = proj_cache_storage[it->second];
            state.prevX = (float)state.lastX;
            state.prevY = (float)state.lastY;
            float dt = (float)(now - state.lastTs) / 1000.0f;
            float ddx = (float)px_i - (float)state.lastX,
                  ddy = (float)py_i - (float)state.lastY;
            float step = sqrtf(std::fma(ddx, ddx, ddy * ddy));
            if (dt > DT_MIN && dt < DT_MAX && step > 1.0f)
            {
                float ux = ddx / step, uy = ddy / step;
                if (state.hasVelocity &&
                    std::fma(ux, state.dir_x, uy * state.dir_y) <
                        RICOCHET_DOT_THRESHOLD &&
                    step > RICOCHET_MIN_STEP)
                {
                    state.bounces++;
                    state.isRicochet =
                        state.isBouncing && state.bounces <= state.maxBounces;
                }
                state.dir_x = ux;
                state.dir_y = uy;
                state.last_ux = ux;
                state.last_uy = uy;
                state.hasVelocity = true;
            }
            state.lastX = px_i;
            state.lastY = py_i;
            state.lastTs = now;
        }
        ProjectileState& state = proj_cache_storage[it->second];
        state.lastSeen = now;
        float dx = px - mx, dy = py - my;
        if (std::fma(dx, dx, dy * dy) > zoneSq) continue;
        if (state.isBeam)
        {
            ad_beam_n++;
            continue;
        }
        if (state.isIndirect) ad_ind_n++;
        float mag = state.speed;
        float vx = state.hasVelocity ? state.dir_x * mag : 0.0f;
        float vy = state.hasVelocity ? state.dir_y * mag : 0.0f;
        if (!state.isIndirect && !state.piercesEnv && !state.piercesEnvButter &&
            !wall_los(px, py, mx, my, WALL_BLOCKS_PROJECTILES))
            continue;
        Threat item{};
        item.x = px;
        item.y = py;
        item.prev_x = state.prevX;
        item.prev_y = state.prevY;
        item.vx = vx;
        item.vy = vy;
        item.rad = state.radius + OWN_HITBOX;
        item.ricochet = state.isRicochet ? 1 : 0;
        item.pid = pid;
        memcpy(item.name, state.name, sizeof(item.name) - 1);
        item.name[sizeof(item.name) - 1] = '\0';
        threats.push_back(item);
    }

    {
        size_t writeIdx = 0;
        for (size_t i = 0; i < proj_cache_storage.size(); i++)
        {
            if (now - proj_cache_storage[i].lastSeen > PROJ_CACHE_TTL_MS)
                continue;
            if (writeIdx != i)
                proj_cache_storage[writeIdx] = proj_cache_storage[i];
            writeIdx++;
        }
        proj_cache_storage.resize(writeIdx);
        if (writeIdx != proj_cache_index.size())
            rebuild_projectile_cache_index();
    }

    for (const auto& t : threats)
    {
        float ttc = timeToCollision(mx, my, ad_vx, ad_vy, t);
        float dist =
            sqrtf(std::fma(t.x - mx, t.x - mx, (t.y - my) * (t.y - my)));
        float ca = swept_ca(mx, my, ad_vx, ad_vy, t.x, t.y, t.vx, t.vy);
        float seg_ca = swept_ca_segment(mx, my, t);
        if (seg_ca < ca) ca = seg_ca;
        float rvx = t.vx - ad_vx, rvy = t.vy - ad_vy;
        float vv = std::fma(rvx, rvx, rvy * rvy);
        float score = 0.0f;
        if (vv > VV_EPS)
        {
            float along = std::fma(t.x - mx, rvx, (t.y - my) * rvy) / sqrtf(vv);
            score = along >= 0.0f ? ca : ca + 1000000.0f;
        }
        else
            score = ca + 1000000.0f;
        if (t.ricochet) score = ca;
        int slot = ad_threat_n;
        if (slot < AD_REPORT_MAX)
            ad_threat_n++;
        else
        {
            int worst = 0;
            for (int i = 1; i < AD_REPORT_MAX; i++)
                if (ad_threat_score[i] > ad_threat_score[worst]) worst = i;
            if (score >= ad_threat_score[worst]) continue;
            slot = worst;
        }
        ad_threat_pid[slot] = t.pid;
        ad_threat_x[slot] = t.x;
        ad_threat_y[slot] = t.y;
        ad_threat_vx[slot] = t.vx;
        ad_threat_vy[slot] = t.vy;
        ad_threat_r[slot] = t.rad;
        ad_threat_d[slot] = dist;
        ad_threat_ttc[slot] = ttc;
        ad_threat_ca[slot] = ca;
        ad_threat_score[slot] = score;
        memcpy(ad_threat_name[slot], t.name, sizeof(ad_threat_name[slot]));
        ad_threat_name[slot][sizeof(ad_threat_name[slot]) - 1] = '\0';
    }
    if (pendingHit)
    {
        snapshot_hit(mx, my);
        pendingHit = 0;
    }
    autododge_scanner_report();

    bool anySoon = false;
    for (const auto& t : threats)
        if (threat_soon(t, mx, my))
        {
            anySoon = true;
            break;
        }
    if (!anySoon && now >= burstUntil)
    {
        endDodge();
        return;
    }

    bool hasPrev = commitDirX != 0.0f || commitDirY != 0.0f;
    int bestIdx = 0;
    float bestDirX = 1.0f, bestDirY = 0.0f;
    float bestClear = -INF_F, bestDanger = INF_F;
    for (int i = 0; i < DIRS; i++)
    {
        float ang = (float)i * 6.2831853f / (float)DIRS;
        float dx = cosf(ang), dy = sinf(ang);
        float clear = clearanceFor(mx, my, dx, dy, CHAR_SPEED);
        float d = -clear + pinPenalty(mx, my, dx, dy) +
                  wallAhead(mx, my, dx, dy, CHAR_SPEED, bodyR);
        bool better = false;
        if (d < bestDanger - DANGER_PICK_EPS)
            better = true;
        else if (d <= bestDanger + DANGER_PICK_EPS && hasPrev)
            better = std::fma(dx, commitDirX, dy * commitDirY) >
                     std::fma(bestDirX, commitDirX, bestDirY * commitDirY);
        if (better)
        {
            bestDanger = d;
            bestClear = clear;
            bestDirX = dx;
            bestDirY = dy;
            bestIdx = i;
        }
    }

    float outDirX = bestDirX, outDirY = bestDirY;
    if (hasPrev && clearanceFor(mx, my, commitDirX, commitDirY, CHAR_SPEED) +
                           OWN_HITBOX * 0.2f >=
                       bestClear)
    {
        outDirX = commitDirX;
        outDirY = commitDirY;
    }
    ad_best_dir = bestIdx;
    commitDirX = outDirX;
    commitDirY = outDirY;

    int32_t tx = clamp_axis(mx + outDirX * REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + outDirY * REACH, wall_cache_h());
    int32_t ddx = tx - ad_target_x, ddy = ty - ad_target_y;
    if ((float)(ddx * ddx + ddy * ddy) > 64.0f ||
        now - lastSentAt > SEND_MIN_MS)
    {
        lastSentAt = now;
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    active = 1;
}
