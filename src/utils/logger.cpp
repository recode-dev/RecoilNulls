#include "../core/scan.h"
#include "../core/scstring.h"
#include "../core/commands.h"
#include "../utils/walls.h"
#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <os/lock.h>
#include <cmath>
#include <limits>
#include <unistd.h>
extern uintptr_t addr_charspeed;
extern uintptr_t addr_charradius;
extern uintptr_t addr_getownidx;
extern uintptr_t addr_getgid;

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
constexpr uintptr_t RAW_6C_OFF = 0x6C;

constexpr float SPEED_SCALE = 1000.0f;
constexpr float RADIUS_SCALE = 10.0f;
constexpr float DT_MIN = 0.002f;
constexpr float DT_MAX = 0.6f;
constexpr float POS_DELTA_MIN_SQ = 16.0f;
constexpr float VEL_ALPHA_NEW = 1.0f;
constexpr float VEL_ALPHA_EXIST = 0.4f;
constexpr int MAX_TRACK = 64;
constexpr uint64_t TRACK_TTL_MS = 1000;

struct Track
{
    uintptr_t data;
    int32_t lastX, lastY;
    uint64_t lastMs;
    float vx, vy;
    bool hasVelocity;
};

static Track tracks[MAX_TRACK];
static int trackCount = 0;

struct Thr
{
    float x, y, vx, vy, rad;
    int32_t raw6c_i;
    int32_t rawSpeed;
    float raw6c_f;
};
struct Ent
{
    float x, y, rad;
};

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

constexpr size_t STRUCT_SCAN_MAX_BYTES = 0x2D0;
constexpr size_t STRUCT_SCAN_MAX_RECORDS = 2048;
constexpr size_t STRUCT_SCAN_MAX_PROJECTILES = 64;
constexpr size_t STRUCT_SCAN_MAX_OBJECT_SAMPLES = 1;
constexpr size_t STRUCT_SCAN_MAX_DATA_TYPES = 64;
constexpr size_t STRUCT_SCAN_MAX_TILE_TYPES = 32;
struct StructScanRecord
{
    uintptr_t address;
    char label[48];
};
static StructScanRecord structScanRecords[STRUCT_SCAN_MAX_RECORDS]{};
static size_t structScanRecordCount = 0;
static size_t structScanTileTypeCount = 0;
static uintptr_t structScanTileTypes[STRUCT_SCAN_MAX_TILE_TYPES]{};
static uintptr_t structScanTileMapAddress = 0;
constexpr size_t STRUCT_SCAN_MAX_PENDING_INPUTS = 2;
struct StructScanPendingInput
{
    uintptr_t address;
    uintptr_t skillAddress;
    uint8_t message[MSG_SIZE + MSG_SLACK];
    uint8_t skillData[0x60];
    bool hasSkillData;
};
static StructScanPendingInput
    structScanPendingInputs[STRUCT_SCAN_MAX_PENDING_INPUTS]{};
static size_t structScanInputMessageCount = 0;
struct StructScanTypeSamples
{
    uintptr_t vtable;
    size_t samples;
};
struct StructScanObjectSamples
{
    uintptr_t vtable;
    size_t samples;
};
static StructScanTypeSamples structScanDataTypes[STRUCT_SCAN_MAX_DATA_TYPES]{};

static StructScanObjectSamples
    structScanObjectTypes[STRUCT_SCAN_MAX_DATA_TYPES]{};
static size_t structScanObjectTypeCount = 0;
static size_t structScanDataVtableCount = 0;
static size_t structScanProjectileCount = 0;
static os_unfair_lock structScanLock = OS_UNFAIR_LOCK_INIT;

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

float wallPenalty(
    float mx, float my, float dx, float dy, float speed, float bodyR)
{
    if (!wall_ready()) return 0.0f;
    float step = speed * PROBE_T;
    int blocked = 0;
    for (int s = 1; s <= PROBE_N; s++)
        if (wall_is_blocked_wide(mx + dx * step * s,
                                 my + dy * step * s,
                                 bodyR,
                                 WALL_BLOCKS_MOVEMENT))
            blocked++;
    return WALL_HIT * (float)blocked / (float)PROBE_N;
}

float dangerFor(
    float mx, float my, float dx, float dy, float speed, float bodyR)
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
        if (ts < 0.0f)
            ts = 0.0f;
        else if (ts > HORIZON_S)
            ts = HORIZON_S;
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

void resetVelocityTracks()
{
    trackCount = 0;
}

static Track* trackFor(uintptr_t data, int32_t px, int32_t py, uint64_t now)
{
    Track* slot = nullptr;
    for (int i = 0; i < trackCount; i++)
    {
        if (tracks[i].data == data)
        {
            slot = &tracks[i];
            break;
        }
    }
    if (slot == nullptr)
    {
        int reuse = -1;
        for (int i = 0; i < trackCount; i++)
        {
            if (now - tracks[i].lastMs > TRACK_TTL_MS)
            {
                reuse = i;
                break;
            }
        }
        if (reuse < 0)
        {
            if (trackCount < MAX_TRACK)
                reuse = trackCount++;
            else
                reuse = 0;
        }
        slot = &tracks[reuse];
        slot->data = data;
        slot->lastX = px;
        slot->lastY = py;
        slot->lastMs = now;
        slot->vx = 0.0f;
        slot->vy = 0.0f;
        slot->hasVelocity = false;
        return slot;
    }

    float dt = (float)(now - slot->lastMs) / 1000.0f;
    float ddx = (float)(px - slot->lastX);
    float ddy = (float)(py - slot->lastY);
    if (dt > DT_MIN && dt < DT_MAX && ddx * ddx + ddy * ddy >= POS_DELTA_MIN_SQ)
    {
        float alpha = slot->hasVelocity ? VEL_ALPHA_EXIST : VEL_ALPHA_NEW;
        slot->vx = slot->vx * (SPEED_MIN - alpha) + (ddx / dt) * alpha;
        slot->vy = slot->vy * (SPEED_MIN - alpha) + (ddy / dt) * alpha;
        slot->hasVelocity = true;
    }
    slot->lastX = px;
    slot->lastY = py;
    slot->lastMs = now;
    return slot;
}

