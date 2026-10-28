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
#include <map>
#include <cstdint>
#include <vector>

int active = 0;
int moving = 0;
int in_move = 0;
int32_t ad_target_x = INT32_MAX;
int32_t ad_target_y = INT32_MAX;
int ad_best_dir = -1;
int ad_char_n = 0;
int ad_proj_n = 0;
int ad_enemy_n = 0;
int ad_threat_n = 0;
int ad_threat_pid[AD_REPORT_MAX] = {0};
float ad_threat_x[AD_REPORT_MAX] = {0.0f};
float ad_threat_y[AD_REPORT_MAX] = {0.0f};
float ad_threat_vx[AD_REPORT_MAX] = {0.0f};
float ad_threat_vy[AD_REPORT_MAX] = {0.0f};
float ad_threat_r[AD_REPORT_MAX] = {0.0f};
float ad_threat_d[AD_REPORT_MAX] = {0.0f};
float ad_threat_ttc[AD_REPORT_MAX] = {0.0f};
float ad_threat_ca[AD_REPORT_MAX] = {0.0f};
float ad_threat_score[AD_REPORT_MAX] = {0.0f};
float ad_threat_spd[AD_REPORT_MAX] = {0.0f};
char ad_vp[AD_VP_MAX][128] = {{0}};
int ad_vp_n = 0;
char ad_threat_name[AD_REPORT_MAX][40] = {{0}};
char ad_probe[256] = {0};
char ad_obj[AD_OBJ_MAX][88] = {{0}};
int ad_obj_n = 0;
char ad_cat[AD_CAT_MAX][420] = {{0}};
int ad_cat_n = 0;
char ad_diff[600] = {0};
int ad_cat_ready = 0;
char ad_hit[256] = {0};
char ad_nv[AD_NV_MAX][96] = {{0}};
int ad_nv_n = 0;
int ad_enemy_pid[AD_ENEMY_MAX] = {0};
float ad_enemy_x[AD_ENEMY_MAX] = {0.0f};
float ad_enemy_y[AD_ENEMY_MAX] = {0.0f};
int ad_enemy_hp[AD_ENEMY_MAX] = {0};
float ad_enemy_d[AD_ENEMY_MAX] = {0.0f};

static struct
{
    float AWARE = 4400.0f;
    float REACT_S = 0.35f;
    int DIR_COUNT = 32;
    float SKIN = 90.0f;
    float REACH = 700.0f;
    float HORIZON_S = 1.55f;
    float MOMENTUM = 40.0f;
    float SWITCH_MARGIN = 140.0f;
    float SWITCH_MIN = 35.0f;
    float TURN_DOT = -0.15f;
    float SMOOTH = 0.48f;
    float ENGAGE = 120.0f;
    uint32_t RELEASE_GRACE_MS = 120;
    float WALL_HIT = 9000.0f;
    int PROBE_N = 6;
    float PROBE_T = 0.12f;
    float WALL_BODY = 300.0f;
    uint32_t SEND_MIN_MS = 16;
    float MIN_POS = 15.0f;
} TUNE;

