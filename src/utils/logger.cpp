#include "../core/scan.h"
#include "../utils/walls.h"
#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <cmath>
#include <limits>
#include <unistd.h>

extern int active;
extern int moving;
extern int32_t ad_target_x;
extern int32_t ad_target_y;

namespace
{
constexpr long long LOG_MAX_BYTES = 8000000;
constexpr int LOG_INTERVAL_MS = 200;
constexpr float AWARE = 4400.0f;
constexpr float SKIN = 90.0f;
constexpr float HORIZON_S = 1.55f;
constexpr float WALL_HIT = 9000.0f;
constexpr int PROBE_N = 6;
constexpr float PROBE_T = 0.12f;
constexpr float DANGER_RADIUS = 300.0f;
constexpr float TTC_MIN = 0.12f;
constexpr float INF_F = std::numeric_limits<float>::infinity();
constexpr float VV_EPS = 1e-6f;
constexpr float LEN_EPS = 0.001f;
constexpr float HITBOX_FALLBACK = 120.0f;
constexpr float SPEED_FALLBACK = 720.0f;
constexpr float SPEED_MIN = 1.0f;
constexpr float HALF = 0.5f;
constexpr float QUADRATIC_K = 2.0f;
constexpr float INTEREST_ESCAPE_W = 0.7f;
constexpr float INTEREST_GOAL_W = 0.3f;
constexpr float ESCAPE_DIST_BIAS = 100.0f;
constexpr float DANGER_SUM_WEIGHT = 0.15f;
constexpr float DANGER_MAX = 1.0f;
constexpr int MAX_THREATS = 32;
constexpr int MAX_ENTITIES = 16;

struct Thr { float x, y, vx, vy, rad; };
struct Ent { float x, y, rad; };

static FILE* file = nullptr;
static dispatch_source_t timer = nullptr;
static std::atomic<unsigned long long> ticks{0};
static std::atomic<uint64_t> lastMs{0};
static char path[1024];

static Thr thr[MAX_THREATS];
static int thrCount = 0;
static Ent ent[MAX_ENTITIES];
static int entCount = 0;

static int dodge_count = 0;
static int hit_count = 0;
static int32_t last_hp = -1;
static int32_t last_tx = INT32_MAX;
static int32_t last_ty = INT32_MAX;

void document_path(char* out, size_t cap)
{
    const char* home = getenv("HOME");
    out[0] = 0;
    if (home != nullptr) snprintf(out, cap, "%s/Documents", home);
}

void rotate_if_needed()
{
    FILE* probe = fopen(path, "rb");
    if (probe == nullptr) return;
    fseek(probe, 0, SEEK_END);
    long long size = ftell(probe);
    fclose(probe);
    if (size <= LOG_MAX_BYTES) return;
    char backup[1100];
    snprintf(backup, sizeof(backup), "%s.1", path);
    rename(path, backup);
}

bool open_log()
{
    char dir[1024];
    document_path(dir, sizeof(dir));
    if (dir[0] == 0) return false;
    snprintf(path, sizeof(path), "%s/Recoil.log", dir);
    file = fopen(path, "a");
    if (file == nullptr) return false;
    fseek(file, 0, SEEK_END);
    if (ftell(file) == 0)
    {
        fprintf(file, "=== autododge log ===\n");
        fprintf(file, "documents: %s\n", dir);
    }
    return true;
}

void line(const char* format, ...)
{
    if (file == nullptr) return;
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    fflush(file);
    va_end(args);
}

float ttcFor(float mx, float my, float mvx, float mvy, const Thr& t)
{
    float relx = t.x - mx, rely = t.y - my;
    float vrx = t.vx - mvx, vry = t.vy - mvy;
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

float tcaFor(const Thr& t, float mx, float my)
{
    float relx = t.x - mx, rely = t.y - my;
    float vv = t.vx * t.vx + t.vy * t.vy;
    if (vv < VV_EPS) return -1.0f;
    return -(relx * t.vx + rely * t.vy) / vv;
}

float wallPenalty(float mx, float my, float dx, float dy, float speed, float bodyR)
{
    if (!wall_ready()) return 0.0f;
    float step = speed * PROBE_T;
    int blocked = 0;
    for (int s = 1; s <= PROBE_N; s++)
        if (wall_is_blocked_wide(mx + dx * step * s, my + dy * step * s, bodyR, WALL_BLOCKS_MOVEMENT)) blocked++;
    return WALL_HIT * (float)blocked / (float)PROBE_N;
}

float dangerFor(float mx, float my, float dx, float dy, float speed, float bodyR)
{
    float mvx = speed * dx, mvy = speed * dy;
    float max_c = 0.0f, sum_c = 0.0f;
    for (int i = 0; i < thrCount; i++)
    {
        if (tcaFor(thr[i], mx, my) < 0.0f) continue;
        float ttc = ttcFor(mx, my, mvx, mvy, thr[i]);
        if (ttc < TTC_MIN) return DANGER_MAX;
        float relx = thr[i].x - mx, rely = thr[i].y - my;
        float vrx = thr[i].vx - mvx, vry = thr[i].vy - mvy;
        float vv = vrx * vrx + vry * vry;
        float ts = vv > VV_EPS ? -(relx * vrx + rely * vry) / vv : 0.0f;
        if (ts < 0.0f) ts = 0.0f; else if (ts > HORIZON_S) ts = HORIZON_S;
        float cx = relx + vrx * ts, cy = rely + vry * ts;
        float m = sqrtf(cx * cx + cy * cy) - thr[i].rad;
        float c = (DANGER_RADIUS - m) / DANGER_RADIUS;
        if (c < 0.0f) c = 0.0f;
        if (c > max_c) max_c = c;
        sum_c += c;
    }
    if (wallPenalty(mx, my, dx, dy, speed, bodyR) > 0.0f) return 0.9f;
    float d = max_c + DANGER_SUM_WEIGHT * (sum_c - max_c);
    return d > DANGER_MAX ? DANGER_MAX : d;
}

void collectThreats(float mx, float my, float myRadius)
{
    thrCount = 0;
    entCount = 0;
    float zoneSq = AWARE * AWARE;
    uintptr_t projVt = base + static_cast<uintptr_t>(VTABLE_PROJ_DATA_RVA);

    for (int i = 0; i < obj_count && thrCount < MAX_THREATS; i++)
    {
        if (!obj_valid[i]) continue;
        uintptr_t obj = obj_snapshot[i];
        if (!obj) continue;

        void* dataRaw = nullptr;
        if (!read_ptr(obj + static_cast<uintptr_t>(ELEM_DEF_OFF), &dataRaw) || !dataRaw) continue;
        uintptr_t data = (uintptr_t)dataRaw;

        void* vtRaw = nullptr;
        if (!read_ptr(data, &vtRaw) || (uintptr_t)vtRaw != projVt) continue;

        int32_t team = -1, px = 0, py = 0;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_TEAM_OFF), &team) || team == own_team_b) continue;