void collectThreats(float mx, float my, float myRadius)
{
    thrCount = 0;
    entCount = 0;
    const uint64_t nowMs = static_cast<uint64_t>(g_get_real_time() / 1000);
    float zoneSq = AWARE * AWARE;
    uintptr_t projVt = base + static_cast<uintptr_t>(VTABLE_PROJ_DATA_RVA);

    for (int i = 0; i < obj_count && thrCount < MAX_THREATS; i++)
    {
        if (!obj_valid[i]) continue;
        uintptr_t obj = obj_snapshot[i];
        if (!obj) continue;

        void* dataRaw = nullptr;
        if (!read_ptr(obj + static_cast<uintptr_t>(ELEM_DEF_OFF), &dataRaw) ||
            !dataRaw)
            continue;
        uintptr_t data = (uintptr_t)dataRaw;

        void* vtRaw = nullptr;
        if (!read_ptr(data, &vtRaw) || (uintptr_t)vtRaw != projVt) continue;

        int32_t team = -1, px = 0, py = 0;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_TEAM_OFF), &team) ||
            team == own_team_b)
            continue;

        int32_t deadFlag = 0;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_DEADFLAG_OFF),
                      &deadFlag) ||
            deadFlag != 0)
            continue;

        if (!read_int(obj + static_cast<uintptr_t>(OBJ_X_OFF), &px)) continue;
        if (!read_int(obj + static_cast<uintptr_t>(OBJ_Y_OFF), &py)) continue;

        int32_t raw6c_i = 0;
        read_int(data + RAW_6C_OFF, &raw6c_i);
        float raw6c_f = 0.0f;
        memcpy(&raw6c_f, &raw6c_i, sizeof(raw6c_f));

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
        read_int(data + static_cast<uintptr_t>(PROJECTILEDATA_SPEED_OFF),
                 &speed);
        read_int(data + static_cast<uintptr_t>(PROJECTILEDATA_RADIUS_OFF),
                 &radius);

        const Track* track = trackFor(data, px, py, nowMs);
        float vx = track->hasVelocity ? track->vx : 0.0f;
        float vy = track->hasVelocity ? track->vy : 0.0f;

        thr[thrCount].x = (float)px;
        thr[thrCount].y = (float)py;
        thr[thrCount].vx = vx;
        thr[thrCount].vy = vy;
        thr[thrCount].rad = (float)radius * RADIUS_SCALE + myRadius;
        thr[thrCount].raw6c_i = raw6c_i;
        thr[thrCount].rawSpeed = speed;
        thr[thrCount].raw6c_f = raw6c_f;
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
         dodge_count,
         hit_count,
         (int)own_hp,
         active,
         moving);
    line("[ad] pos=(%.0f,%.0f) spd=%.0f r=%.0f tgt=(%d,%d)\n",
         mx,
         my,
         speed,
         myRadius,
         ad_target_x,
         ad_target_y);
    line("[ad] thr=%d ent=%d near_ttc=%.3f\n", thrCount, entCount, minTtc);

    for (int i = 0; i < thrCount && i < 8; i++)
        line("  t%d: (%.0f,%.0f) v=(%.0f,%.0f) |v|=%.0f exp=%.0f r=%.0f "
             "tca=%.3f raw6c_i=%d rs=%d raw6c_f=%.3f\n",
             i,
             thr[i].x,
             thr[i].y,
             thr[i].vx,
             thr[i].vy,
             sqrtf(thr[i].vx * thr[i].vx + thr[i].vy * thr[i].vy),
             (float)thr[i].rawSpeed * SPEED_SCALE,
             thr[i].rad,
             tcaFor(thr[i], mx, my),
             thr[i].raw6c_i,
             thr[i].rawSpeed,
             thr[i].raw6c_f);

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
    if (el > LEN_EPS)
    {
        ex /= el;
        ey /= el;
    }

    float gx = 0.0f, gy = 0.0f;
    if (wall_cache_w() > 0 && wall_cache_h() > 0)
    {
        gx = (wall_cache_w() * WALL_TILE_SIZE * HALF) - mx;
        gy = (wall_cache_h() * WALL_TILE_SIZE * HALF) - my;
        float gl = sqrtf(gx * gx + gy * gy);
        if (gl > LEN_EPS)
        {
            gx /= gl;
            gy /= gl;
        }
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
        float inter =
            escapeScore * INTEREST_ESCAPE_W + goalScore * INTEREST_GOAL_W;
        bool better = false;
        if (d < bestDanger - 0.02f)
            better = true;
        else if (fabsf(d - bestDanger) <= 0.02f && inter > bestInterest)
            better = true;
        if (better)
        {
            bestDanger = d;
            bestInterest = inter;
            bestIdx = i;
        }
    }

    float botAngle =
        atan2f((float)(ad_target_y - own_y), (float)(ad_target_x - own_x)) *
        (180.0f / (float)M_PI);
    line("[ad] best=%d (%.1f) bot=%.1f\n",
         bestIdx,
         (float)bestIdx * 90.0f,
         botAngle);
}

static uintptr_t scan_call(uintptr_t fn, uintptr_t a, uintptr_t b)
{
    if (!fn || !base) return 0;
    uintptr_t rva = (fn >= base) ? (fn - base) : fn;
    uintptr_t target = callable(rva);
    if (!target) return 0;
    using fn_t = uintptr_t (*)(uintptr_t, uintptr_t);
    return ((fn_t)target)(a, b);
}

static bool struct_scan_remember(uintptr_t address, const char* label)
{
    const char* recordLabel = label ? label : "?";
    for (size_t i = 0; i < structScanRecordCount; i++)
        if (structScanRecords[i].address == address &&
            strcmp(structScanRecords[i].label, recordLabel) == 0)
            return false;
    if (structScanRecordCount >= STRUCT_SCAN_MAX_RECORDS) return false;
    StructScanRecord& record = structScanRecords[structScanRecordCount++];
    record.address = address;
    snprintf(record.label, sizeof(record.label), "%s", recordLabel);
    return true;
}

static bool struct_scan_dump_buffer(uintptr_t address,
                                    const char* label,
                                    const uint8_t* source,
                                    size_t bytes,
                                    int32_t pid)
{
    if (!source || !bytes || bytes > STRUCT_SCAN_MAX_BYTES) return false;
    uint8_t buffer[STRUCT_SCAN_MAX_BYTES];
    memcpy(buffer, source, bytes);
    const char* safeLabel = label ? label : "?";
    size_t redactedBegin = bytes;
    size_t redactedEnd = bytes;
    if (strcmp(safeLabel, "BattleMode") == 0)
    {
        redactedBegin = static_cast<size_t>(BM_HASH_KEY_OFF);
        redactedEnd = redactedBegin + 0x10;
    }
    else if (strncmp(safeLabel, "ClientInputMessage", 18) == 0)
    {
        redactedBegin = static_cast<size_t>(CI_TOKEN_OFF);
        redactedEnd = redactedBegin + sizeof(uint32_t);
    }
    if (redactedBegin < bytes)
    {
        if (redactedEnd > bytes) redactedEnd = bytes;
        memset(buffer + redactedBegin, 0, redactedEnd - redactedBegin);
    }

    os_unfair_lock_lock(&structScanLock);
    const bool remembered = struct_scan_remember(address, safeLabel);
    if (!remembered)
    {
        os_unfair_lock_unlock(&structScanLock);
        return false;
    }
    os_unfair_lock_unlock(&structScanLock);
    line("[STRUCTSCAN] begin label=%s address=%p bytes=0x%zx pid=%d\n",
         safeLabel,
         (void*)address,
         bytes,
         pid);
    if (redactedBegin < bytes)
        line("[STRUCTSCAN] redacted label=%s begin=0x%zx end=0x%zx\n",
             safeLabel,
             redactedBegin,
             redactedEnd);
    for (size_t off = 0; off + sizeof(uint32_t) <= bytes; off += 4)
    {
        uint32_t bits = 0;
        int32_t integer = 0;
        float real = 0.0f;
        memcpy(&bits, buffer + off, sizeof(bits));
        memcpy(&integer, buffer + off, sizeof(integer));
        memcpy(&real, buffer + off, sizeof(real));
        line("[STRUCTSCAN] field label=%s pid=%d off=0x%03zx bits=%08x i=%d "
             "f=%.6g\n",
             safeLabel,
             pid,
             off,
             bits,
             integer,
             real);
    }
    line("[STRUCTSCAN] end label=%s address=%p\n", safeLabel, (void*)address);
    return true;
}

