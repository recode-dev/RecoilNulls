#include "autododge.h"
#include "../core/commands.h"
#include "../core/scan.h"
#include "../core/scstring.h"
#define PROJ_DATA_BLOCK_SIZE (PROJECTILEDATA_RADIUS_OFF + sizeof(int32_t))
#include "../utils/walls.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <unordered_map>
#include <cstdint>
#include <vector>

int active = 0, moving = 0, in_move = 0;
int32_t ad_target_x = INT32_MAX, ad_target_y = INT32_MAX;
int ad_best_dir = -1;
int ad_char_n = 0, ad_proj_n = 0, ad_enemy_n = 0, ad_threat_n = 0;
int ad_threat_pid[AD_REPORT_MAX] = {0};
float ad_threat_x[AD_REPORT_MAX] = {0}, ad_threat_y[AD_REPORT_MAX] = {0};
float ad_threat_vx[AD_REPORT_MAX] = {0}, ad_threat_vy[AD_REPORT_MAX] = {0};
float ad_threat_r[AD_REPORT_MAX] = {0}, ad_threat_d[AD_REPORT_MAX] = {0};
float ad_threat_ttc[AD_REPORT_MAX] = {0}, ad_threat_ca[AD_REPORT_MAX] = {0};
float ad_threat_score[AD_REPORT_MAX] = {0}, ad_threat_spd[AD_REPORT_MAX] = {0};
int ad_threat_ind[AD_REPORT_MAX] = {0};
char ad_threat_name[AD_REPORT_MAX][40] = {{0}};
char ad_hit[256] = {0};

char ad_vp[AD_VP_MAX][128] = {{0}};
int ad_vp_n = 0;
char ad_probe[256] = {0};
char ad_obj[AD_OBJ_MAX][88] = {{0}};
int ad_obj_n = 0;
char ad_cat[AD_CAT_MAX][620] = {{0}};
int ad_cat_n = 0;
int ad_cat_ready = 0;
char ad_diff[600] = {0};
char ad_nv[AD_NV_MAX][96] = {{0}};
int ad_nv_n = 0;
int ad_enemy_pid[AD_ENEMY_MAX] = {0};
float ad_enemy_x[AD_ENEMY_MAX] = {0.0f};
float ad_enemy_y[AD_ENEMY_MAX] = {0.0f};
int ad_enemy_hp[AD_ENEMY_MAX] = {0};
float ad_enemy_d[AD_ENEMY_MAX] = {0.0f};

constexpr float AWARE = 4400.0f, REACT_S = 0.35f, SKIN = 90.0f, REACH = 700.0f;
constexpr float HORIZON_S = 1.55f, ENGAGE = 120.0f;
constexpr uint32_t RELEASE_GRACE_MS = 400, SEND_MIN_MS = 16;
constexpr float WALL_HIT = 9000.0f, PROBE_T = 0.12f;
constexpr int PROBE_N = 6;
constexpr float MIN_POS = 15.0f;
constexpr uint64_t POST_DODGE_HOLD_MS = 350;
constexpr uint64_t DANGER_HOLD_MS = 400;
constexpr uint64_t RICOCHET_HOLD_MS = 700;