        int32_t deadFlag = 0;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_DEADFLAG_OFF), &deadFlag) || deadFlag != 0) continue;

        if (!read_int(obj + static_cast<uintptr_t>(OBJ_X_OFF), &px)) continue;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_Y_OFF), &py)) continue;

        float dx = px - mx, dy = py - my;
        if (dx * dx + dy * dy > zoneSq)
        {
            if (entCount < MAX_ENTITIES)
            {
                ent[entCount].x = (float)px;
                ent[entCount].y = (float)py;
                ent[entCount].rad = myRadius;
                entCount++;
            }
            continue;
        }

        int32_t speed = 0, radius = 0;
        read_int(data + static_cast<uintptr_t>(PROJECTILEDATA_SPEED_OFF), &speed);
        read_int(data + static_cast<uintptr_t>(PROJECTILEDATA_RADIUS_OFF), &radius);

        thr[thrCount].x = (float)px;
        thr[thrCount].y = (float)py;
        thr[thrCount].vx = 0.0f;
        thr[thrCount].vy = 0.0f;
        thr[thrCount].rad = (float)radius + myRadius;
        thrCount++;
    }
}

void trackStats()
{
    if (own_hp > 0)
    {
        int32_t cur_hp = (int32_t)own_hp;
        if (last_hp >= 0 && cur_hp < last_hp) hit_count++;
        last_hp = cur_hp;
    }
    if (active != 0 && (ad_target_x != last_tx || ad_target_y != last_ty))
    {
        if (ad_target_x != INT32_MAX && ad_target_y != INT32_MAX) dodge_count++;
        last_tx = ad_target_x;
        last_ty = ad_target_y;
    }
}

