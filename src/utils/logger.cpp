#include "../core/scan.h"
#include "../utils/walls.h"
#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <csignal>
#include <cmath>
#include <sys/_types/_ucontext64.h>
#include <unistd.h>

extern int32_t active;
extern int32_t moving;
extern int32_t ad_target_x;
extern int32_t ad_target_y;
extern uintptr_t addr_projbeam;
extern uintptr_t addr_projearly;
extern uintptr_t addr_dataname;
extern uintptr_t callable(uintptr_t rva);

namespace
{
constexpr long long LOG_MAX_BYTES = 8000000;
constexpr int LOG_INTERVAL_MS = 200;

constexpr int DIR_COUNT = 32;
constexpr float AWARE = 4400.0f;
constexpr float SKIN = 90.0f;
constexpr float HORIZON_S = 1.55f;
constexpr float WALL_HIT = 9000.0f;
constexpr int PROBE_N = 6;
constexpr float PROBE_T = 0.12f;
constexpr float DANGER_RADIUS = 180.0f;
constexpr float TTC_MIN = 0.09f;
constexpr int MAX_THREATS = 32;

struct Thr { float x, y, vx, vy, rad; };
struct Ent { float x, y, rad; };

static FILE *file = nullptr;
static dispatch_source_t timer = nullptr;
static std::atomic<unsigned long long> ticks{0};
static std::atomic<uint64_t> lastMs{0};
static char path[1024];

static float ringX[DIR_COUNT];
static float ringY[DIR_COUNT];
static bool ringReady = false;

static Thr thr[MAX_THREATS];
static int thrCount = 0;
static Ent ent[16];
static int entCount = 0;

void buildRing()
{
    if (ringReady) return;
    for (int i = 0; i < DIR_COUNT; i++)
    {
        float a = (float)M_PI * 2.0f * (float)i / (float)DIR_COUNT;
        ringX[i] = cosf(a);
        ringY[i] = sinf(a);
    }
    ringReady = true;
}

void document_path(char *out, size_t cap)
{
    const char *home = getenv("HOME");
    out[0] = 0;
    if (home != nullptr) snprintf(out, cap, "%s/Documents", home);
}

void rotate_if_needed()
{
    FILE *probe = fopen(path, "rb");
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

void line(const char *format, ...)
{
    if (file == nullptr) return;
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    fflush(file);
    va_end(args);
}

void write_crash_marks(const char *format, ...)
{
    FILE *f = fopen(path, "a");
    if (f == nullptr) return;
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fflush(f);
    fclose(f);
}

void crash_signal(int sig, siginfo_t *info, void *ctx)
{
    char buf[256];
    uintptr_t pc = 0, lr = 0, fault = 0;
    Dl_info di;
    if (ctx != nullptr)
    {
        ucontext64_t *uc = static_cast<ucontext64_t *>(ctx);
        pc = static_cast<uintptr_t>(uc->uc_mcontext64->__ss.__pc);
        lr = static_cast<uintptr_t>(uc->uc_mcontext64->__ss.__lr);
    }
    if (info != nullptr) fault = (uintptr_t)info->si_addr;
    int len = snprintf(buf, sizeof(buf),
                       "[crash-signal] sig=%d fault=%p pc=%p lr=%p\n",
                       sig, (void *)fault, (void *)pc, (void *)lr);
    if (len > 0) write(STDERR_FILENO, buf, static_cast<size_t>(len));
    if (path[0] != 0)
    {
        int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (fd >= 0)
        {
            if (len > 0) write(fd, buf, static_cast<size_t>(len));
            if (dladdr((void *)pc, &di) && di.dli_fname)
            {
                len = snprintf(buf, sizeof(buf),
                               "[crash-signal] pc in %s off=%lx sym=%s\n",
                               di.dli_fname,
                               static_cast<unsigned long>(pc - (uintptr_t)di.dli_fbase),
                               di.dli_sname ? di.dli_sname : "?");
                if (len > 0) write(fd, buf, static_cast<size_t>(len));
            }
            close(fd);
        }
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void install_crash_handler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_signal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGTRAP, &sa, nullptr);
}

float ttcFor(float mx, float my, float mvx, float mvy, const Thr &t)
{
    float relx = t.x - mx, rely = t.y - my;
    float vrx = t.vx - mvx, vry = t.vy - mvy;
    float vv = vrx * vrx + vry * vry;
    if (vv < 1e-6f) return 1e9f;
    float b = 2.0f * (relx * vrx + rely * vry);
    float c = relx * relx + rely * rely - t.rad * t.rad;
    float disc = b * b - 4.0f * vv * c;
    if (disc < 0.0f) return 1e9f;
    float sd = sqrtf(disc);
    float t1 = (-b - sd) / (2.0f * vv);
    float t2 = (-b + sd) / (2.0f * vv);
    if (t1 > 0.0f) return t1;
    if (t2 > 0.0f) return t2;
    return 1e9f;
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
        float ttc = ttcFor(mx, my, mvx, mvy, thr[i]);
        if (ttc < TTC_MIN) return 1.0f;
        float relx = thr[i].x - mx, rely = thr[i].y - my;
        float vrx = thr[i].vx - mvx, vry = thr[i].vy - mvy;
        float vv = vrx * vrx + vry * vry;
        float ts = vv > 1e-6f ? -(relx * vrx + rely * vry) / vv : 0.0f;
        if (ts < 0.0f) ts = 0.0f; else if (ts > HORIZON_S) ts = HORIZON_S;
        float cx = relx + vrx * ts, cy = rely + vry * ts;
        float m = sqrtf(cx * cx + cy * cy) - thr[i].rad;
        float c = (DANGER_RADIUS - m) / DANGER_RADIUS;
        if (c < 0.0f) c = 0.0f;
        if (c > max_c) max_c = c;
        sum_c += c;
    }
    if (wallPenalty(mx, my, dx, dy, speed, bodyR) > 0.0f) return 0.9f;
    float d = max_c + 0.15f * (sum_c - max_c);
    return d > 1.0f ? 1.0f : d;
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
        uintptr_t data = 0;
        if (!read_ptr(obj + static_cast<uintptr_t>(ELEM_DEF_OFF), &data) || !data) continue;
        uintptr_t vt = 0;
        if (!read_ptr(data, &vt) || vt != projVt) continue;
        int32_t team = -1, px = 0, py = 0;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_TEAM_OFF), &team) || team == own_team_b) continue;
        uint32_t df = 0;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_DEADFLAG_OFF), reinterpret_cast<int32_t *>(&df)) || df != 0) continue;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_X_OFF), &px)) continue;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_Y_OFF), &py)) continue;
        float dx = px - mx, dy = py - my;
        if (dx * dx + dy * dy > zoneSq)
        {
            ent[entCount].x = (float)px;
            ent[entCount].y = (float)py;
            ent[entCount].rad = myRadius;
            if (entCount < 15) entCount++;
            continue;
        }
        int32_t speed = 0, radius = 0;
        read_int(data + static_cast<uintptr_t>(PROJECTILEDATA_SPEED_OFF), &speed);
        read_int(data + static_cast<uintptr_t>(PROJECTILEDATA_RADIUS_OFF), &radius);
        float vx = 0, vy = 0;
        thr[thrCount].x = (float)px;
        thr[thrCount].y = (float)py;
        thr[thrCount].vx = vx;
        thr[thrCount].vy = vy;
        thr[thrCount].rad = (float)radius + myRadius;
        thrCount++;
    }
}