namespace
{
constexpr uintptr_t EARLY_TICKS_OFF = 0x80;
constexpr float SPEED_SCALE = 1.0f, RADIUS_SCALE = 1.0f;
constexpr int32_t EARLY_TICK_MS = 16;
constexpr float INF_F = std::numeric_limits<float>::infinity();
constexpr float VV_EPS = 1e-6f, DT_MIN = 0.005f, DT_MAX = 0.2f;
constexpr float POS_DELTA_MIN_SQ = 16.0f, PLAYER_ALONG_MIN = 0.0f;
constexpr float BLOB_SPEED_MAX = 1600.0f, DIR_KEEP = 0.55f, MEAS_KEEP = 0.5f,
                MEAS_NEW = 0.5f;
constexpr float DEF_SPEED_MIN = 200.0f, DEF_SPEED_MAX = 20000.0f,
                DIR_NEW = 0.45f;
constexpr float HITBOX_FALLBACK = 120.0f, SPEED_FALLBACK = 720.0f;
constexpr float DANGER_RADIUS = 550.0f, DANGER_RADIUS_RICOCHET = 800.0f;
constexpr float TTC_MIN = 0.05f, DANGER_WALL = 0.9f;
constexpr float DANGER_SUM_WEIGHT = 0.50f, DANGER_MAX = 1.0f,
                DANGER_PICK_EPS = 0.02f;
constexpr float INTEREST_ESCAPE_W = 0.7f, INTEREST_GOAL_W = 0.3f;
constexpr float INTEREST_BACK_DOT = -0.5f, INTEREST_BACK_PENALTY = 0.2f;
constexpr float ESCAPE_DIST_BIAS = 100.0f, POS_CHANGE_MIN_SQ = 64.0f,
                QUADRATIC_K = 2.0f;
constexpr float HALF = 0.5f, SPEED_MIN = 1.0f, HYSTERESIS_MARGIN = 0.25f;
constexpr uint64_t DIR_HOLD_MS = 350;
constexpr int AD_HIT_SCAN = 64;
constexpr float BEHIND_MARGIN = 50.0f;
constexpr float BURST_DOT_MIN = 0.95f;
constexpr float BURST_PARALLEL_PENALTY = 0.40f;
constexpr float BURST_PERP_BLEND = 0.70f;
constexpr float POST_DODGE_OVERRIDE_TTC = 0.075f;
constexpr float POST_DODGE_CA_MARGIN = 20.0f;
constexpr float RICOCHET_DOT_THRESHOLD = 0.5f;
constexpr float RICOCHET_MIN_STEP = 30.0f;
constexpr float SWEEP_HIT_MARGIN = 0.0f;
constexpr int CARDINALS = 8;
constexpr float DIAG = 0.70710678f;
static const float cardDirX[CARDINALS] = {
    1.0f, DIAG, 0.0f, -DIAG, -1.0f, -DIAG, 0.0f, DIAG};
static const float cardDirY[CARDINALS] = {
    0.0f, DIAG, 1.0f, DIAG, 0.0f, -DIAG, -1.0f, -DIAG};

struct Threat
{
    float x, y, vx, vy, rad, mag, prev_x, prev_y;
    int ind;
    int ricochet;
    int32_t pid;
    char name[40];
};
struct ProjectileState
{
    float dir_x, dir_y, speed, radius, meas;
    char name[128];
    float spawnX, spawnY, prevX, prevY;
    int earlyTicks;
    uint64_t spawnedAt, lastSeen, lastTs;
    int32_t lastX, lastY, pid;
    int32_t seen;
    bool probed;
    float last_ux, last_uy;
    bool isIndirect, isBeam, hasVelocity, isRicochet;
    uint64_t ricochetUntil;
    int32_t rawSpeed, rawRadius, rawEarly;
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
static int headingIdx = -1;
static uint64_t lastDangerTs = 0, lastSentAt = 0, dirLockUntil = 0;
static uint32_t lastHpSeen = 0;
static int pendingHit = 0, pendingDmg = 0;
static float commitDirX = 0.0f, commitDirY = 0.0f;
static float CHAR_SPEED = SPEED_FALLBACK;
static float ad_vx = 0.0f, ad_vy = 0.0f;
static float prev_mx = 0.0f, prev_my = 0.0f;
static uint64_t prev_time = 0;
static bool g_has_burst = false;
static float g_burst_vx = 0.0f, g_burst_vy = 0.0f;
static uint64_t postDodgeUntil = 0;
static float postDodgeDirX = 0.0f, postDodgeDirY = 0.0f;
static uint64_t dangerHoldUntil = 0;

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

static float wallAhead(float mx,
                       float my,
                       float dirX,
                       float dirY,
                       float speed,
                       float bodyR) noexcept
{
    if (!wall_ready()) return 0.0f;
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
    return WALL_HIT * (float)blocked / (float)PROBE_N;
}

static int32_t clamp_axis(float value, int tiles) noexcept
{
    if (tiles <= 0) return (int32_t)roundf(value);
    float limit = (float)tiles * WALL_TILE_SIZE - SPEED_MIN;
    if (limit <= 0.0f) return (int32_t)roundf(value);
    if (value < 0.0f) return 0;
    if (value > limit) return (int32_t)limit;
    return (int32_t)roundf(value);
}

static void clearHeading() noexcept
{
    headingIdx = -1;
    commitDirX = 0.0f;
    commitDirY = 0.0f;
}

static void endDodge() noexcept
{
    dirLockUntil = 0;
    postDodgeUntil = 0;
    postDodgeDirX = 0.0f;
    postDodgeDirY = 0.0f;
    dangerHoldUntil = 0;
    clearHeading();
    ad_target_x = INT32_MAX;
    ad_target_y = INT32_MAX;
    active = 0;
    moving = 0;
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

static float dangerFor(
    float mx, float my, float dx, float dy, float speed, float bodyR) noexcept
{
    float mvx = speed * dx, mvy = speed * dy;
    float max_c = 0.0f, sum_c = 0.0f;
    for (const auto& t : threats)
    {
        if (timeToClosestApproach(t, mx, my, ad_vx, ad_vy) < 0.0f &&
            t.ricochet == 0)
            continue;
        float ttc = timeToCollision(mx, my, mvx, mvy, t);
        if (ttc < TTC_MIN) return DANGER_MAX;
        float ca = swept_ca(mx, my, mvx, mvy, t.x, t.y, t.vx, t.vy);
        float seg_ca = swept_ca_segment(mx, my, t);
        if (seg_ca < ca) ca = seg_ca;
        float radius = t.ricochet ? DANGER_RADIUS_RICOCHET : DANGER_RADIUS;
        float m = ca - t.rad;
        float c = (radius - m) / radius;
        if (c < 0.0f) c = 0.0f;
        if (c > max_c) max_c = c;
        sum_c += c;
    }
    if (wallAhead(mx, my, dx, dy, speed, bodyR) > 0.0f) return DANGER_WALL;
    float danger = max_c + DANGER_SUM_WEIGHT * (sum_c - max_c);
    if (g_has_burst)
    {
        float parallel = fabsf(dx * g_burst_vx + dy * g_burst_vy);
        danger += parallel * BURST_PARALLEL_PENALTY;
    }
    return danger > DANGER_MAX ? DANGER_MAX : danger;
}

static void snapshot_hit(float mx, float my)
{
    int len = 0, picked = 0;
    char used[AD_HIT_SCAN] = {0};
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
            if (i >= AD_HIT_SCAN || used[i]) continue;
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

static bool char_like(uintptr_t vtRva, const uint8_t* obj) noexcept
{
    if (char_vt_rva) return vtRva == char_vt_rva;
    if (vtRva == VTABLE_PROJ_DATA_RVA) return false;
    int32_t team, hp, gid;
    memcpy(&team, obj + OBJ_TEAM_OFF, sizeof(team));
    memcpy(&hp, obj + OBJ_HP_OFF, sizeof(hp));
    memcpy(&gid, obj + OBJ_GLOBALID_OFF, sizeof(gid));
    return team >= 0 && team <= 1 && hp > 0 && hp <= 200000 && gid > 0;
}

static void detect_burst()
{
    g_has_burst = false;
    g_burst_vx = 0.0f;
    g_burst_vy = 0.0f;
    int n = static_cast<int>(threats.size());
    if (n < 2) return;
    float acc_x = 0.0f, acc_y = 0.0f;
    int burst_threats = 0;
    for (int i = 0; i < n; i++)
    {
        float vix = threats[i].vx, viy = threats[i].vy;
        float mi = sqrtf(std::fma(vix, vix, viy * viy));
        if (mi < 100.0f) continue;
        int same = 0;
        for (int j = 0; j < n; j++)
        {
            if (i == j) continue;
            float vjx = threats[j].vx, vjy = threats[j].vy;
            float mj = sqrtf(std::fma(vjx, vjx, vjy * vjy));
            if (mj < 100.0f) continue;
            float dot = std::fma(vix, vjx, viy * vjy);
            if (dot / (mi * mj) > BURST_DOT_MIN) same++;
        }
        if (same >= 1)
        {
            acc_x += vix / mi;
            acc_y += viy / mi;
            burst_threats++;
        }
    }
    if (burst_threats >= 2)
    {
        float mag = sqrtf(std::fma(acc_x, acc_x, acc_y * acc_y));
        if (mag > 0.001f)
        {
            g_burst_vx = acc_x / mag;
            g_burst_vy = acc_y / mag;
            g_has_burst = true;
        }
    }
}

static void enter_post_dodge(uint64_t now) noexcept
{
    if (headingIdx < 0) return;
    if (postDodgeUntil != 0) return;
    if (commitDirX == 0.0f && commitDirY == 0.0f) return;
    postDodgeUntil = now + POST_DODGE_HOLD_MS;
    postDodgeDirX = commitDirX;
    postDodgeDirY = commitDirY;
}

static bool try_post_dodge(float mx, float my, uint64_t now)
{
    if (postDodgeUntil == 0) return false;
    if (now >= postDodgeUntil) return false;
    float mvx = CHAR_SPEED * postDodgeDirX;
    float mvy = CHAR_SPEED * postDodgeDirY;
    float myRadius = own_r < SPEED_MIN ? HITBOX_FALLBACK : own_r;
    for (const auto& t : threats)
    {
        float ca = swept_ca(mx, my, mvx, mvy, t.x, t.y, t.vx, t.vy);
        if (ca > t.rad + myRadius + POST_DODGE_CA_MARGIN) continue;
        float ttc = timeToCollision(mx, my, mvx, mvy, t);
        if (ttc > 0.0f && ttc < POST_DODGE_OVERRIDE_TTC)
        {
            postDodgeUntil = 0;
            return false;
        }
    }
    int32_t tx = clamp_axis(mx + postDodgeDirX * REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + postDodgeDirY * REACH, wall_cache_h());
    enqueue(tx, ty);
    ad_target_x = tx;
    ad_target_y = ty;
    active = 1;
    moving = 1;
    return true;
}
}

void autododge_reset()
{
    ad_best_dir = -1;
    lastHpSeen = 0;
    pendingHit = 0;
    pendingDmg = 0;
    dirLockUntil = 0;
    ad_char_n = 0;
    ad_proj_n = 0;
    ad_enemy_n = 0;
    ad_threat_n = 0;
    ad_vp_n = 0;
    ad_obj_n = 0;
    ad_cat_n = 0;
    ad_cat_ready = 0;
    ad_nv_n = 0;
    proj_cache_storage.clear();
    proj_cache_index.clear();
    headingIdx = -1;
    lastDangerTs = 0;
    lastSentAt = 0;
    commitDirX = 0.0f;
    commitDirY = 0.0f;
    ad_target_x = INT32_MAX;
    ad_target_y = INT32_MAX;
    active = 0;
    moving = 0;
    ad_vx = 0.0f;
    ad_vy = 0.0f;
    prev_mx = 0.0f;
    prev_my = 0.0f;
    prev_time = 0;
    g_has_burst = false;
    g_burst_vx = 0.0f;
    g_burst_vy = 0.0f;
    postDodgeUntil = 0;
    postDodgeDirX = 0.0f;
    postDodgeDirY = 0.0f;
    dangerHoldUntil = 0;
}

void autododge()
{
    ad_char_n = ad_proj_n = ad_enemy_n = ad_threat_n = 0;
    ad_vp_n = 0;
    ad_obj_n = 0;
    ad_nv_n = 0;
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
    read_int(own_elem + static_cast<uintptr_t>(OBJ_GLOBALID_OFF), &own_gid);

    uint64_t now = us() / 1000;
    float mx = (float)own_x, my = (float)own_y;
    float myRadius = own_r < SPEED_MIN ? HITBOX_FALLBACK : own_r;
    if (fabsf(mx) < MIN_POS && fabsf(my) < MIN_POS) return;

    if (prev_time && now > prev_time)
    {
        float dt = (float)(now - prev_time) / 1000.0f;
        if (dt > DT_MIN && dt < DT_MAX && (prev_mx != 0.0f || prev_my != 0.0f))
        {
            ad_vx = (mx - prev_mx) / dt;
            ad_vy = (my - prev_my) / dt;
            float vmag = sqrtf(std::fma(ad_vx, ad_vx, ad_vy * ad_vy));
            if (vmag > 4000.0f)
            {
                ad_vx = 0.0f;
                ad_vy = 0.0f;
            }
        }
        else if (prev_mx == 0.0f && prev_my == 0.0f)
        {
            ad_vx = 0.0f;
            ad_vy = 0.0f;
        }
    }
    prev_mx = mx;
    prev_my = my;
    prev_time = now;

    CHAR_SPEED = own_speed < SPEED_MIN ? SPEED_FALLBACK : own_speed;
    threats.clear();
    threats.reserve(32);
    float bodyR = myRadius + SKIN;
    float zoneSq = AWARE * AWARE;

    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i]) continue;
        const uint8_t* obj = obj_block[i];
        uintptr_t objPtr = obj_snapshot[i], dataPtr = 0;
        int32_t nb_pid, nb_x, nb_y, nb_team;
        memcpy(&nb_pid, obj + OBJ_GLOBALID_OFF, sizeof(nb_pid));
        memcpy(&nb_x, obj + OBJ_X_OFF, sizeof(nb_x));
        memcpy(&nb_y, obj + OBJ_Y_OFF, sizeof(nb_y));
        memcpy(&nb_team, obj + OBJ_TEAM_OFF, sizeof(nb_team));
        memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));
        if (!dataPtr || !pointer_plausible(dataPtr)) continue;

        uint8_t data[PROJ_DATA_BLOCK_SIZE];
        if (!read_bytes(dataPtr, data, sizeof(data))) continue;

        uintptr_t dataVtable;
        memcpy(&dataVtable, data, sizeof(dataVtable));
        if (!dataVtable || !pointer_plausible(dataVtable)) continue;
        uintptr_t vtRva = dataVtable - base;
        if (vtRva < DC_RVA_LO || vtRva >= DC_RVA_LO + DC_RVA_SIZE) continue;

        if (vtRva != VTABLE_PROJ_DATA_RVA)
        {
            if (char_like(vtRva, obj))
            {
                int32_t cteam, chp, cgid, cx_i, cy_i;
                memcpy(&cteam, obj + OBJ_TEAM_OFF, sizeof(cteam));
                memcpy(&chp, obj + OBJ_HP_OFF, sizeof(chp));
                memcpy(&cgid, obj + OBJ_GLOBALID_OFF, sizeof(cgid));
                memcpy(&cx_i, obj + OBJ_X_OFF, sizeof(cx_i));
                memcpy(&cy_i, obj + OBJ_Y_OFF, sizeof(cy_i));
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
            ProjectileState state{};
            state.lastX = px_i;
            state.lastY = py_i;
            state.prevX = px;
            state.prevY = py;
            state.lastTs = now;
            int32_t ind, projSpeed, projRadius, projEarly;
            if (!read_int(objPtr + PROJECTILE_ISINDIRECT_OFF, &ind)) continue;
            read_int(dataPtr + PROJECTILEDATA_SPEED_OFF, &projSpeed);
            memcpy(&projRadius,
                   data + PROJECTILEDATA_RADIUS_OFF,
                   sizeof(projRadius));
            read_int(dataPtr + EARLY_TICKS_OFF, &projEarly);
            state.pid = pid;
            state.rawSpeed = projSpeed;
            state.rawRadius = projRadius;
            state.rawEarly = projEarly;
            state.isIndirect = ind != 0;
            state.isBeam = false;
            state.isRicochet = false;
            state.ricochetUntil = 0;
            state.speed = (float)projSpeed * SPEED_SCALE;
            state.radius = (float)projRadius * RADIUS_SCALE;
            state.earlyTicks = projEarly;
            state.spawnedAt =
                now - (uint64_t)state.earlyTicks * (uint64_t)EARLY_TICK_MS;
            state.spawnX = px;
            state.spawnY = py;
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
            if (dt > DT_MIN && dt < DT_MAX && step * step >= POS_DELTA_MIN_SQ)
            {
                float inst = step / dt;
                if (inst >= 100.0f && inst <= 9000.0f)
                    state.meas = state.meas > 0.0f
                                     ? state.meas * MEAS_KEEP + inst * MEAS_NEW
                                     : inst;
                float ux = ddx / step, uy = ddy / step;
                bool flipped = state.hasVelocity &&
                               ux * state.dir_x + uy * state.dir_y < 0.0f;
                float dot_prev = state.hasVelocity
                                     ? (ux * state.dir_x + uy * state.dir_y)
                                     : 1.0f;
                if (state.hasVelocity && !flipped)
                {
                    ux = state.dir_x * DIR_KEEP + ux * DIR_NEW;
                    uy = state.dir_y * DIR_KEEP + uy * DIR_NEW;
                    float norm = sqrtf(std::fma(ux, ux, uy * uy));
                    if (norm > VV_EPS)
                    {
                        ux /= norm;
                        uy /= norm;
                    }
                }
                if (state.hasVelocity && dot_prev < RICOCHET_DOT_THRESHOLD &&
                    step > RICOCHET_MIN_STEP)
                {
                    state.isRicochet = true;
                    state.ricochetUntil = now + RICOCHET_HOLD_MS;
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
        state.seen++;
        if (state.isRicochet && now >= state.ricochetUntil)
            state.isRicochet = false;
        if (!state.hasVelocity && state.speed > 0.0f)
        {
            float dx = px - state.spawnX, dy = py - state.spawnY;
            float len = sqrtf(std::fma(dx, dx, dy * dy));
            if (len > 1.0f)
            {
                state.dir_x = dx / len;
                state.dir_y = dy / len;
                state.last_ux = state.dir_x;
                state.last_uy = state.dir_y;
                state.hasVelocity = true;
            }
        }
        float dx = px - mx, dy = py - my;
        if (std::fma(dx, dx, dy * dy) > zoneSq) continue;

        float mag = state.speed;
        if (mag < DEF_SPEED_MIN || mag > DEF_SPEED_MAX) mag = state.meas;
        bool blob = !state.isBeam && !state.isIndirect && mag > 0.0f &&
                    mag < BLOB_SPEED_MAX;
        bool tracked = state.hasVelocity && mag > SPEED_MIN;
        float vx = (blob || !tracked) ? 0.0f : state.dir_x * mag;
        float vy = (blob || !tracked) ? 0.0f : state.dir_y * mag;
        float speedNow = sqrtf(std::fma(vx, vx, vy * vy));
        float ux = speedNow > SPEED_MIN ? vx / speedNow : 0.0f;
        float uy = speedNow > SPEED_MIN ? vy / speedNow : 0.0f;

        if (!blob && speedNow >= SPEED_MIN && !state.isRicochet)
        {
            float playerAlong = std::fma(mx - px, ux, (my - py) * uy);
            float distNow =
                sqrtf(std::fma(mx - px, mx - px, (my - py) * (my - py)));
            float distPrev =
                sqrtf(std::fma(mx - state.prevX,
                               mx - state.prevX,
                               (my - state.prevY) * (my - state.prevY)));
            float sweepDist = swept_ca(mx, my, ad_vx, ad_vy, px, py, vx, vy);
            bool movingAway = playerAlong < PLAYER_ALONG_MIN;
            bool growing = distNow > distPrev;
            bool farEnough =
                sweepDist > (state.radius + myRadius + BEHIND_MARGIN);
            if (movingAway && growing && farEnough) continue;
            if (!state.isIndirect &&
                !wall_los(px,
                          py,
                          px + ux * (playerAlong > 0.0f ? playerAlong : 0.0f),
                          py + uy * (playerAlong > 0.0f ? playerAlong : 0.0f),
                          WALL_BLOCKS_PROJECTILES))
                continue;
        }
        Threat item{};
        item.x = px;
        item.y = py;
        item.prev_x = state.prevX;
        item.prev_y = state.prevY;
        item.vx = vx;
        item.vy = vy;
        item.rad = state.radius + myRadius;
        item.mag = mag;
        item.ind = state.isIndirect ? 1 : 0;
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
            if (now - proj_cache_storage[i].lastSeen > 1024) continue;
            if (writeIdx != i)
                proj_cache_storage[writeIdx] = proj_cache_storage[i];
            writeIdx++;
        }
        proj_cache_storage.resize(writeIdx);
        if (writeIdx != proj_cache_index.size())
            rebuild_projectile_cache_index();
    }

    detect_burst();

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
        ad_threat_spd[slot] = t.mag;
        ad_threat_ind[slot] = t.ind;
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

    if (postDodgeUntil > 0 && now >= postDodgeUntil)
    {
        endDodge();
        return;
    }
    if (try_post_dodge(mx, my, now)) return;

    if (threats.empty())
    {
        if (now - lastDangerTs > RELEASE_GRACE_MS)
        {
            enter_post_dodge(now);
            if (try_post_dodge(mx, my, now)) return;
            endDodge();
        }
        return;
    }

    float stayClearSq = INF_F;
    bool hasRicochet = false;
    for (const auto& t : threats)
    {
        if (t.ricochet) hasRicochet = true;
        float relx = t.x - mx, rely = t.y - my;
        float vrx = t.vx - ad_vx, vry = t.vy - ad_vy;
        float vv = std::fma(vrx, vrx, vry * vry);
        float ts = vv > VV_EPS ? -std::fma(relx, vrx, rely * vry) / vv : 0.0f;
        ts = ts < 0.0f ? 0.0f : (ts > HORIZON_S ? HORIZON_S : ts);
        float cx = std::fma(vrx, ts, relx);
        float cy = std::fma(vry, ts, rely);
        float clearSq = std::fma(cx, cx, cy * cy) - t.rad * t.rad;
        if (clearSq < stayClearSq) stayClearSq = clearSq;
    }
    bool inDanger = stayClearSq < (ENGAGE * ENGAGE);
    if (!inDanger)
    {
        for (const auto& t : threats)
        {
            float ttc = timeToCollision(mx, my, ad_vx, ad_vy, t);
            if (ttc > 0.0f && ttc < REACT_S)
            {
                inDanger = true;
                break;
            }
        }
    }
    if (!inDanger && hasRicochet) inDanger = true;
    if (inDanger)
    {
        lastDangerTs = now;
        dangerHoldUntil =
            now + (hasRicochet ? RICOCHET_HOLD_MS : DANGER_HOLD_MS);
    }
    if (!inDanger && now > dangerHoldUntil &&
        (headingIdx < 0 || now - lastDangerTs > RELEASE_GRACE_MS))
    {
        enter_post_dodge(now);
        if (try_post_dodge(mx, my, now)) return;
        endDodge();
        return;
    }

    float ex = 0.0f, ey = 0.0f;
    for (const auto& t : threats)
    {
        if (timeToClosestApproach(t, mx, my, ad_vx, ad_vy) < 0.0f &&
            !t.ricochet)
            continue;
        float dx = mx - t.x, dy = my - t.y;
        float distSq = std::fma(dx, dx, dy * dy);
        if (distSq > 1.0f)
        {
            float dist = sqrtf(distSq);
            float w = SPEED_MIN / (dist + ESCAPE_DIST_BIAS);
            if (t.ricochet) w *= 2.0f;
            ex = std::fma(dx / dist, w, ex);
            ey = std::fma(dy / dist, w, ey);
        }
    }
    float elen = sqrtf(std::fma(ex, ex, ey * ey));
    if (elen > 0.001f)
    {
        ex /= elen;
        ey /= elen;
    }

    if (g_has_burst)
    {
        float px1 = -g_burst_vy, py1 = g_burst_vx;
        float px2 = g_burst_vy, py2 = -g_burst_vx;
        float dot1 = ex * px1 + ey * py1;
        float dot2 = ex * px2 + ey * py2;
        float perpX = (dot1 >= dot2) ? px1 : px2;
        float perpY = (dot1 >= dot2) ? py1 : py2;
        ex = ex * (1.0f - BURST_PERP_BLEND) + perpX * BURST_PERP_BLEND;
        ey = ey * (1.0f - BURST_PERP_BLEND) + perpY * BURST_PERP_BLEND;
        elen = sqrtf(std::fma(ex, ex, ey * ey));
        if (elen > 0.001f)
        {
            ex /= elen;
            ey /= elen;
        }
    }

    float gx = 0.0f, gy = 0.0f;
    if (wall_cache_w() > 0 && wall_cache_h() > 0)
    {
        gx = (wall_cache_w() * WALL_TILE_SIZE * HALF) - mx;
        gy = (wall_cache_h() * WALL_TILE_SIZE * HALF) - my;
        float glen = sqrtf(std::fma(gx, gx, gy * gy));
        if (glen > 0.001f)
        {
            gx /= glen;
            gy /= glen;
        }
    }

    bool hasPrev = commitDirX != 0.0f || commitDirY != 0.0f;
    float prevDirX = commitDirX, prevDirY = commitDirY;
    int bestIdx = 0;
    float bestDanger = INF_F, bestInterest = -INF_F;

    for (int i = 0; i < CARDINALS; i++)
    {
        float d =
            dangerFor(mx, my, cardDirX[i], cardDirY[i], CHAR_SPEED, bodyR);
        float dotEscape = cardDirX[i] * ex + cardDirY[i] * ey;
        float escapeScore = (dotEscape + SPEED_MIN) * HALF;
        float dotGoal = cardDirX[i] * gx + cardDirY[i] * gy;
        float goalScore = (dotGoal + SPEED_MIN) * HALF;
        float inter = std::fma(
            escapeScore, INTEREST_ESCAPE_W, goalScore * INTEREST_GOAL_W);
        if (hasPrev)
        {
            float dotPrev = cardDirX[i] * prevDirX + cardDirY[i] * prevDirY;
            if (dotPrev < INTEREST_BACK_DOT) inter -= INTEREST_BACK_PENALTY;
        }
        bool better = false;
        if (d < bestDanger - DANGER_PICK_EPS)
            better = true;
        else if (fabsf(d - bestDanger) <= DANGER_PICK_EPS &&
                 inter > bestInterest)
            better = true;
        if (better)
        {
            bestDanger = d;
            bestInterest = inter;
            bestIdx = i;
        }
    }

    float outDirX = cardDirX[bestIdx], outDirY = cardDirY[bestIdx];
    if (hasPrev)
    {
        float prevD = dangerFor(mx, my, prevDirX, prevDirY, CHAR_SPEED, bodyR);
        if (now < dirLockUntil || prevD <= bestDanger + HYSTERESIS_MARGIN)
        {
            outDirX = prevDirX;
            outDirY = prevDirY;
        }
    }
    if (hasPrev && (outDirX != prevDirX || outDirY != prevDirY))
        dirLockUntil = now + DIR_HOLD_MS;
    commitDirX = outDirX;
    commitDirY = outDirY;
    headingIdx = bestIdx;
    ad_best_dir = bestIdx;

    int32_t tx = clamp_axis(mx + outDirX * REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + outDirY * REACH, wall_cache_h());
    int32_t ddx = tx - ad_target_x, ddy = ty - ad_target_y;
    if ((float)(ddx * ddx + ddy * ddy) > POS_CHANGE_MIN_SQ ||
        now - lastSentAt > SEND_MIN_MS)
    {
        lastSentAt = now;
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    active = 1;
    moving = 1;
}