namespace
{
constexpr uintptr_t EARLY_TICKS_OFF = 0x80;

constexpr float SPEED_SCALE = 1.0f;
constexpr float RADIUS_SCALE = 1.0f;
constexpr int32_t EARLY_TICK_MS = 16;

constexpr float INF_F = std::numeric_limits<float>::infinity();
constexpr float VV_EPS = 1e-6f;
constexpr float DT_MIN = 0.002f;
constexpr float DT_MAX = 0.3f;
constexpr float POS_DELTA_MIN_SQ = 16.0f;
constexpr float PLAYER_ALONG_MIN = 0.0f;
constexpr float BLOB_SPEED_MAX = 1600.0f;
constexpr float DIR_KEEP = 0.55f;
constexpr float MEAS_KEEP = 0.5f;
constexpr float MEAS_NEW = 0.5f;
constexpr float DEF_SPEED_MIN = 200.0f;
constexpr float DEF_SPEED_MAX = 20000.0f;
constexpr float DIR_NEW = 0.45f;
constexpr float HITBOX_FALLBACK = 120.0f;
constexpr float SPEED_FALLBACK = 720.0f;
constexpr float BODY_EXTRA_R = 120.0f;
constexpr float DANGER_RADIUS = 300.0f;
constexpr float TTC_MIN = 0.12f;
constexpr float DANGER_WALL = 0.9f;
constexpr float DANGER_SUM_WEIGHT = 0.15f;
constexpr float DANGER_MAX = 1.0f;
constexpr float DANGER_PICK_EPS = 0.02f;
constexpr float INTEREST_ESCAPE_W = 0.7f;
constexpr float INTEREST_GOAL_W = 0.3f;
constexpr float INTEREST_BACK_DOT = -0.5f;
constexpr float INTEREST_BACK_PENALTY = 0.2f;
constexpr float ESCAPE_DIST_BIAS = 100.0f;
constexpr float POS_CHANGE_MIN_SQ = 64.0f;
constexpr float QUADRATIC_K = 2.0f;
constexpr float HALF = 0.5f;
constexpr float SPEED_MIN = 1.0f;
constexpr float HYSTERESIS_MARGIN = 0.25f;
constexpr uint64_t DIR_HOLD_MS = 200;
constexpr int AD_HIT_SCAN = 64;

constexpr int CARDINALS = 8;
constexpr float DIAG = 0.70710678f;
static const float cardDirX[CARDINALS] = {
    1.0f, DIAG, 0.0f, -DIAG, -1.0f, -DIAG, 0.0f, DIAG};
static const float cardDirY[CARDINALS] = {
    0.0f, DIAG, 1.0f, DIAG, 0.0f, -DIAG, -1.0f, -DIAG};

struct Threat
{
    float x, y, vx, vy, rad, mag;
    int32_t pid;
    char name[40];
};
struct Body
{
    float x, y, rad;
};

struct ProjectileState
{
    float dir_x, dir_y, speed, radius, meas;
    char name[128];
    float spawnX, spawnY;
    int earlyTicks;
    uint64_t spawnedAt, lastSeen, lastTs;
    int32_t lastX, lastY, pid;
    int32_t seen;
    bool probed;
    float last_ux, last_uy;
    bool isIndirect, isBeam, hasVelocity;
    int32_t rawSpeed, rawRadius, rawEarly;
};

constexpr int PROBE_MAX_SAMPLES = 48;
constexpr int PROBE_BYTES = 0x300;
constexpr int PROBE_MIN_HITS = 10;

struct ProbeSample
{
    uint8_t bytes[PROBE_BYTES];
    float ux, uy, ang, spd;
    int valid_len;
};

static ProbeSample probe_samples[PROBE_MAX_SAMPLES];
static int probe_count = 0;
static int probe_analysis_done = 0;

static float mag_of(const ProjectileState& st)
{
    float m = st.speed;
    if (m < DEF_SPEED_MIN || m > DEF_SPEED_MAX) m = st.meas;
    return m;
}

static void probe_sample(uintptr_t objPtr, float ux, float uy, float spd)
{
    ProbeSample* ps = nullptr;
    if (probe_count >= PROBE_MAX_SAMPLES) return;
    if (spd <= SPEED_MIN) return;
    ps = &probe_samples[probe_count];
    memset(ps->bytes, 0, PROBE_BYTES);
    ps->valid_len = 0;
    for (int chunk = 0; chunk < PROBE_BYTES; chunk += 0x100)
    {
        if (!read_bytes(objPtr + static_cast<uintptr_t>(chunk),
                        ps->bytes + chunk,
                        0x100))
        {
            break;
        }
        ps->valid_len = chunk + 0x100;
    }
    if (ps->valid_len < 0x40) return;
    ps->ux = ux;
    ps->uy = uy;
    ps->ang = atan2f(uy, ux);
    ps->spd = spd;
    probe_count++;
}

static void probe_add(const char* what, int off, int hits, int total)
{
    if (ad_vp_n >= AD_VP_MAX) return;
    snprintf(ad_vp[ad_vp_n],
             sizeof(ad_vp[ad_vp_n]),
             "%s off=0x%x hit=%d/%d",
             what,
             static_cast<unsigned>(off),
             hits,
             total);
    ad_vp_n++;
}

static void probe_analyze()
{
    int n = probe_count;
    int need = 0;
    if (probe_analysis_done || n < PROBE_MIN_HITS) return;
    probe_analysis_done = 1;
    need = n - (n / 10);
    for (int off = 0; off + 8 <= PROBE_BYTES; off += 4)
    {
        int pair = 0;
        int pairRot = 0;
        int vecv = 0;
        int rad = 0;
        int deg = 0;
        int spd = 0;
        int ispd = 0;
        for (int i = 0; i < n; i++)
        {
            const ProbeSample& ps = probe_samples[i];
            float f0 = 0.0f;
            if (off + 8 > ps.valid_len) continue;
            float f1 = 0.0f;
            int32_t iv = 0;
            memcpy(&f0, ps.bytes + off, 4);
            memcpy(&f1, ps.bytes + off + 4, 4);
            if (!(f0 == f0) || !(f1 == f1)) continue;
            if (fabsf(f0) > 1e18f || fabsf(f1) > 1e18f) continue;
            if (fabsf(f0 - ps.ux) < 0.03f && fabsf(f1 - ps.uy) < 0.03f) pair++;
            if (fabsf(f0 - ps.uy) < 0.03f && fabsf(f1 + ps.ux) < 0.03f)
                pairRot++;
            if (fabsf(f0 - ps.ux * ps.spd) < 60.0f &&
                fabsf(f1 - ps.uy * ps.spd) < 60.0f)
                vecv++;
            if (fabsf(f0 - ps.ang) < 0.02f) rad++;
            if (fabsf(f0 - ps.ang * 57.29578f) < 1.5f) deg++;
            if (fabsf(f0 - ps.spd) < 8.0f) spd++;
            memcpy(&iv, ps.bytes + off, 4);
            if (iv > 0 && iv - static_cast<int32_t>(ps.spd) < 8 &&
                static_cast<int32_t>(ps.spd) - iv < 8)
                ispd++;
        }
        if (pair >= need) probe_add("vec", off, pair, n);
        if (pairRot >= need) probe_add("vec90", off, pairRot, n);
        if (vecv >= need) probe_add("vec_v", off, vecv, n);
        if (rad >= need) probe_add("ang_rad", off, rad, n);
        if (deg >= need) probe_add("ang_deg", off, deg, n);
        if (spd >= need) probe_add("spd_f", off, spd, n);
        if (ispd >= need) probe_add("spd_i", off, ispd, n);
    }
    if (ad_vp_n == 0)
    {
        snprintf(ad_vp[0], sizeof(ad_vp[0]), "no candidates n=%d", n);
        ad_vp_n = 1;
    }
}

constexpr uintptr_t CAT_LO = 0x0;
constexpr uintptr_t CAT_HI = 0x220;
constexpr int CAT_OFF_N = static_cast<int>((CAT_HI - CAT_LO) / 4);
constexpr int CAT_SAMPLE_MAX = 12;

struct ClassCat
{
    uintptr_t vt;
    int samples;
    char name[40];
    int32_t mn[CAT_OFF_N];
    int32_t mx[CAT_OFF_N];
};

static ClassCat cats[AD_CAT_MAX];
static int cat_n = 0;
static int cat_dirty = 1;
static uint64_t cat_print_ts = 0;

static void cat_sample(uintptr_t vt, const char* name, uintptr_t dataPtr)
{
    ClassCat* slot = nullptr;
    for (int i = 0; i < cat_n; i++)
    {
        if (cats[i].vt == vt)
        {
            slot = &cats[i];
            break;
        }
    }
    if (!slot)
    {
        if (cat_n >= AD_CAT_MAX) return;
        slot = &cats[cat_n];
        slot->vt = vt;
        slot->samples = 0;
        snprintf(slot->name, sizeof(slot->name), "%s", name ? name : "");
        for (int i = 0; i < CAT_OFF_N; i++)
        {
            slot->mn[i] = INT32_MAX;
            slot->mx[i] = INT32_MIN;
        }
        cat_n++;
        cat_dirty = 1;
    }
    if (slot->samples >= CAT_SAMPLE_MAX) return;
    for (int i = 0; i < CAT_OFF_N; i++)
    {
        int32_t value = 0;
        if (!read_int(dataPtr + CAT_LO + static_cast<uintptr_t>(i) * 4, &value))
            continue;
        if (value < slot->mn[i]) slot->mn[i] = value;
        if (value > slot->mx[i]) slot->mx[i] = value;
    }
    slot->samples++;
    cat_dirty = 1;
}

static void cat_build(int idx)
{
    ClassCat& c = cats[idx];
    char* out = ad_cat[idx];
    int cap = static_cast<int>(sizeof(ad_cat[idx]));
    int len = snprintf(out,
                       static_cast<size_t>(cap),
                       "vt=0x%x n=%d %s",
                       static_cast<unsigned>(c.vt),
                       c.samples,
                       c.name[0] ? c.name : "-");
    int shown = 0;
    for (int pass = 0; pass < 2; pass++)
    {
        for (int i = 0; i < CAT_OFF_N; i++)
        {
            if (len < 0 || len >= cap - 24) break;
            if (c.mn[i] > c.mx[i]) continue;
            bool varying = c.mn[i] == 0 && c.mx[i] == 1;
            bool set = c.mn[i] == 1 && c.mx[i] == 1;
            if (pass == 0 && !varying) continue;
            if (pass == 1 && (!set || shown >= 12)) continue;
            len += snprintf(out + len,
                            static_cast<size_t>(cap - len),
                            " %x:%s",
                            static_cast<unsigned>(CAT_LO + i * 4),
                            varying ? "0-1" : "1");
            shown++;
        }
    }
}

static void cat_build_diff()
{
    int cap = static_cast<int>(sizeof(ad_diff));
    int len = snprintf(ad_diff,
                       static_cast<size_t>(cap),
                       "class diff");
    if (cat_n < 2)
    {
        snprintf(ad_diff, static_cast<size_t>(cap), "class diff single class");
        return;
    }
    for (int i = 0; i < CAT_OFF_N && len > 0 && len < cap - 40; i++)
    {
        bool differs = false;
        for (int k = 1; k < cat_n; k++)
        {
            if (cats[k].mx[i] != cats[0].mx[i] ||
                cats[k].mn[i] != cats[0].mn[i])
            {
                differs = true;
                break;
            }
        }
        if (!differs) continue;
        len += snprintf(ad_diff + len,
                        static_cast<size_t>(cap - len),
                        " %x:",
                        static_cast<unsigned>(CAT_LO + i * 4));
        for (int k = 0; k < cat_n && len < cap - 16; k++)
        {
            len += snprintf(ad_diff + len,
                            static_cast<size_t>(cap - len),
                            "%s%d",
                            k ? "|" : "",
                            cats[k].mx[i]);
        }
    }
}

static constexpr int AD_NOTE_MAX = 16;
static int32_t notePid[AD_NOTE_MAX];
static uintptr_t noteVt[AD_NOTE_MAX];
static int32_t noteTeam[AD_NOTE_MAX];
static float notePosX[AD_NOTE_MAX];
static float notePosY[AD_NOTE_MAX];
static const char* noteWhy[AD_NOTE_MAX];
static int noteN = 0;

static void note_obj(int32_t pid,
                     uintptr_t vt,
                     int32_t team,
                     float x,
                     float y,
                     const char* why)
{
    if (noteN >= AD_NOTE_MAX) return;
    notePid[noteN] = pid;
    noteVt[noteN] = vt;
    noteTeam[noteN] = team;
    notePosX[noteN] = x;
    notePosY[noteN] = y;
    noteWhy[noteN] = why;
    noteN++;
}

static uintptr_t char_vt_rva = 0;
static int32_t own_gid = 0;
static std::vector<ProjectileState> proj_cache_storage;
static std::map<int32_t, size_t> proj_cache_index;

static void rebuild_projectile_cache_index()
{
    proj_cache_index.clear();
    for (size_t i = 0; i < proj_cache_storage.size(); i++)
        proj_cache_index[proj_cache_storage[i].pid] = i;
}

static std::vector<Body> bodies;
static std::vector<Threat> threats;
static int headingIdx = -1;
static uint64_t lastDangerTs = 0, lastGoodTs = 0, lastSentAt = 0;
static uint64_t dirLockUntil = 0;
static uint32_t lastHpSeen = 0;
static int pendingHit = 0;
static int pendingDmg = 0;
static float commitDirX = 0.0f, commitDirY = 0.0f;
static float CHAR_SPEED = SPEED_FALLBACK;

static float clearanceFor(const std::vector<Threat>& th,
                          const std::vector<Body>& bd,
                          float mx,
                          float my,
                          float mvx,
                          float mvy)
{
    float minClearSq = INF_F;
    for (const auto& t : th)
    {
        float relx = t.x - mx;
        float rely = t.y - my;
        float vrx = t.vx - mvx;
        float vry = t.vy - mvy;
        float vv = vrx * vrx + vry * vry;
        float ts = vv > VV_EPS ? -(relx * vrx + rely * vry) / vv : 0.0f;
        ts = ts < 0.0f ? 0.0f : (ts > TUNE.HORIZON_S ? TUNE.HORIZON_S : ts);
        float cx = relx + vrx * ts;
        float cy = rely + vry * ts;
        float clearSq = (cx * cx + cy * cy) - (t.rad * t.rad);
        if (clearSq < minClearSq) minClearSq = clearSq;
    }
    for (const auto& b : bd)
    {
        float dx = b.x - mx;
        float dy = b.y - my;
        float r = b.rad + BODY_EXTRA_R;
        float clearSq = (dx * dx + dy * dy) - (r * r);
        if (clearSq < minClearSq) minClearSq = clearSq;
    }
    return minClearSq;
}

static float
timeToCollision(float mx, float my, float mvx, float mvy, const Threat& t)
{
    float relx = t.x - mx;
    float rely = t.y - my;
    float vrx = t.vx - mvx;
    float vry = t.vy - mvy;
    float vv = vrx * vrx + vry * vry;
    if (vv < VV_EPS) return INF_F;
    float b = QUADRATIC_K * (relx * vrx + rely * vry);
    float c = relx * relx + rely * rely - t.rad * t.rad;
    float disc = b * b - QUADRATIC_K * QUADRATIC_K * vv * c;
    if (disc < 0.0f) return INF_F;
    float sd = sqrtf(disc);
    float t1 = (-b - sd) / (QUADRATIC_K * vv);
    float t2 = (-b + sd) / (QUADRATIC_K * vv);
    if (t1 > 0.0f) return t1;
    if (t2 > 0.0f) return t2;
    return INF_F;
}

static float timeToClosestApproach(const Threat& t, float mx, float my)
{
    float relx = t.x - mx;
    float rely = t.y - my;
    float vv = t.vx * t.vx + t.vy * t.vy;
    if (vv < VV_EPS) return -1.0f;
    return -(relx * t.vx + rely * t.vy) / vv;
}

static float
wallAhead(float mx, float my, float dirX, float dirY, float speed, float bodyR)
{
    if (!wall_ready()) return 0.0f;
    float step = speed * TUNE.PROBE_T;
    int blocked = 0;
    for (int s = 1; s <= TUNE.PROBE_N; s++)
    {
        if (wall_is_blocked_wide(mx + dirX * step * s,
                                 my + dirY * step * s,
                                 bodyR,
                                 WALL_BLOCKS_MOVEMENT))
            blocked++;
    }
    return TUNE.WALL_HIT * (float)blocked / (float)TUNE.PROBE_N;
}

static int32_t clamp_axis(float value, int tiles)
{
    if (tiles <= 0) return (int32_t)roundf(value);
    float limit = (float)tiles * WALL_TILE_SIZE - SPEED_MIN;
    if (limit <= 0.0f) return (int32_t)roundf(value);
    if (value < 0.0f) return 0;
    if (value > limit) return (int32_t)limit;
    return (int32_t)roundf(value);
}

static void clearHeading()
{
    headingIdx = -1;
    commitDirX = 0.0f;
    commitDirY = 0.0f;
}

static void endDodge()
{
    dirLockUntil = 0;
    if (headingIdx < 0) return;
    clearHeading();
    ad_target_x = INT32_MAX;
    ad_target_y = INT32_MAX;
    active = 0;
    moving = 0;
}

static uintptr_t call_fn(uintptr_t fn, uintptr_t a, uintptr_t b)
{
    if (!fn || !base) return 0;
    uintptr_t rva = (fn >= base) ? (fn - base) : fn;
    uintptr_t target = callable(rva);
    if (!target) return 0;
    using fn_t = uintptr_t (*)(uintptr_t, uintptr_t);
    return ((fn_t)target)(a, b);
}

static float
dangerFor(float mx, float my, float dx, float dy, float speed, float bodyR)
{
    float mvx = speed * dx;
    float mvy = speed * dy;
    float max_c = 0.0f;
    float sum_c = 0.0f;

    for (const auto& t : threats)
    {
        if (timeToClosestApproach(t, mx, my) < 0.0f) continue;

        float ttc = timeToCollision(mx, my, mvx, mvy, t);
        if (ttc < TTC_MIN) return DANGER_MAX;

        float relx = t.x - mx;
        float rely = t.y - my;
        float vrx = t.vx - mvx;
        float vry = t.vy - mvy;
        float vv = vrx * vrx + vry * vry;
        float ts = vv > VV_EPS ? -(relx * vrx + rely * vry) / vv : 0.0f;
        ts = ts < 0.0f ? 0.0f : (ts > TUNE.HORIZON_S ? TUNE.HORIZON_S : ts);
        float cx = relx + vrx * ts;
        float cy = rely + vry * ts;
        float m = sqrtf(cx * cx + cy * cy) - t.rad;

        float c = (DANGER_RADIUS - m) / DANGER_RADIUS;
        if (c < 0.0f) c = 0.0f;
        if (c > max_c) max_c = c;
        sum_c += c;
    }

    if (wallAhead(mx, my, dx, dy, speed, bodyR) > 0.0f) return DANGER_WALL;

    float danger = max_c + DANGER_SUM_WEIGHT * (sum_c - max_c);
    return danger > DANGER_MAX ? DANGER_MAX : danger;
}
}