static bool struct_scan_dump(uintptr_t address,
                             const char* label,
                             size_t bytes,
                             int32_t pid)
{
    uint8_t buffer[STRUCT_SCAN_MAX_BYTES];
    if (!address || !bytes || bytes > sizeof(buffer) ||
        !read_bytes(address, buffer, bytes))
    {
        line("[STRUCTSCAN] unreadable label=%s address=%p bytes=0x%zx pid=%d\n",
             label ? label : "?",
             (void*)address,
             bytes,
             pid);
        return false;
    }
    return struct_scan_dump_buffer(address, label, buffer, bytes, pid);
}

static bool struct_scan_vtable_in_image(uintptr_t vtable)
{
    return vtable >= base + static_cast<uintptr_t>(DC_RVA_LO) &&
           vtable < base + static_cast<uintptr_t>(DC_RVA_LO + DC_RVA_SIZE);
}

static void struct_scan_string(uintptr_t value, const char* label)
{
    if (!value || !pointer_plausible(value)) return;
    struct_scan_dump(value, label, 0x20, 0);
    char text[96]{};
    if (read_scstring(value, SCSTRING_DEFAULT_MAX_LENGTH, text, sizeof(text)))
        line("[STRUCTSCAN] string label=%s address=%p text=%s\n",
             label,
             (void*)value,
             text);
}

static void struct_scan_tilemap(uintptr_t tileMap)
{
    int32_t width = 0, height = 0;
    void* arrayRaw = nullptr;
    if (!tileMap || !pointer_plausible(tileMap)) return;
    if (!read_int(tileMap + static_cast<uintptr_t>(TILEMAP_W_OFF), &width) ||
        !read_int(tileMap + static_cast<uintptr_t>(TILEMAP_H_OFF), &height) ||
        width <= 0 || width > WALL_MAX_MAP_TILES || height <= 0 ||
        height > WALL_MAX_MAP_TILES ||
        !read_ptr(tileMap + static_cast<uintptr_t>(TILEMAP_ARRAY_OFF),
                  &arrayRaw) ||
        !arrayRaw)
    {
        os_unfair_lock_lock(&structScanLock);
        if (structScanTileMapAddress == tileMap) structScanTileMapAddress = 0;
        os_unfair_lock_unlock(&structScanLock);
        line("[STRUCTSCAN] tilemap-unavailable address=%p\n", (void*)tileMap);
        return;
    }

    os_unfair_lock_lock(&structScanLock);
    const bool alreadyScanned = structScanTileMapAddress == tileMap;
    if (!alreadyScanned) structScanTileMapAddress = tileMap;
    os_unfair_lock_unlock(&structScanLock);
    if (alreadyScanned)
    {
        line("[STRUCTSCAN] tilemap-skip address=%p reason=already-scanned\n",
             (void*)tileMap);
        return;
    }
    struct_scan_dump(tileMap, "TileMap", 0x100, 0);

    const uintptr_t array = reinterpret_cast<uintptr_t>(arrayRaw);
    const size_t total =
        static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t probes = total;
    size_t typeOverflow = 0;
    bool tileSampleDumped = false;

    for (size_t probe = 0; probe < total; probe++)
    {
        const size_t index = probe;
        void* tileRaw = nullptr;
        if (!read_ptr(array + index * sizeof(uintptr_t), &tileRaw) || !tileRaw)
            continue;
        const uintptr_t tile = reinterpret_cast<uintptr_t>(tileRaw);
        void* typeRaw = nullptr;
        if (!pointer_plausible(tile) || !read_ptr(tile, &typeRaw) || !typeRaw)
            continue;
        const uintptr_t type = reinterpret_cast<uintptr_t>(typeRaw);
        if (!pointer_plausible(type)) continue;
        if (!tileSampleDumped)
        {
            tileSampleDumped = struct_scan_dump(tile, "Tile/sample", 0x20, 0);
        }

        if (structScanTileTypeCount >= STRUCT_SCAN_MAX_TILE_TYPES)
        {
            typeOverflow++;
            continue;
        }
        bool typeSeen = false;
        for (size_t i = 0; i < structScanTileTypeCount; i++)
            if (structScanTileTypes[i] == type) typeSeen = true;
        if (typeSeen) continue;

        char typeLabel[48];
        snprintf(typeLabel,
                 sizeof(typeLabel),
                 "TileType/sample-%zu",
                 structScanTileTypeCount);
        if (struct_scan_dump(type, typeLabel, 0x60, 0))
            structScanTileTypes[structScanTileTypeCount++] = type;
    }
    line("[STRUCTSCAN] tilemap-samples width=%d height=%d probed=%zu "
         "tileTypes=%zu typeOverflow=%zu\n",
         width,
         height,
         probes,
         structScanTileTypeCount,
         typeOverflow);
}

static void struct_scan_flush_input_messages()
{
    StructScanPendingInput pending[STRUCT_SCAN_MAX_PENDING_INPUTS]{};
    size_t count = 0;
    os_unfair_lock_lock(&structScanLock);
    count = structScanInputMessageCount;
    if (count > STRUCT_SCAN_MAX_PENDING_INPUTS)
        count = STRUCT_SCAN_MAX_PENDING_INPUTS;
    memcpy(pending, structScanPendingInputs, count * sizeof(pending[0]));
    memset(structScanPendingInputs, 0, sizeof(structScanPendingInputs));
    structScanInputMessageCount = 0;
    os_unfair_lock_unlock(&structScanLock);

    for (size_t i = 0; i < count; i++)
    {
        struct_scan_dump_buffer(pending[i].address,
                                "ClientInputMessage",
                                pending[i].message,
                                sizeof(pending[i].message),
                                0);
        if (pending[i].hasSkillData)
            struct_scan_dump_buffer(pending[i].skillAddress,
                                    "ClientInputSkillData",
                                    pending[i].skillData,
                                    sizeof(pending[i].skillData),
                                    0);
    }
}