void dump_ad()
{
    float mx = (float)own_x;
    float my = (float)own_y;
    float myRadius = own_r < 1.0f ? 120.0f : own_r;
    float speed = own_speed < 1.0f ? 720.0f : own_speed;
    float bodyR = myRadius + SKIN;
    float mvx = 0, mvy = 0;
    buildRing();
    collectThreats(mx, my, myRadius);
    float minTtc = 1e9f;
    int nearIdx = -1;
    for (int i = 0; i < thrCount; i++)
    {
        float t = ttcFor(mx, my, 0, 0, thr[i]);
        if (t < minTtc) { minTtc = t; nearIdx = i; }
    }
    line("[ad] pos=(%.0f,%.0f) spd=%.0f r=%.0f thr=%d ent=%d\n",
         mx, my, speed, myRadius, thrCount, entCount);
    line("[ad] near_t=%.3f near_idx=%d\n", minTtc, nearIdx);
    for (int i = 0; i < thrCount && i < 8; i++)
        line("  t%d: (%.0f,%.0f) v=(%.0f,%.0f) r=%.0f\n",
             i, thr[i].x, thr[i].y, thr[i].vx, thr[i].vy, thr[i].rad);
    int bestIdx = 0;
    float bestScore = -1e18f;
    float minDanger = 1e18f;
    float dangerArr[DIR_COUNT];
    float interestArr[DIR_COUNT];
    for (int i = 0; i < DIR_COUNT; i++)
        dangerArr[i] = dangerFor(mx, my, ringX[i], ringY[i], speed, bodyR);
    for (int i = 0; i < DIR_COUNT; i++)
        if (dangerArr[i] < minDanger) minDanger = dangerArr[i];
    float gx = 0, gy = 0;
    if (wall_cache_w() > 0 && wall_cache_h() > 0)
    {
        gx = (wall_cache_w() * WALL_TILE_SIZE * 0.5f) - mx;
        gy = (wall_cache_h() * WALL_TILE_SIZE * 0.5f) - my;
        float gl = sqrtf(gx * gx + gy * gy);
        if (gl > 0.001f) { gx /= gl; gy /= gl; }
    }
    float ex = 0, ey = 0;
    for (int i = 0; i < thrCount; i++)
    {
        float dx = mx - thr[i].x, dy = my - thr[i].y;
        float d2 = dx * dx + dy * dy;
        if (d2 > 1.0f)
        {
            float id = 1.0f / sqrtf(d2);
            float w = 1.0f / (sqrtf(d2) + 100.0f);
            ex += dx * id * w;
            ey += dy * id * w;
        }
    }
    float el = sqrtf(ex * ex + ey * ey);
    if (el > 0.001f) { ex /= el; ey /= el; }
    for (int i = 0; i < DIR_COUNT; i++)
    {
        float de = ringX[i] * ex + ringY[i] * ey;
        float dg = ringX[i] * gx + ringY[i] * gy;
        interestArr[i] = (de + 1.0f) * 0.35f + (dg + 1.0f) * 0.15f;
        if (dangerArr[i] > minDanger + 0.01f) continue;
        if (interestArr[i] > bestScore) { bestScore = interestArr[i]; bestIdx = i; }
    }
    float botAngle = atan2f((float)(ad_target_y - own_y), (float)(ad_target_x - own_x)) * 180.0f / (float)M_PI;
    float bestAngle = (float)bestIdx * 360.0f / (float)DIR_COUNT;
    line("[ad] bestIdx=%d bestAngle=%.1f botAngle=%.1f active=%d moving=%d tgt=(%d,%d)\n",
         bestIdx, bestAngle, botAngle, active, moving, ad_target_x, ad_target_y);
    for (int i = 0; i < DIR_COUNT; i += 2)
        line("  d%d: danger=%.2f interest=%.2f\n", i, dangerArr[i], interestArr[i]);
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
        line("idle t=%llu base=%p own=%p dead=%d\n",
             ticks.load(std::memory_order_relaxed),
             (void *)base, (void *)own_elem, dead);
        lastMs.store(0);
        fflush(file);
        rotate_if_needed();
        return;
    }
    uint64_t now = static_cast<uint64_t>(g_get_real_time() / 1000);
    if (lastMs.load() == 0) line("\n##### match #####\n");
    line("\n--- t=%llu ms=%.3f ---\n",
         ticks.load(std::memory_order_relaxed),
         static_cast<double>(g_get_real_time()) / 1000.0);
    write_crash_marks("[crash-mark] t=%llu stage=ad\n", ticks.load(std::memory_order_relaxed));
    dump_ad();
    write_crash_marks("[crash-mark] t=%llu stage=done\n", ticks.load(std::memory_order_relaxed));
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
    write_crash_marks("[crash-mark] process start\n");
    install_crash_handler();
    dispatch_async(dispatch_get_main_queue(), ^{ start(); });
}