static void snapshot_hit(float mx, float my)
{
    int len = 0;
    int picked = 0;
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
            float rx = threats[i].x - mx;
            float ry = threats[i].y - my;
            float dist = sqrtf(rx * rx + ry * ry);
            float vv =
                threats[i].vx * threats[i].vx + threats[i].vy * threats[i].vy;
            float ca = dist;
            if (vv > VV_EPS)
            {
                float al =
                    (rx * threats[i].vx + ry * threats[i].vy) / sqrtf(vv);
                float perp = dist * dist - al * al;
                ca = perp > 0.0f ? sqrtf(perp) : 0.0f;
            }
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
            len += snprintf(ad_hit + len,
                            sizeof(ad_hit) - static_cast<size_t>(len),
                            " | ca=%.0f ttc=%.2f r=%.0f",
                            bestCa,
                            timeToCollision(mx, my, 0.0f, 0.0f, threats[best]),
                            threats[best].rad);
        }
    }
    if (picked < 1 && len > 0 && len < static_cast<int>(sizeof(ad_hit)) - 8)
    {
        snprintf(
            ad_hit + len, sizeof(ad_hit) - static_cast<size_t>(len), " | none");
    }
    ad_obj_n = 0;
    for (int i = 0; i < noteN && i < AD_OBJ_MAX; i++)
    {
        float dx = notePosX[i] - mx;
        float dy = notePosY[i] - my;
        snprintf(ad_obj[i],
                 sizeof(ad_obj[i]),
                 "pid=%d vt=0x%x tm=%d d=%.0f %s",
                 notePid[i],
                 static_cast<unsigned>(noteVt[i]),
                 noteTeam[i],
                 sqrtf(dx * dx + dy * dy),
                 noteWhy[i]);
        ad_obj_n++;
    }
}