static void scan_offset_audit()
{
    static uint64_t lastMs = 0;
    const uint64_t now = us() / 1000;
    if (lastMs && now - lastMs < 3000) return;
    lastMs = now;
    if (!own_elem) return;

    int32_t x = 0, y = 0, oi = 0, tm = 0, gid = 0, hp = 0, hp2 = 0, d0 = 0,
            alive140 = 0, act1e8 = 0;
    read_int(own_elem + 0x30, &x);
    read_int(own_elem + 0x34, &y);
    read_int(own_elem + 0x3c, &oi);
    read_int(own_elem + 0x40, &tm);
    read_int(own_elem + 0x08, &gid);
    read_int(own_elem + 0xc4, &hp);
    read_int(own_elem + 0xc8, &hp2);
    read_int(own_elem + 0xd0, &d0);
    read_int(own_elem + 0x140, &alive140);
    read_int(own_elem + 0x1e8, &act1e8);
    void* dataRaw = nullptr;
    read_ptr(own_elem + 0x10, &dataRaw);
    const uintptr_t data = reinterpret_cast<uintptr_t>(dataRaw);
    line("[OFFCHK] own=%p x=%d y=%d oi=%d team=%d gid=%d hp=%d hp2=%d "
         "d0=%d alive140=%d act1e8=%d def=%p\n",
         (void*)own_elem,
         x,
         y,
         oi,
         tm,
         gid,
         hp,
         hp2,
         d0,
         alive140,
         act1e8,
         (void*)data);

    if (data)
    {
        int32_t d6c = 0, d74 = 0, d118 = 0, d124 = 0, d130 = 0, d13c = 0;
        int32_t d1d4 = 0, d1d8 = 0, d200 = 0, d208 = 0, d210 = 0, d218 = 0;
        int32_t d294 = 0, d298 = 0, d50 = 0, d54 = 0;
        read_int(data + 0x50, &d50);
        read_int(data + 0x54, &d54);
        read_int(data + 0x6c, &d6c);
        read_int(data + 0x74, &d74);
        read_int(data + 0x118, &d118);
        read_int(data + 0x124, &d124);
        read_int(data + 0x130, &d130);
        read_int(data + 0x13c, &d13c);
        read_int(data + 0x1d4, &d1d4);
        read_int(data + 0x1d8, &d1d8);
        read_int(data + 0x200, &d200);
        read_int(data + 0x208, &d208);
        read_int(data + 0x210, &d210);
        read_int(data + 0x218, &d218);
        read_int(data + 0x294, &d294);
        read_int(data + 0x298, &d298);
        line("[OFFCHK] def d50=%d d54=%d d6c=%d d74=%d d118=%d d124=%d "
             "d130=%d d13c=%d d1d4=%d d1d8=%d d200=%d d208=%d d210=%d "
             "d218=%d d294=%d d298=%d\n",
             d50,
             d54,
             d6c,
             d74,
             d118,
             d124,
             d130,
             d13c,
             d1d4,
             d1d8,
             d200,
             d208,
             d210,
             d218,
             d294,
             d298);

        const int32_t rvSpeed =
            static_cast<int32_t>(scan_call(addr_charspeed, data, 0));
        const int32_t rvRadius =
            static_cast<int32_t>(scan_call(addr_charradius, data, 0));
        const int32_t rvEarly =
            static_cast<int32_t>(scan_call(addr_projearly, data, 0));
        const uintptr_t rvBeam = scan_call(addr_projbeam, data, 0);
        const uintptr_t rvName = scan_call(addr_dataname, data, 0);
        char nameText[64]{};
        if (rvName)
            read_scstring(rvName,
                          SCSTRING_DEFAULT_MAX_LENGTH,
                          nameText,
                          sizeof(nameText));
        line("[OFFCHK] rva speed=%d radius=%d early=%d beam=%p name=%p "
             "text=%s\n",
             rvSpeed,
             rvRadius,
             rvEarly,
             (void*)rvBeam,
             (void*)rvName,
             nameText[0] ? nameText : "?");
    }

    uintptr_t proj = 0;
    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i]) continue;
        const uintptr_t obj = obj_snapshot[i];
        if (!obj) continue;
        int32_t pg = 0;
        if (!read_int(obj + 0x08, &pg)) continue;
        if (pg >= 2000000 && pg < 3000000)
        {
            proj = obj;
            break;
        }
    }
    if (proj)
    {
        int32_t px = 0, py = 0, p38 = 0, p3c = 0, p40 = 0, pd0 = 0, p118 = 0;
        read_int(proj + 0x30, &px);
        read_int(proj + 0x34, &py);
        read_int(proj + 0x38, &p38);
        read_int(proj + 0x3c, &p3c);
        read_int(proj + 0x40, &p40);
        read_int(proj + 0xd0, &pd0);
        read_int(proj + 0x118, &p118);
        void* pdataRaw = nullptr;
        read_ptr(proj + 0x10, &pdataRaw);
        const uintptr_t pdata = reinterpret_cast<uintptr_t>(pdataRaw);
        char pname[64]{};
        if (pdata)
        {
            const uintptr_t s = scan_call(addr_dataname, pdata, 0);
            if (s)
                read_scstring(
                    s, SCSTRING_DEFAULT_MAX_LENGTH, pname, sizeof(pname));
        }
        line("[OFFCHK] proj=%p x=%d y=%d p38=%d oi=%d team=%d o118=%d oD0=%d "
             "def=%p name=%s\n",
             (void*)proj,
             px,
             py,
             p38,
             p3c,
             p40,
             p118,
             pd0,
             (void*)pdata,
             pname[0] ? pname : "?");
    }

    if (proj)
    {
        void* pdefRaw = nullptr;
        read_ptr(proj + 0x10, &pdefRaw);
        const uintptr_t pdef = reinterpret_cast<uintptr_t>(pdefRaw);
        if (pdef)
        {
            int32_t s6c = 0, s74 = 0, s124 = 0, s1d4 = 0, s1d8 = 0;
            read_int(pdef + 0x6c, &s6c);
            read_int(pdef + 0x74, &s74);
            read_int(pdef + 0x124, &s124);
            read_int(pdef + 0x1d4, &s1d4);
            read_int(pdef + 0x1d8, &s1d8);
            line("[OFFCHK] pdef=%p d6c=%d d74=%d d124=%d d1d4=%d d1d8=%d\n",
                 (void*)pdef,
                 s6c,
                 s74,
                 s124,
                 s1d4,
                 s1d8);
        }
    }

    int shownEnemy = 0;
    for (int i = 0; i < obj_count && shownEnemy < 2; i++)
    {
        if (!obj_valid[i]) continue;
        const uintptr_t obj = obj_snapshot[i];
        if (!obj || obj == own_elem) continue;
        int32_t eg = 0, et = 0, eh = 0, ex = 0, ey = 0;
        if (!read_int(obj + 0x08, &eg) || eg <= 0 || eg >= 2000000) continue;
        read_int(obj + 0x40, &et);
        read_int(obj + 0xc4, &eh);
        read_int(obj + 0x30, &ex);
        read_int(obj + 0x34, &ey);
        if (et < 0 || et == own_team_b || eh <= 0) continue;
        void* edefRaw = nullptr;
        read_ptr(obj + 0x10, &edefRaw);
        const uintptr_t edef = reinterpret_cast<uintptr_t>(edefRaw);
        char ename[64]{};
        int32_t espd = -1;
        if (edef)
        {
            const uintptr_t ns = scan_call(addr_dataname, edef, 0);
            if (ns)
                read_scstring(
                    ns, SCSTRING_DEFAULT_MAX_LENGTH, ename, sizeof(ename));
            if (addr_charspeed)
                espd = static_cast<int32_t>(scan_call(addr_charspeed, edef, 0));
        }
        line("[OFFCHK] enemy=%p name=%s team=%d hp=%d x=%d y=%d spd=%d\n",
             (void*)obj,
             ename[0] ? ename : "?",
             et,
             eh,
             ex,
             ey,
             espd);
        shownEnemy++;
    }

    if (scene_object)
    {
        void* ctrlRaw = nullptr;
        uintptr_t ctrl = 0;
        if (read_ptr(scene_object + static_cast<uintptr_t>(CLIENT_OFF),
                     &ctrlRaw) &&
            ctrlRaw)
            ctrl = reinterpret_cast<uintptr_t>(ctrlRaw);
        int32_t cOwn = 0, cTeam = 0, cCount = 0, cMoveX = 0, cMoveY = 0,
                cMoveOn = 0, cMoveZero = 0;
        if (ctrl)
        {
            read_int(ctrl + 0xe0, &cOwn);
            read_int(ctrl + 0xe4, &cTeam);
            read_int(ctrl + 0x0c, &cCount);
            read_int(ctrl + 0xfcc, &cMoveX);
            read_int(ctrl + 0xfd0, &cMoveY);
            read_int(ctrl + 0xf9c, &cMoveOn);
            read_int(ctrl + 0xfa0, &cMoveZero);
        }
        line("[OFFCHK] ctrl=%p ownIdx=%d ownTeam=%d count=%d mvX=%d mvY=%d "
             "mvOn=%d mvZero=%d\n",
             (void*)ctrl,
             cOwn,
             cTeam,
             cCount,
             cMoveX,
             cMoveY,
             cMoveOn,
             cMoveZero);
    }

    const uintptr_t rvOwnChar = scan_call(addr_getownchar, scene_object, 0);
    const uintptr_t rvOwnTeam = scan_call(addr_getteam, scene_object, 0);
    const uintptr_t rvOwnIdx = scan_call(addr_getownidx, scene_object, 0);
    const uintptr_t rvGetX = scan_call(addr_getx, own_elem, 0);
    const uintptr_t rvGetY = scan_call(addr_gety, own_elem, 0);
    const uintptr_t rvGetGid = scan_call(addr_getgid, own_elem, 0);
    const uintptr_t rvGetData = scan_call(addr_getdata, own_elem, 0);
    line("[OFFCHK] rva2 ownChar=%p ownTeam=%p ownIdx=%p getX=%p getY=%p "
         "getGid=%p getData=%p\n",
         (void*)rvOwnChar,
         (void*)rvOwnTeam,
         (void*)rvOwnIdx,
         (void*)rvGetX,
         (void*)rvGetY,
         (void*)rvGetGid,
         (void*)rvGetData);
}