void dump_ad()
{
    trackStats();

    float mx = (float)own_x;
    float my = (float)own_y;
    float myRadius = own_r < SPEED_MIN ? HITBOX_FALLBACK : own_r;
    float speed = own_speed < SPEED_MIN ? SPEED_FALLBACK : own_speed;
    float bodyR = myRadius + SKIN;

    collectThreats(mx, my, myRadius);

    float minTtc = INF_F;
    for (int i = 0; i < thrCount; i++)
    {
        if (tcaFor(thr[i], mx, my) < 0.0f) continue;
        float t = ttcFor(mx, my, 0.0f, 0.0f, thr[i]);
        if (t < minTtc) minTtc = t;
    }

    line("[ad] dodges=%d hits=%d hp=%d active=%d moving=%d\n",
         dodge_count, hit_count, (int)own_hp, active, moving);
    line("[ad] pos=(%.0f,%.0f) spd=%.0f r=%.0f tgt=(%d,%d)\n",
         mx, my, speed, myRadius, ad_target_x, ad_target_y);
    line("[ad] thr=%d ent=%d near_ttc=%.3f\n",
         thrCount, entCount, minTtc);

    for (int i = 0; i < thrCount && i < 8; i++)
        line("  t%d: (%.0f,%.0f) v=(%.0f,%.0f) r=%.0f tca=%.3f\n",
             i, thr[i].x, thr[i].y, thr[i].vx, thr[i].vy, thr[i].rad, tcaFor(thr[i], mx, my));

    float ex = 0.0f, ey = 0.0f;
    for (int i = 0; i < thrCount; i++)
    {
        if (tcaFor(thr[i], mx, my) < 0.0f) continue;
        float dx = mx - thr[i].x, dy = my - thr[i].y;
        float d2 = dx * dx + dy * dy;
        if (d2 > SPEED_MIN)
        {
            float d = sqrtf(d2);
            float w = SPEED_MIN / (d + ESCAPE_DIST_BIAS);
            ex += (dx / d) * w;
            ey += (dy / d) * w;
        }
    }
    float el = sqrtf(ex * ex + ey * ey);
    if (el > LEN_EPS) { ex /= el; ey /= el; }

    float gx = 0.0f, gy = 0.0f;
    if (wall_cache_w() > 0 && wall_cache_h() > 0)
    {
        gx = (wall_cache_w() * WALL_TILE_SIZE * HALF) - mx;
        gy = (wall_cache_h() * WALL_TILE_SIZE * HALF) - my;
        float gl = sqrtf(gx * gx + gy * gy);
        if (gl > LEN_EPS) { gx /= gl; gy /= gl; }
    }

    int bestIdx = 0;
    float bestDanger = INF_F;
    float bestInterest = -INF_F;
    static const float cardX[4] = {1.0f, 0.0f, -1.0f, 0.0f};
    static const float cardY[4] = {0.0f, 1.0f, 0.0f, -1.0f};

    for (int i = 0; i < 4; i++)
    {
        float d = dangerFor(mx, my, cardX[i], cardY[i], speed, bodyR);
        float dotEscape = cardX[i] * ex + cardY[i] * ey;
        float escapeScore = (dotEscape + SPEED_MIN) * HALF;
        float dotGoal = cardX[i] * gx + cardY[i] * gy;
        float goalScore = (dotGoal + SPEED_MIN) * HALF;
        float inter = escapeScore * INTEREST_ESCAPE_W + goalScore * INTEREST_GOAL_W;
        bool better = false;
        if (d < bestDanger - 0.02f) better = true;
        else if (fabsf(d - bestDanger) <= 0.02f && inter > bestInterest) better = true;
        if (better) { bestDanger = d; bestInterest = inter; bestIdx = i; }
    }

    float botAngle = atan2f((float)(ad_target_y - own_y), (float)(ad_target_x - own_x)) * (180.0f / (float)M_PI);
    line("[ad] best=%d (%.1f) bot=%.1f\n", bestIdx, (float)bestIdx * 90.0f, botAngle);
}

void tick()
{
    if (file == nullptr)
    {
        if (!open_log()) return;
        line("\n##### session #####\n");
    }
    ticks.fetch_add(1, std::memory_order_relaxed);
    if (!base || !own_elem || dead)
    {
        line("idle t=%llu\n", ticks.load(std::memory_order_relaxed));
        lastMs.store(0);
        fflush(file);
        rotate_if_needed();
        return;
    }
    uint64_t now = static_cast<uint64_t>(g_get_real_time() / 1000);
    if (lastMs.load() == 0)
    {
        line("\n##### match #####\n");
        last_hp = -1;
        last_tx = INT32_MAX;
        last_ty = INT32_MAX;
        dodge_count = 0;
        hit_count = 0;
    }
    line("\n--- t=%llu ---\n", ticks.load(std::memory_order_relaxed));
    dump_ad();
    lastMs.store(now);
    fflush(file);
    rotate_if_needed();
}

void start()
{
    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    if (!timer) return;
    dispatch_source_set_timer(
        timer,
        dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(150 * NSEC_PER_MSEC)),
        static_cast<uint64_t>(LOG_INTERVAL_MS * NSEC_PER_MSEC),
        static_cast<uint64_t>(10 * NSEC_PER_MSEC));
    dispatch_source_set_event_handler(timer, ^{ tick(); });
    dispatch_resume(timer);
}
}

__attribute__((constructor)) static void logger_entry()
{
    dispatch_async(dispatch_get_main_queue(), ^{ start(); });
}