static void note_novelo(int32_t pid,
                        uintptr_t vtRva,
                        int32_t rawSpeed,
                        float rad,
                        float px,
                        float py,
                        float mx,
                        float my)
{
    float dx = px - mx;
    float dy = py - my;
    if (ad_nv_n >= AD_NV_MAX) return;
    snprintf(ad_nv[ad_nv_n],
             sizeof(ad_nv[ad_nv_n]),
             "pid=%d vt=0x%x spd=%d r=%.0f p=(%.0f,%.0f) d=%.0f",
             pid,
             static_cast<unsigned>(vtRva),
             rawSpeed,
             rad,
             px,
             py,
             sqrtf(dx * dx + dy * dy));
    ad_nv_n++;
}

static void probe_speed_field(uintptr_t dataPtr, int32_t pid, int32_t raw)
{
    int len = 0;
    if (ad_probe[0] != 0) return;
    len = snprintf(
        ad_probe, sizeof(ad_probe), "pid=%d speed@124=%d cand", pid, raw);
    for (uintptr_t off = 0x40; off <= 0x1f0 && len > 0 &&
                               len < static_cast<int>(sizeof(ad_probe)) - 16;
         off += 4)
    {
        int32_t cand = 0;
        if (!read_int(dataPtr + off, &cand)) continue;
        if (cand < 300 || cand > 9000) continue;
        len += snprintf(ad_probe + len,
                        sizeof(ad_probe) - static_cast<size_t>(len),
                        " %x=%d",
                        static_cast<unsigned>(off),
                        cand);
    }
}