static void scan_required_structures(bool newMatch)
{
    if (newMatch)
    {
        os_unfair_lock_lock(&structScanLock);
        structScanRecordCount = 0;
        structScanTileTypeCount = 0;
        structScanDataVtableCount = 0;
        structScanObjectTypeCount = 0;
        structScanProjectileCount = 0;
        structScanTileMapAddress = 0;
        structScanInputMessageCount = 0;
        memset(structScanPendingInputs, 0, sizeof(structScanPendingInputs));
        memset(structScanDataTypes, 0, sizeof(structScanDataTypes));
        memset(structScanObjectTypes, 0, sizeof(structScanObjectTypes));
        memset(structScanTileTypes, 0, sizeof(structScanTileTypes));
        os_unfair_lock_unlock(&structScanLock);
    }
    if (!base || !scene_object || !own_elem) return;
    struct_scan_flush_input_messages();

    line("[STRUCTSCAN] pass objects=%d own=%p scene=%p manager=%p "
         "tilemap-cache=%dx%d\n",
         obj_count,
         (void*)own_elem,
         (void*)scene_object,
         (void*)manager_ptr,
         wall_cache_w(),
         wall_cache_h());

    int32_t ownPid = 0;
    read_int(own_elem + static_cast<uintptr_t>(OBJ_GLOBALID_OFF), &ownPid);
    struct_scan_dump(own_elem, "GameObjectClient/own", OBJ_BLOCK_SIZE, ownPid);

    // State slot and scene/client chain consumed by scan_world/client().
    const uintptr_t stateSlot =
        reinterpret_cast<uintptr_t>(read_global_ptr(STATE_RVA));
    struct_scan_dump(stateSlot, "BattleState", 0x58, 0);
    struct_scan_dump(scene_object, "Scene", 0x80, 0);
    void* ctrlRaw = nullptr;
    if (read_ptr(scene_object + static_cast<uintptr_t>(CLIENT_OFF), &ctrlRaw) &&
        ctrlRaw)
        struct_scan_dump(
            reinterpret_cast<uintptr_t>(ctrlRaw), "BattleClient", 0x80, 0);
    struct_scan_dump(manager_ptr, "GameObjectManager", 0x40, 0);
    if (manager_array_ptr && manager_count_v > 0 &&
        manager_count_v <= OBJECT_COUNT_MAX)
    {
        const size_t arrayBytes =
            static_cast<size_t>(manager_count_v) * sizeof(uintptr_t);
        uint8_t arrayHead[STRUCT_SCAN_MAX_BYTES]{};
        const size_t maxArrayBytes =
            STRUCT_SCAN_MAX_BYTES - (STRUCT_SCAN_MAX_BYTES % sizeof(uintptr_t));
        const size_t bytes =
            arrayBytes < maxArrayBytes ? arrayBytes : maxArrayBytes;
        if (read_bytes(manager_array_ptr, arrayHead, bytes))
        {
            char label[48];
            snprintf(label,
                     sizeof(label),
                     "GameObjectManager/array-%d",
                     manager_count_v);
            struct_scan_dump(manager_array_ptr, label, bytes, 0);
        }
    }

    // The other project-owned data layouts are grouped by validated in-image
    // vtable.
    const uintptr_t projVt =
        base + static_cast<uintptr_t>(VTABLE_PROJ_DATA_RVA);
    if (addr_getdata)
    {
        for (int i = 0; i < obj_count; i++)
        {
            if (!obj_valid[i]) continue;
            const uintptr_t obj = obj_snapshot[i];
            if (!obj || !pointer_plausible(obj)) continue;
            void* objectVtableRaw = nullptr;
            if (read_ptr(obj, &objectVtableRaw) && objectVtableRaw)
            {
                const uintptr_t objectVtable =
                    reinterpret_cast<uintptr_t>(objectVtableRaw);
                if (struct_scan_vtable_in_image(objectVtable))
                {
                    size_t objectType = 0;
                    while (objectType < structScanObjectTypeCount &&
                           structScanObjectTypes[objectType].vtable !=
                               objectVtable)
                        objectType++;
                    if (objectType == structScanObjectTypeCount &&
                        structScanObjectTypeCount < STRUCT_SCAN_MAX_DATA_TYPES)
                    {
                        structScanObjectTypes[objectType].vtable = objectVtable;
                        structScanObjectTypeCount++;
                    }
                    if (objectType < structScanObjectTypeCount &&
                        structScanObjectTypes[objectType].samples <
                            STRUCT_SCAN_MAX_OBJECT_SAMPLES)
                    {
                        char objectLabel[48];
                        snprintf(objectLabel,
                                 sizeof(objectLabel),
                                 "GameObject/vt-%llx-sample-%zu",
                                 static_cast<unsigned long long>(objectVtable -
                                                                 base),
                                 structScanObjectTypes[objectType].samples);
                        if (struct_scan_dump(
                                obj, objectLabel, OBJ_BLOCK_SIZE, 0))
                            structScanObjectTypes[objectType].samples++;
                    }
                }
            }
            int32_t pid = 0;
            read_int(obj + static_cast<uintptr_t>(OBJ_GLOBALID_OFF), &pid);

            void* dataRaw = nullptr;
            if (!read_ptr(obj + static_cast<uintptr_t>(ELEM_DEF_OFF),
                          &dataRaw) ||
                !dataRaw)
                continue;
            const uintptr_t data = reinterpret_cast<uintptr_t>(dataRaw);
            void* vtableRaw = nullptr;
            if (!read_ptr(data, &vtableRaw) || !vtableRaw) continue;
            const uintptr_t vtable = reinterpret_cast<uintptr_t>(vtableRaw);

            if (vtable == projVt)
            {
                if (structScanProjectileCount < STRUCT_SCAN_MAX_PROJECTILES &&
                    struct_scan_dump(
                        data, "ProjectileData", STRUCT_SCAN_MAX_BYTES, pid))
                    structScanProjectileCount++;
                continue;
            }
            if (!struct_scan_vtable_in_image(vtable)) continue;

            size_t typeIndex = 0;
            while (typeIndex < structScanDataVtableCount &&
                   structScanDataTypes[typeIndex].vtable != vtable)
                typeIndex++;
            if (typeIndex >= STRUCT_SCAN_MAX_DATA_TYPES) continue;
            if (typeIndex == structScanDataVtableCount)
            {
                structScanDataTypes[typeIndex].vtable = vtable;
                structScanDataVtableCount++;
            }
            if (structScanDataTypes[typeIndex].samples >= 3) continue;

            char name[64] = "?";
            const uintptr_t nameValue = scan_call(addr_dataname, data, 0);
            if (nameValue)
                read_scstring(
                    nameValue, SCSTRING_DEFAULT_MAX_LENGTH, name, sizeof(name));
            char label[48];
            snprintf(label,
                     sizeof(label),
                     "DataType/vt-%llx-sample-%zu-%s",
                     static_cast<unsigned long long>(vtable - base),
                     structScanDataTypes[typeIndex].samples,
                     name);
            if (struct_scan_dump(data, label, 0x100, pid))
                structScanDataTypes[typeIndex].samples++;
            if (nameValue) struct_scan_string(nameValue, "SCString/data-name");
        }
    }

    const uintptr_t tileMapFn = callable(RVA_LOGICBATTLEMODECLIENT_GETTILEMAP);
    const uintptr_t ctrl = controller();
    const uintptr_t tileMap =
        tileMapFn && ctrl ? scan_call(tileMapFn, ctrl, 0) : 0;
    if (tileMap) struct_scan_tilemap(tileMap);

    // Capture the live input manager and queue links without enqueueing a test
    // command.
    const uintptr_t battleFn = callable(RVA_BATTLEMODE_GETINSTANCE);
    uintptr_t battle = 0;
    if (battleFn)
    {
        using battle_fn_t = uintptr_t (*)();
        battle = reinterpret_cast<battle_fn_t>(battleFn)();
    }
    if (battle && pointer_plausible(battle))
    {
        struct_scan_dump(battle, "BattleMode", 0x80, 0);
        void* inputManagerRaw = nullptr;
        if (read_ptr(battle + static_cast<uintptr_t>(MGR_OFF),
                     &inputManagerRaw) &&
            inputManagerRaw &&
            pointer_plausible(reinterpret_cast<uintptr_t>(inputManagerRaw)))
        {
            const uintptr_t inputManager =
                reinterpret_cast<uintptr_t>(inputManagerRaw);
            struct_scan_dump(inputManager, "ClientInputManager", 0x50, 0);
            void* queueRaw = nullptr;
            if (read_ptr(inputManager +
                             static_cast<uintptr_t>(CI_MGR_QUEUE_OFF),
                         &queueRaw) &&
                queueRaw &&
                pointer_plausible(reinterpret_cast<uintptr_t>(queueRaw)))
                struct_scan_dump(reinterpret_cast<uintptr_t>(queueRaw),
                                 "ClientInputQueue",
                                 0x40,
                                 0);
        }
    }
    scan_offset_audit();
    line("[STRUCTSCAN] totals records=%zu projectileDefinitions=%zu "
         "otherDataTypes=%zu objectTypes=%zu tileTypes=%zu\n",
         structScanRecordCount,
         structScanProjectileCount,
         structScanDataVtableCount,
         structScanObjectTypeCount,
         structScanTileTypeCount);
}

void scan_projectile_data(bool newMatch)
{
    struct ScanStats
    {
        uint32_t objects, invalidSnapshot, nullObject, dataReadFail,
            vtableReadFail;
        uint32_t knownProjectileVtable, otherDataVtable, moduleDataVtable;
        uint32_t outOfModuleVtable, projectileDataReadFail,
            projectileFieldReadFail;
        uint32_t projectileIndirectReadFail, projectileZeroPid,
            projectileOwnTeam;
        uint32_t projectileDead, projectileOutsideRange, projectileInRange;
        uint32_t projectileEligible;
    };
    struct VtableBucket
    {
        uintptr_t vtable;
        uintptr_t sampleData;
        int32_t samplePid;
        uint32_t count;
        char name[48];
    };
    constexpr size_t DETAIL_LIMIT = 128;
    constexpr size_t VTABLE_LIMIT = 16;
    static uintptr_t detailedData[DETAIL_LIMIT]{};
    static size_t detailedCount = 0;
    static uint32_t detailOverflow = 0;
    static ScanStats totals{};
    static VtableBucket vtables[VTABLE_LIMIT]{};
    static size_t vtableCount = 0;
    static uint32_t vtableOverflow = 0;
    static uint64_t lastReportMs = 0;

    if (newMatch)
    {
        detailedCount = 0;
        detailOverflow = 0;
        totals = {};
        vtableCount = 0;
        vtableOverflow = 0;
        lastReportMs = 0;
    }
    if (!base) return;
    ScanStats current{};
    current.objects = obj_count > 0 ? static_cast<uint32_t>(obj_count) : 0;
    const uintptr_t projVt =
        base + static_cast<uintptr_t>(VTABLE_PROJ_DATA_RVA);
    const uintptr_t dataEnd = static_cast<uintptr_t>(DC_RVA_LO + DC_RVA_SIZE);
    const float zoneSq = AWARE * AWARE;

    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i])
        {
            current.invalidSnapshot++;
            continue;
        }
        const uintptr_t obj = obj_snapshot[i];
        if (!obj)
        {
            current.nullObject++;
            continue;
        }

        void* dataRaw = nullptr;
        if (!read_ptr(obj + static_cast<uintptr_t>(ELEM_DEF_OFF), &dataRaw) ||
            !dataRaw)
        {
            current.dataReadFail++;
            continue;
        }
        const uintptr_t data = reinterpret_cast<uintptr_t>(dataRaw);
        void* vtRaw = nullptr;
        if (!read_ptr(data, &vtRaw) || !vtRaw)
        {
            current.vtableReadFail++;
            continue;
        }
        const uintptr_t vtable = reinterpret_cast<uintptr_t>(vtRaw);
        if (vtable != projVt)
        {
            current.otherDataVtable++;
            if (vtable >= base + static_cast<uintptr_t>(DC_RVA_LO) &&
                vtable < base + dataEnd)
            {
                current.moduleDataVtable++;
                size_t bucket = 0;
                while (bucket < vtableCount && vtables[bucket].vtable != vtable)
                    bucket++;
                if (bucket == vtableCount && vtableCount < VTABLE_LIMIT)
                {
                    VtableBucket& entry = vtables[vtableCount++];
                    entry.vtable = vtable;
                    entry.sampleData = data;
                    entry.samplePid = 0;
                    entry.count = 0;
                    snprintf(entry.name, sizeof(entry.name), "?");
                    int32_t pid = 0;
                    read_int(obj + static_cast<uintptr_t>(OBJ_GLOBALID_OFF),
                             &pid);
                    entry.samplePid = pid;
                    const uintptr_t nameStr = scan_call(addr_dataname, data, 0);
                    if (nameStr)
                        read_scstring(nameStr,
                                      SCSTRING_DEFAULT_MAX_LENGTH,
                                      entry.name,
                                      sizeof(entry.name));
                    bucket = vtableCount - 1;
                }
                if (bucket < vtableCount)
                    vtables[bucket].count++;
                else
                    vtableOverflow++;
            }
            else
            {
                current.outOfModuleVtable++;
            }
            continue;
        }
        current.knownProjectileVtable++;

        // LogicProjectileData extends through the reference fields at 0x2c0;
        // capture the full 0x2d0-byte object instead of only the old 0x60..0xc0
        // window.
        uint8_t buf[0x2D0];
        if (!read_bytes(data, buf, sizeof(buf)))
        {
            current.projectileDataReadFail++;
            continue;
        }

        int32_t pid = 0, team = -1, px = 0, py = 0, deadFlag = 0, indirect = 0;
        const bool pidOk =
            read_int(obj + static_cast<uintptr_t>(OBJ_GLOBALID_OFF), &pid);
        const bool teamOk =
            read_int(obj + static_cast<uintptr_t>(OBJ_TEAM_OFF), &team);
        const bool xOk = read_int(obj + static_cast<uintptr_t>(OBJ_X_OFF), &px);
        const bool yOk = read_int(obj + static_cast<uintptr_t>(OBJ_Y_OFF), &py);
        const bool deadOk =
            read_int(obj + static_cast<uintptr_t>(OBJ_DEADFLAG_OFF), &deadFlag);
        const bool indirectOk = read_int(
            obj + static_cast<uintptr_t>(PROJECTILE_ISINDIRECT_OFF), &indirect);
        if (!pidOk || !teamOk || !xOk || !yOk || !deadOk)
            current.projectileFieldReadFail++;
        if (!indirectOk) current.projectileIndirectReadFail++;
        if (pidOk && pid == 0) current.projectileZeroPid++;
        if (teamOk && team == own_team_b) current.projectileOwnTeam++;
        if (deadOk && deadFlag != 0) current.projectileDead++;

        bool withinRange = false;
        if (xOk && yOk)
        {
            const float dx = static_cast<float>(px) - static_cast<float>(own_x);
            const float dy = static_cast<float>(py) - static_cast<float>(own_y);
            withinRange = dx * dx + dy * dy <= zoneSq;
            if (withinRange)
                current.projectileInRange++;
            else
                current.projectileOutsideRange++;
        }

        const bool eligible = pidOk && teamOk && xOk && yOk && deadOk &&
                              indirectOk && pid != 0 && team != own_team_b &&
                              deadFlag == 0 && withinRange;
        if (eligible) current.projectileEligible++;
        const uintptr_t nameStr = scan_call(addr_dataname, data, 0);
        char name[64] = "?";
        if (nameStr)
        {
            read_scstring(
                nameStr, SCSTRING_DEFAULT_MAX_LENGTH, name, sizeof(name));
            struct_scan_string(nameStr, "SCString/projectile-name");
        }

        bool alreadyDetailed = false;
        for (size_t j = 0; j < detailedCount; j++)
            if (detailedData[j] == data)
            {
                alreadyDetailed = true;
                break;
            }
        if (!alreadyDetailed)
        {
            if (detailedCount < DETAIL_LIMIT)
                detailedData[detailedCount++] = data;
            else
                detailOverflow++;

            const int beam =
                static_cast<int>(scan_call(addr_projbeam, data, 0));
            const int early =
                static_cast<int>(scan_call(addr_projearly, data, 0));
            int32_t configuredSpeed = 0, configuredRadius = 0;
            memcpy(&configuredSpeed,
                   buf + static_cast<size_t>(PROJECTILEDATA_SPEED_OFF),
                   sizeof(configuredSpeed));
            memcpy(&configuredRadius,
                   buf + static_cast<size_t>(PROJECTILEDATA_RADIUS_OFF),
                   sizeof(configuredRadius));
            line("[SCANNER] projectile data=%p pid=%d team=%d pos=(%d,%d) "
                 "eligible=%d fields=%d indirect=%d beam=%d early=%d "
                 "speed@%llx=%d radius@%llx=%d name=%s\n",
                 (void*)data,
                 pid,
                 team,
                 px,
                 py,
                 eligible ? 1 : 0,
                 (pidOk && teamOk && xOk && yOk && deadOk && indirectOk) ? 1
                                                                         : 0,
                 indirect,
                 beam,
                 early,
                 static_cast<unsigned long long>(PROJECTILEDATA_SPEED_OFF),
                 configuredSpeed,
                 static_cast<unsigned long long>(PROJECTILEDATA_RADIUS_OFF),
                 configuredRadius,
                 name);
        }
    }