static void learn_char_vtable()
{
    void* dataPtr = nullptr;
    void* vt = nullptr;
    if (char_vt_rva || !base || !own_elem) return;
    if (!read_ptr(own_elem + ELEM_DEF_OFF, &dataPtr) || !dataPtr) return;
    if (!pointer_plausible((uintptr_t)dataPtr)) return;
    if (!read_ptr((uintptr_t)dataPtr, &vt) || !vt) return;
    uintptr_t rva = (uintptr_t)vt - base;
    if (rva == 0 || rva == VTABLE_PROJ_DATA_RVA) return;
    if (rva < DC_RVA_LO || rva >= DC_RVA_LO + DC_RVA_SIZE) return;
    char_vt_rva = rva;
}

static bool char_like(uintptr_t vtRva, const uint8_t* obj)
{
    int32_t team = 0, hp = 0, gid = 0;
    if (char_vt_rva) return vtRva == char_vt_rva;
    if (vtRva == VTABLE_PROJ_DATA_RVA) return false;
    memcpy(&team, obj + OBJ_TEAM_OFF, sizeof(team));
    memcpy(&hp, obj + OBJ_HP_OFF, sizeof(hp));
    memcpy(&gid, obj + OBJ_GLOBALID_OFF, sizeof(gid));
    return team >= 0 && team <= 1 && hp > 0 && hp <= 200000 && gid > 0;
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
    proj_cache_storage.clear();
    proj_cache_index.clear();
    headingIdx = -1;
    lastDangerTs = 0;
    lastGoodTs = 0;
    lastSentAt = 0;
    commitDirX = 0.0f;
    commitDirY = 0.0f;
    ad_target_x = INT32_MAX;
    ad_target_y = INT32_MAX;
    active = 0;
    moving = 0;
}