#define SCAN_ACCUMULATE(field) totals.field += current.field
    SCAN_ACCUMULATE(objects);
    SCAN_ACCUMULATE(invalidSnapshot);
    SCAN_ACCUMULATE(nullObject);
    SCAN_ACCUMULATE(dataReadFail);
    SCAN_ACCUMULATE(vtableReadFail);
    SCAN_ACCUMULATE(knownProjectileVtable);
    SCAN_ACCUMULATE(otherDataVtable);
    SCAN_ACCUMULATE(moduleDataVtable);
    SCAN_ACCUMULATE(outOfModuleVtable);
    SCAN_ACCUMULATE(projectileDataReadFail);
    SCAN_ACCUMULATE(projectileFieldReadFail);
    SCAN_ACCUMULATE(projectileIndirectReadFail);
    SCAN_ACCUMULATE(projectileZeroPid);
    SCAN_ACCUMULATE(projectileOwnTeam);
    SCAN_ACCUMULATE(projectileDead);
    SCAN_ACCUMULATE(projectileOutsideRange);
    SCAN_ACCUMULATE(projectileInRange);
    SCAN_ACCUMULATE(projectileEligible);
#undef SCAN_ACCUMULATE

    const uint64_t now = static_cast<uint64_t>(g_get_real_time() / 1000);
    if (lastReportMs && now - lastReportMs < 1000) return;
    lastReportMs = now;
    line("[SCANNER] summary objs=%u invalidSnap=%u nullObj=%u noData=%u "
         "badVtable=%u projVtable=%u otherVtable=%u dataVtableInImage=%u "
         "otherVtableOutsideImage=%u projDataUnreadable=%u projFieldsFail=%u "
         "indirectReadFail=%u pidZero=%u ownTeam=%u dead=%u "
         "outsideRange=%u inRange=%u eligible=%u detailOverflow=%u "
         "vtableOverflow=%u\n",
         totals.objects,
         totals.invalidSnapshot,
         totals.nullObject,
         totals.dataReadFail,
         totals.vtableReadFail,
         totals.knownProjectileVtable,
         totals.otherDataVtable,
         totals.moduleDataVtable,
         totals.outOfModuleVtable,
         totals.projectileDataReadFail,
         totals.projectileFieldReadFail,
         totals.projectileIndirectReadFail,
         totals.projectileZeroPid,
         totals.projectileOwnTeam,
         totals.projectileDead,
         totals.projectileOutsideRange,
         totals.projectileInRange,
         totals.projectileEligible,
         detailOverflow,
         vtableOverflow);
    for (size_t i = 0; i < vtableCount; i++)
        line("[SCANNER] other-data-vtable rva=0x%llx count=%u sample=%p pid=%d "
             "name=%s\n",
             static_cast<unsigned long long>(vtables[i].vtable - base),
             vtables[i].count,
             (void*)vtables[i].sampleData,
             vtables[i].samplePid,
             vtables[i].name);
    totals = {};
    vtableCount = 0;
    vtableOverflow = 0;
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
        scan_projectile_data(true);
        scan_required_structures(true);
        last_hp = -1;
        last_tx = INT32_MAX;
        last_ty = INT32_MAX;
        dodge_count = 0;
        hit_count = 0;
        resetVelocityTracks();
    }
    line("\n--- t=%llu ---\n", ticks.load(std::memory_order_relaxed));
    dump_ad();
    scan_projectile_data(false);
    scan_required_structures(false);
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
        dispatch_time(DISPATCH_TIME_NOW,
                      static_cast<int64_t>(150 * NSEC_PER_MSEC)),
        static_cast<uint64_t>(LOG_INTERVAL_MS * NSEC_PER_MSEC),
        static_cast<uint64_t>(10 * NSEC_PER_MSEC));
    dispatch_source_set_event_handler(timer, ^{ tick(); });
    dispatch_resume(timer);
}

}

void struct_scan_capture_input_message(uintptr_t message)
{
    if (!message || !pointer_plausible(message)) return;
    StructScanPendingInput pending{};
    if (!read_bytes(message, pending.message, sizeof(pending.message))) return;
    pending.address = message;
    void* skillRaw = nullptr;
    if (read_ptr(message + static_cast<uintptr_t>(OFF_CLIENTINPUT_SKILLDATA),
                 &skillRaw) &&
        skillRaw && pointer_plausible(reinterpret_cast<uintptr_t>(skillRaw)) &&
        read_bytes(reinterpret_cast<uintptr_t>(skillRaw),
                   pending.skillData,
                   sizeof(pending.skillData)))
    {
        pending.skillAddress = reinterpret_cast<uintptr_t>(skillRaw);
        pending.hasSkillData = true;
    }

    os_unfair_lock_lock(&structScanLock);
    if (structScanInputMessageCount < STRUCT_SCAN_MAX_PENDING_INPUTS)
    {
        structScanPendingInputs[structScanInputMessageCount++] = pending;
    }
    os_unfair_lock_unlock(&structScanLock);
}

void autododge_scanner_log(uint64_t now, const char* format, ...)
{
    if (!format) return;
    if (!file && !open_log()) return;
    line("\n[SCANNER] t=%llu ", static_cast<unsigned long long>(now));
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fflush(file);
}

__attribute__((constructor)) static void logger_entry()
{
    dispatch_async(dispatch_get_main_queue(), ^{ start(); });
}