void autododge()
{
    ad_char_n = 0;
    ad_proj_n = 0;
    ad_enemy_n = 0;
    ad_threat_n = 0;
    ad_nv_n = 0;
    uint64_t catNow = us() / 1000;
    if (cat_dirty && cat_n > 0 && catNow - cat_print_ts > 5000)
    {
        for (int i = 0; i < cat_n; i++) cat_build(i);
        cat_build_diff();
        ad_cat_n = cat_n;
        ad_cat_ready = 1;
        cat_dirty = 0;
        cat_print_ts = catNow;
    }
    noteN = 0;
    ad_obj_n = 0;
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
        active = 0;
        moving = 0;
        return;
    }
    learn_char_vtable();
    own_gid = 0;
    read_int(own_elem + static_cast<uintptr_t>(OBJ_GLOBALID_OFF), &own_gid);

    uint64_t now = us() / 1000;
    float mx = (float)own_x, my = (float)own_y;
    float myRadius = own_r < SPEED_MIN ? HITBOX_FALLBACK : own_r;
    if (fabsf(mx) < TUNE.MIN_POS && fabsf(my) < TUNE.MIN_POS) return;

    CHAR_SPEED = own_speed < SPEED_MIN ? SPEED_FALLBACK : own_speed;
    threats.clear();
    bodies.clear();
    threats.reserve(32);
    bodies.reserve(16);

    float bodyR = myRadius + TUNE.SKIN;
    float zoneSq = TUNE.AWARE * TUNE.AWARE;

    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i])
        {
            autododge_scanner_event(SCAN_AD_INVALID_SNAPSHOT);
            continue;
        }
        const uint8_t* obj = obj_block[i];
        uintptr_t objPtr = obj_snapshot[i], dataPtr = 0;
        int32_t nb_pid = 0, nb_x = 0, nb_y = 0, nb_team = -1;
        memcpy(&nb_pid, obj + OBJ_GLOBALID_OFF, sizeof(nb_pid));
        memcpy(&nb_x, obj + OBJ_X_OFF, sizeof(nb_x));
        memcpy(&nb_y, obj + OBJ_Y_OFF, sizeof(nb_y));
        memcpy(&nb_team, obj + OBJ_TEAM_OFF, sizeof(nb_team));
        memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));
        if (!dataPtr || !pointer_plausible(dataPtr))
        {
            autododge_scanner_event(SCAN_AD_BAD_DATA_POINTER);
            note_obj(nb_pid, 0, nb_team, (float)nb_x, (float)nb_y, "nodef");
            continue;
        }

        uint8_t data[PROJ_DATA_BLOCK_SIZE];
        if (!read_bytes(dataPtr, data, sizeof(data)))
        {
            autododge_scanner_event(SCAN_AD_DATA_READ_FAIL);
            note_obj(nb_pid, 0, nb_team, (float)nb_x, (float)nb_y, "noread");
            continue;
        }

        uintptr_t dataVtable = 0;
        memcpy(&dataVtable, data, sizeof(dataVtable));
        if (!dataVtable || !pointer_plausible(dataVtable))
        {
            autododge_scanner_event(SCAN_AD_BAD_VTABLE);
            note_obj(nb_pid, 0, nb_team, (float)nb_x, (float)nb_y, "badvt");
            continue;
        }
        uintptr_t vtRva = dataVtable - base;
        if (vtRva < DC_RVA_LO || vtRva >= DC_RVA_LO + DC_RVA_SIZE)
        {
            autododge_scanner_event(SCAN_AD_BAD_VTABLE);
            note_obj(nb_pid, vtRva, nb_team, (float)nb_x, (float)nb_y, "outimg");
            continue;
        }
        if (vtRva != VTABLE_PROJ_DATA_RVA)
        {
            if (char_like(vtRva, obj))
            {
                int32_t cteam = 0, chp = 0, cgid = 0, cx_i = 0, cy_i = 0;
                memcpy(&cteam, obj + OBJ_TEAM_OFF, sizeof(cteam));
                memcpy(&chp, obj + OBJ_HP_OFF, sizeof(chp));
                memcpy(&cgid, obj + OBJ_GLOBALID_OFF, sizeof(cgid));
                memcpy(&cx_i, obj + OBJ_X_OFF, sizeof(cx_i));
                memcpy(&cy_i, obj + OBJ_Y_OFF, sizeof(cy_i));
                ad_char_n++;
                note_obj(cgid, vtRva, cteam, (float)cx_i, (float)cy_i, "char");
                if (cteam != own_team_b && chp > 0 && cgid > 0 &&
                    cgid != own_gid && ad_enemy_n < AD_ENEMY_MAX)
                {
                    float edx = (float)cx_i - mx;
                    float edy = (float)cy_i - my;
                    ad_enemy_pid[ad_enemy_n] = cgid;
                    ad_enemy_x[ad_enemy_n] = (float)cx_i;
                    ad_enemy_y[ad_enemy_n] = (float)cy_i;
                    ad_enemy_hp[ad_enemy_n] = chp;
                    ad_enemy_d[ad_enemy_n] = sqrtf(edx * edx + edy * edy);
                    ad_enemy_n++;
                }
            }
            else
            {
                autododge_scanner_event(SCAN_AD_NOT_PROJECTILE_VTABLE);
                note_obj(nb_pid, vtRva, nb_team, (float)nb_x, (float)nb_y, "other");
            }
            continue;
        }
        ad_proj_n++;

        int32_t pid = 0, teamId = 0, px_i = 0, py_i = 0;
        memcpy(&pid, obj + OBJ_GLOBALID_OFF, sizeof(pid));
        memcpy(&teamId, obj + OBJ_TEAM_OFF, sizeof(teamId));
        memcpy(&px_i, obj + OBJ_X_OFF, sizeof(px_i));
        memcpy(&py_i, obj + OBJ_Y_OFF, sizeof(py_i));

        float px = (float)px_i, py = (float)py_i;
        if (pid == 0)
        {
            autododge_scanner_event(SCAN_AD_ZERO_PID);
            note_obj(pid, vtRva, teamId, px, py, "pid0");
            continue;
        }
        if (teamId == own_team_b)
        {
            autododge_scanner_event(SCAN_AD_OWN_TEAM);
            note_obj(pid, vtRva, teamId, px, py, "own");
            continue;
        }

        uint32_t deadFlag = 0;
        memcpy(&deadFlag, obj + OBJ_DEADFLAG_OFF, sizeof(deadFlag));
        if (deadFlag != 0)
        {
            autododge_scanner_event(SCAN_AD_DEAD);
            note_obj(pid, vtRva, teamId, px, py, "dead");
            continue;
        }

        auto it = proj_cache_index.find(pid);
        if (it == proj_cache_index.end())
        {
            ProjectileState state{};
            state.lastX = px_i;
            state.lastY = py_i;
            state.lastTs = now;
            int32_t ind = 0, projSpeed = 0, projRadius = 0, projEarly = 0;
            if (!read_int(objPtr + PROJECTILE_ISINDIRECT_OFF, &ind))
            {
                autododge_scanner_event(SCAN_AD_INDIRECT_READ_FAIL);
                note_obj(pid, vtRva, teamId, px, py, "indread");
                continue;
            }

            read_int(dataPtr + PROJECTILEDATA_SPEED_OFF, &projSpeed);
            if (projSpeed < 200 || projSpeed > 20000)
            {
                probe_speed_field(dataPtr, pid, projSpeed);
            }
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

            cat_sample(vtRva, state.name, dataPtr);
            proj_cache_index[pid] = proj_cache_storage.size();
            proj_cache_storage.push_back(state);
            it = proj_cache_index.find(pid);
        }
        else
        {
            if (it->second >= proj_cache_storage.size() ||
                proj_cache_storage[it->second].pid != pid)
            {
                autododge_scanner_event(SCAN_AD_CACHE_INDEX_REPAIRED);
                rebuild_projectile_cache_index();
                it = proj_cache_index.find(pid);
            }
            if (it == proj_cache_index.end()) continue;
            ProjectileState& state = proj_cache_storage[it->second];
            float dt = (float)(now - state.lastTs) / 1000.0f;
            float ddx = (float)px_i - (float)state.lastX;
            float ddy = (float)py_i - (float)state.lastY;
            float step = sqrtf(ddx * ddx + ddy * ddy);
            if (dt > DT_MIN && dt < DT_MAX && step * step >= POS_DELTA_MIN_SQ)
            {
                float inst = step / dt;
                if (inst >= 100.0f && inst <= 9000.0f)
                {
                    state.meas = state.meas > 0.0f
                                     ? state.meas * MEAS_KEEP + inst * MEAS_NEW
                                     : inst;
                }
                float ux = ddx / step;
                float uy = ddy / step;
                bool flipped =
                    state.hasVelocity &&
                    ux * state.dir_x + uy * state.dir_y < 0.0f;
                if (state.hasVelocity && !flipped)
                {
                    ux = state.dir_x * DIR_KEEP + ux * DIR_NEW;
                    uy = state.dir_y * DIR_KEEP + uy * DIR_NEW;
                    float norm = sqrtf(ux * ux + uy * uy);
                    if (norm > VV_EPS)
                    {
                        ux /= norm;
                        uy /= norm;
                    }
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
        if (state.hasVelocity && state.seen >= 2 && !state.probed &&
            probe_count < PROBE_MAX_SAMPLES)
        {
            state.probed = true;
            probe_sample(objPtr, state.last_ux, state.last_uy, mag_of(state));
        }

        if (!state.hasVelocity && state.speed > 0.0f)
        {
            float dx = px - state.spawnX;
            float dy = py - state.spawnY;
            float len = sqrtf(dx * dx + dy * dy);
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
        if (dx * dx + dy * dy > zoneSq)
        {
            autododge_scanner_event(SCAN_AD_OUT_OF_RANGE);
            note_obj(pid, vtRva, teamId, px, py, "far");
            continue;
        }

        float mag = state.speed;
        if (mag < DEF_SPEED_MIN || mag > DEF_SPEED_MAX) mag = state.meas;
        bool blob = !state.isBeam && !state.isIndirect && mag > 0.0f &&
                    mag < BLOB_SPEED_MAX;
        bool tracked = state.hasVelocity && mag > SPEED_MIN;
        if (!blob && !tracked)
        {
            autododge_scanner_event(SCAN_AD_NO_VELOCITY);
            note_novelo(
                pid, vtRva, state.rawSpeed, state.radius, px, py, mx, my);
        }
        if (blob) autododge_scanner_event(SCAN_AD_BLOB);
        float vx = (blob || !tracked) ? 0.0f : state.dir_x * mag;
        float vy = (blob || !tracked) ? 0.0f : state.dir_y * mag;
        float speedNow = sqrtf(vx * vx + vy * vy);
        float ux = speedNow > SPEED_MIN ? vx / speedNow : 0.0f;
        float uy = speedNow > SPEED_MIN ? vy / speedNow : 0.0f;

        if (!blob && speedNow >= SPEED_MIN)
        {
            float playerAlong = (mx - px) * ux + (my - py) * uy;
            if (playerAlong < PLAYER_ALONG_MIN)
            {
                autododge_scanner_event(SCAN_AD_BEHIND);
                note_obj(pid, vtRva, teamId, px, py, "behind");
                continue;
            }
            if (!state.isIndirect &&
                !wall_los(px,
                          py,
                          px + ux * (playerAlong > 0.0f ? playerAlong : 0.0f),
                          py + uy * (playerAlong > 0.0f ? playerAlong : 0.0f),
                          WALL_BLOCKS_PROJECTILES))
            {
                autododge_scanner_event(SCAN_AD_WALL_BLOCKED);
                note_obj(pid, vtRva, teamId, px, py, "wall");
                continue;
            }
        }
        autododge_scanner_event(SCAN_AD_THREAT_CANDIDATE);
        Threat item{};
        item.x = px;
        item.y = py;
        item.vx = vx;
        item.vy = vy;
        item.rad = state.radius + myRadius;
        item.mag = mag;
        item.pid = pid;
        note_obj(pid, vtRva, teamId, px, py, "proj");
        memcpy(item.name, state.name, sizeof(item.name) - 1);
        item.name[sizeof(item.name) - 1] = '\0';
        threats.push_back(item);
    }

    {
        size_t writeIdx = 0;
        for (size_t i = 0; i < proj_cache_storage.size(); i++)
        {
            if (now - proj_cache_storage[i].lastSeen > 1024)
            {
                autododge_scanner_event(SCAN_AD_CACHE_REBUILT);
                continue;
            }
            if (writeIdx != i)
                proj_cache_storage[writeIdx] = proj_cache_storage[i];
            writeIdx++;
        }
        proj_cache_storage.resize(writeIdx);
        if (writeIdx != proj_cache_index.size())
            rebuild_projectile_cache_index();
    }
    if (threats.size() >= 32) autododge_scanner_event(SCAN_AD_MAX_THREATS);
    for (const auto& t : threats)
    {
        float ttc = timeToCollision(mx, my, 0.0f, 0.0f, t);
        float relx = t.x - mx;
        float rely = t.y - my;
        float dist = sqrtf(relx * relx + rely * rely);
        float vv = t.vx * t.vx + t.vy * t.vy;
        float along = 0.0f;
        float ca = dist;
        float score = 0.0f;
        if (vv > VV_EPS)
        {
            along = (relx * t.vx + rely * t.vy) / sqrtf(vv);
            float perp = dist * dist - along * along;
            ca = perp > 0.0f ? sqrtf(perp) : 0.0f;
        }
        score = timeToClosestApproach(t, mx, my) >= 0.0f ? ca : ca + 1000000.0f;
        int slot = ad_threat_n;
        if (slot < AD_REPORT_MAX)
        {
            ad_threat_n++;
        }
        else
        {
            int worst = 0;
            for (int i = 1; i < AD_REPORT_MAX; i++)
            {
                if (ad_threat_score[i] > ad_threat_score[worst])
                {
                    worst = i;
                }
            }
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
        ad_threat_score[slot] = score;
        memcpy(ad_threat_name[slot], t.name, sizeof(ad_threat_name[slot]));
        ad_threat_name[slot][sizeof(ad_threat_name[slot]) - 1] = '\0';
    }
    probe_analyze();
    if (pendingHit)
    {
        snapshot_hit(mx, my);
        pendingHit = 0;
    }
    autododge_scanner_report();

    if (threats.empty() && bodies.empty())
    {
        if (now - lastDangerTs > TUNE.RELEASE_GRACE_MS) endDodge();
        return;
    }

    float stayClearSq = clearanceFor(threats, bodies, mx, my, 0.0f, 0.0f);
    bool inDanger = stayClearSq < (TUNE.ENGAGE * TUNE.ENGAGE);
    if (!inDanger)
    {
        for (const auto& t : threats)
        {
            float ttc = timeToCollision(mx, my, 0.0f, 0.0f, t);
            if (ttc > 0.0f && ttc < TUNE.REACT_S)
            {
                inDanger = true;
                break;
            }
        }
    }
    if (inDanger) lastDangerTs = now;

    if (!inDanger &&
        (headingIdx < 0 || now - lastDangerTs > TUNE.RELEASE_GRACE_MS))
    {
        endDodge();
        return;
    }

    float ex = 0.0f, ey = 0.0f;
    for (const auto& t : threats)
    {
        if (timeToClosestApproach(t, mx, my) < 0.0f) continue;
        float dx = mx - t.x;
        float dy = my - t.y;
        float distSq = dx * dx + dy * dy;
        if (distSq > 1.0f)
        {
            float dist = sqrtf(distSq);
            float w = SPEED_MIN / (dist + ESCAPE_DIST_BIAS);
            ex += (dx / dist) * w;
            ey += (dy / dist) * w;
        }
    }
    float elen = sqrtf(ex * ex + ey * ey);
    if (elen > 0.001f)
    {
        ex /= elen;
        ey /= elen;
    }

    float gx = 0.0f, gy = 0.0f;
    if (wall_cache_w() > 0 && wall_cache_h() > 0)
    {
        gx = (wall_cache_w() * WALL_TILE_SIZE * HALF) - mx;
        gy = (wall_cache_h() * WALL_TILE_SIZE * HALF) - my;
        float glen = sqrtf(gx * gx + gy * gy);
        if (glen > 0.001f)
        {
            gx /= glen;
            gy /= glen;
        }
    }

    bool hasPrev = commitDirX != 0.0f || commitDirY != 0.0f;
    float prevDirX = commitDirX, prevDirY = commitDirY;

    int bestIdx = 0;
    float bestDanger = INF_F;
    float bestInterest = -INF_F;

    for (int i = 0; i < CARDINALS; i++)
    {
        float d =
            dangerFor(mx, my, cardDirX[i], cardDirY[i], CHAR_SPEED, bodyR);

        float dotEscape = cardDirX[i] * ex + cardDirY[i] * ey;
        float escapeScore = (dotEscape + SPEED_MIN) * HALF;
        float dotGoal = cardDirX[i] * gx + cardDirY[i] * gy;
        float goalScore = (dotGoal + SPEED_MIN) * HALF;
        float inter =
            escapeScore * INTEREST_ESCAPE_W + goalScore * INTEREST_GOAL_W;

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

    float outDirX = cardDirX[bestIdx];
    float outDirY = cardDirY[bestIdx];

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
    {
        dirLockUntil = now + DIR_HOLD_MS;
    }
    commitDirX = outDirX;
    commitDirY = outDirY;
    headingIdx = bestIdx;
    ad_best_dir = bestIdx;
    lastGoodTs = now;

    int32_t tx = clamp_axis(mx + outDirX * TUNE.REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + outDirY * TUNE.REACH, wall_cache_h());
    int32_t ddx = tx - ad_target_x, ddy = ty - ad_target_y;

    if ((float)(ddx * ddx + ddy * ddy) > POS_CHANGE_MIN_SQ ||
        now - lastSentAt > TUNE.SEND_MIN_MS)
    {
        lastSentAt = now;
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    active = 1;
    moving = 1;
}
