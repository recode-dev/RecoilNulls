#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <frida-gum.h>

#include "aimbot.hpp"
#include "../core/offsets.hpp"
#include "../core/functions.hpp"
#include "../core/scanner.hpp"
#include "../utils/walls.hpp"

namespace
{
    constexpr float SPEED_FALLBACK = 3000.0f;
    constexpr uint32_t SPEED_REFRESH_MS = 500;
    constexpr float LATENCY_S = 0.06f;
    constexpr int32_t LEAD_ITERATIONS = 3;
    constexpr float RANGE_FLIGHT_S = 1.1f;
    constexpr float RANGE_MARGIN = 0.88f;
    constexpr float RANGE_UNIT = 300.0f;
    constexpr float SPEED_CAP_MUL = 1.35f;
    constexpr float VEL_SMOOTH = 0.6f;
    constexpr float LEAD_CLAMP = 0.35f;
    constexpr float LEAD_RAMP_DIST = 1200.0f;
    constexpr float SPEED_DEFAULT_CAP = 1200.0f;
    constexpr float SPEED_SPIKE_MUL = 1.6f;
    constexpr float STICKY_MARGIN = 1.12f;
    constexpr float AIM_JITTER = 40.0f;
    constexpr int32_t HIST_SIZE = 8;
    constexpr int32_t TRACK_SLOTS = 32;

    struct Predicted
    {
        float x;
        float y;
        float vx;
        float vy;
        bool juke;
    };

    struct TrackEntry
    {
        uint32_t gid;
        uint32_t t;
        float x;
        float y;
        float vx;
        float vy;
        float histX[HIST_SIZE];
        float histY[HIST_SIZE];
        uint32_t histT[HIST_SIZE];
        int32_t histCount;
        bool used;
    };

    TrackEntry g_track[TRACK_SLOTS];
    scanner::Enemy g_target;
    bool g_hasTarget = false;
    uintptr_t g_targetPtr = 0;
    float g_projectileSpeed = SPEED_FALLBACK;
    uint32_t g_speedAt = 0;
    bool g_ownBeam = false;
    float g_rangeCache = 0.0f;
    uint32_t g_rangeAt = 0;
    bool g_hooks = false;
    bool g_enabled = false;
    uint8_t g_pathBuffer[0x40];
    uint32_t g_randomState = 0x9e3779b9u;

    bool readBytes(uintptr_t address, void *out, size_t size)
    {
        if (address == 0 || out == nullptr || size == 0) return false;
        if (!gum_memory_is_readable((gconstpointer)address, (gsize)size)) return false;
        memcpy(out, (const void *)address, size);
        return true;
    }

    bool readU32(uintptr_t address, uint32_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    bool readPtr(uintptr_t address, uintptr_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    bool readFloat(uintptr_t address, float &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    uint32_t nowMs()
    {
        return (uint32_t)(g_get_monotonic_time() / 1000);
    }

    float randomUnit()
    {
        g_randomState ^= g_randomState << 13;
        g_randomState ^= g_randomState >> 17;
        g_randomState ^= g_randomState << 5;
        return (float)(g_randomState & 0xffff) / 65535.0f;
    }

    bool isSkipped(const char *name)
    {
        constexpr uint8_t TAG[4] = {0x18, 0x1b, 0x16, 0x16};
        if (name == nullptr || name[0] == 0) return true;

        for (const char *p = name; *p != 0; p++)
        {
            size_t i = 0;
            while (p[i] != 0 && i < 4 && (uint8_t)p[i] == (uint8_t)(TAG[i] ^ 0x5a)) i++;
            if (i == 4) return true;
        }
        return false;
    }

    int32_t roundToInt(float value)
    {
        return (int32_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
    }

    TrackEntry *findEntry(uint32_t gid)
    {
        for (int32_t i = 0; i < TRACK_SLOTS; i++)
        {
            if (g_track[i].used && g_track[i].gid == gid) return &g_track[i];
        }
        return nullptr;
    }

    TrackEntry *allocateEntry(uint32_t gid)
    {
        for (int32_t i = 0; i < TRACK_SLOTS; i++)
        {
            if (!g_track[i].used)
            {
                memset(&g_track[i], 0, sizeof(g_track[i]));
                g_track[i].used = true;
                g_track[i].gid = gid;
                return &g_track[i];
            }
        }

        TrackEntry *oldest = &g_track[0];
        for (int32_t i = 1; i < TRACK_SLOTS; i++)
        {
            if (g_track[i].t < oldest->t) oldest = &g_track[i];
        }
        memset(oldest, 0, sizeof(*oldest));
        oldest->used = true;
        oldest->gid = gid;
        return oldest;
    }

    void pushHistory(TrackEntry *entry, float x, float y, uint32_t now)
    {
        if (entry->histCount < HIST_SIZE)
        {
            const int32_t slot = entry->histCount;
            entry->histX[slot] = x;
            entry->histY[slot] = y;
            entry->histT[slot] = now;
            entry->histCount++;
            return;
        }

        for (int32_t i = 1; i < HIST_SIZE; i++)
        {
            entry->histX[i - 1] = entry->histX[i];
            entry->histY[i - 1] = entry->histY[i];
            entry->histT[i - 1] = entry->histT[i];
        }
        entry->histX[HIST_SIZE - 1] = x;
        entry->histY[HIST_SIZE - 1] = y;
        entry->histT[HIST_SIZE - 1] = now;
    }

    Predicted sample(uint32_t gid, float x, float y, int32_t moveSpeed, uint32_t now)
    {
        Predicted out = { x, y, 0.0f, 0.0f, false };
        TrackEntry *entry = findEntry(gid);
        const bool had = entry != nullptr;
        TrackEntry previous = {};
        if (had) previous = *entry;

        const float cap = moveSpeed > 0 ? (float)moveSpeed * SPEED_CAP_MUL : SPEED_DEFAULT_CAP;

        float vx = 0.0f;
        float vy = 0.0f;
        if (had)
        {
            const float dt = (float)(now - previous.t) / 1000.0f;
            if (dt >= 0.008f && dt <= 0.4f)
            {
                const float ivx = (x - previous.x) / dt;
                const float ivy = (y - previous.y) / dt;
                const float im = sqrtf(ivx * ivx + ivy * ivy);
                if (im > cap * SPEED_SPIKE_MUL)
                {
                    vx = previous.vx;
                    vy = previous.vy;
                }
                else
                {
                    vx = ivx * VEL_SMOOTH + previous.vx * (1.0f - VEL_SMOOTH);
                    vy = ivy * VEL_SMOOTH + previous.vy * (1.0f - VEL_SMOOTH);
                }
            }
            else
            {
                vx = previous.vx;
                vy = previous.vy;
            }
        }

        const float mag = sqrtf(vx * vx + vy * vy);
        if (mag > cap)
        {
            vx = vx / mag * cap;
            vy = vy / mag * cap;
        }

        if (!had) entry = allocateEntry(gid);

        entry->x = x;
        entry->y = y;
        entry->t = now;
        entry->vx = vx;
        entry->vy = vy;
        pushHistory(entry, x, y, now);

        bool juke = false;
        if (entry->histCount >= 4)
        {
            const uint32_t firstT = entry->histT[0];
            const float span = (float)(now - firstT) / 1000.0f;
            if (span > 0.03f)
            {
                const float mvx = (x - entry->histX[0]) / span;
                const float mvy = (y - entry->histY[0]) / span;
                const float meanMag = sqrtf(mvx * mvx + mvy * mvy);
                juke = mag > 1.5f * (meanMag + 40.0f);
            }
        }

        if (juke)
        {
            float bx = 0.0f;
            float by = 0.0f;
            for (int32_t i = 0; i < entry->histCount; i++)
            {
                bx += entry->histX[i];
                by += entry->histY[i];
            }
            out.x = bx / (float)entry->histCount;
            out.y = by / (float)entry->histCount;
            out.vx = 0.0f;
            out.vy = 0.0f;
            out.juke = true;
            return out;
        }

        out.x = x;
        out.y = y;
        out.vx = vx;
        out.vy = vy;
        return out;
    }

    struct Solution
    {
        int32_t x;
        int32_t y;
        float t;
        bool valid;
    };

    Solution solve(float mx, float my, const Predicted &pred, float speed, float range)
    {
        Solution out = { 0, 0, 0.0f, false };

        if (speed < 1.0f) speed = SPEED_FALLBACK;

        const float cdx = pred.x - mx;
        const float cdy = pred.y - my;
        const float currentDist = sqrtf(cdx * cdx + cdy * cdy);

        if (g_ownBeam)
        {
            if (range > 0.0f && currentDist > range) return out;
            out.x = roundToInt(pred.x);
            out.y = roundToInt(pred.y);
            out.t = LATENCY_S;
            out.valid = true;
            return out;
        }

        const float rampDist = LEAD_RAMP_DIST > 0.0f ? LEAD_RAMP_DIST : 1.0f;
        float ramp = currentDist / rampDist;
        if (ramp > 1.0f) ramp = 1.0f;
        const float maxLead = currentDist * LEAD_CLAMP * ramp;

        float flight = LATENCY_S + currentDist / speed;
        float lx = 0.0f;
        float ly = 0.0f;

        for (int32_t i = 0; i < LEAD_ITERATIONS; i++)
        {
            lx = pred.vx * flight;
            ly = pred.vy * flight;
            const float lm = sqrtf(lx * lx + ly * ly);
            if (lm > maxLead)
            {
                lx *= maxLead / lm;
                ly *= maxLead / lm;
            }
            const float px = pred.x + lx - mx;
            const float py = pred.y + ly - my;
            flight = LATENCY_S + sqrtf(px * px + py * py) / speed;
        }

        const float ax = pred.x + lx - mx;
        const float ay = pred.y + ly - my;
        if (range > 0.0f && sqrtf(ax * ax + ay * ay) > range) return out;

        out.x = roundToInt(pred.x + lx);
        out.y = roundToInt(pred.y + ly);
        out.t = flight;
        out.valid = true;
        return out;
    }

    float ownProjectileSpeed()
    {
        const uint32_t now = nowMs();
        if (now - g_speedAt < SPEED_REFRESH_MS) return g_projectileSpeed;
        g_speedAt = now;

        const scanner::ScannerData &data = scanner::data();
        if (data.ownCharacter == 0) return g_projectileSpeed;
        if (functions::LogicCharacterClient_getWeaponSkill == nullptr) return g_projectileSpeed;
        if (functions::LogicSkillData_getProjectileData == nullptr) return g_projectileSpeed;

        const uintptr_t skill = functions::LogicCharacterClient_getWeaponSkill(data.ownCharacter);
        if (skill == 0) return g_projectileSpeed;

        const uintptr_t projectileData = functions::LogicSkillData_getProjectileData(skill, 0);
        if (projectileData == 0) return g_projectileSpeed;

        uint32_t raw = 0;
        if (readU32(projectileData + offsets::ProjectileData_speed, raw))
        {
            if (raw >= 300 && raw <= 8500) g_projectileSpeed = (float)raw;
        }

        g_ownBeam = functions::LogicProjectileData_isBeam != nullptr &&
            functions::LogicProjectileData_isBeam(projectileData) != 0;

        return g_projectileSpeed;
    }

    float ownRange(float speed)
    {
        const uint32_t now = nowMs();
        const float fallback = speed * RANGE_FLIGHT_S * RANGE_MARGIN;
        if (now - g_rangeAt < SPEED_REFRESH_MS) return g_rangeCache > 0.0f ? g_rangeCache : fallback;
        g_rangeAt = now;

        int32_t tiles = 0;
        const scanner::ScannerData &data = scanner::data();
        if (data.ownCharacter != 0 &&
            functions::LogicCharacterClient_getWeaponSkill != nullptr &&
            functions::LogicSkillData_getCastingRange != nullptr)
        {
            const uintptr_t skill = functions::LogicCharacterClient_getWeaponSkill(data.ownCharacter);
            if (skill != 0) tiles = functions::LogicSkillData_getCastingRange(skill);
        }

        if (tiles > 0 && tiles <= 40) g_rangeCache = (float)tiles * RANGE_UNIT * RANGE_MARGIN;
        else g_rangeCache = 0.0f;

        return g_rangeCache > 0.0f ? g_rangeCache : fallback;
    }

    bool callPath(uintptr_t screen, uintptr_t own, uintptr_t skill, float mx, float my, Solution &aim)
    {
        if (functions::BattleScreen_calculateProjectilePath == nullptr) return false;

        memset(g_pathBuffer, 0, sizeof(g_pathBuffer));
        functions::BattleScreen_calculateProjectilePath(g_pathBuffer, (void *)screen, (void *)own, (void *)skill,
            0, mx, my, 0.0f, (float)aim.x, (float)aim.y, 0.0f);

        uintptr_t begin = 0;
        uintptr_t end = 0;
        if (!readPtr((uintptr_t)g_pathBuffer, begin)) return false;
        if (!readPtr((uintptr_t)g_pathBuffer + sizeof(uintptr_t), end)) return false;
        if (begin == 0 || end == 0 || end <= begin) return false;

        const int64_t span = (int64_t)(end - begin);
        const int32_t count = (int32_t)(span / 12);
        if (count < 2 || count > 64) return false;

        const uintptr_t last = begin + (uintptr_t)((count - 1) * 12);
        float px = 0.0f;
        float py = 0.0f;
        if (!readFloat(last + 4, px)) return false;
        if (!readFloat(last + 8, py)) return false;
        if (!isfinite(px) || !isfinite(py)) return false;
        if (fabsf(px) > 40000.0f || fabsf(py) > 40000.0f) return false;

        aim.x = roundToInt(px);
        aim.y = roundToInt(py);
        return true;
    }

    bool aimAt(uint32_t now, Solution &out)
    {
        if (!g_hasTarget) return false;
        if (g_target.ptr == 0) return false;

        const scanner::ScannerData &data = scanner::data();
        float ex = (float)g_target.x;
        float ey = (float)g_target.y;

        const uintptr_t sampleTarget = g_target.body != 0 ? g_target.body : g_target.ptr;
        int32_t bx = 0;
        int32_t by = 0;
        if (functions::LogicGameObjectClient_getX != nullptr && sampleTarget != 0)
        {
            bx = functions::LogicGameObjectClient_getX(sampleTarget);
            by = functions::LogicGameObjectClient_getY(sampleTarget);
            ex = (float)bx;
            ey = (float)by;
        }

        const float speed = ownProjectileSpeed();
        const Predicted predicted = sample(g_target.gid, ex, ey, g_target.moveSpeed, now);
        Solution aim = solve((float)data.myX, (float)data.myY, predicted, speed, ownRange(speed));
        if (!aim.valid) return false;

        if (data.battleScreen != 0 && data.ownCharacter != 0 &&
            functions::LogicCharacterClient_getWeaponSkill != nullptr)
        {
            const uintptr_t skill = functions::LogicCharacterClient_getWeaponSkill(data.ownCharacter);
            if (skill != 0)
            {
                callPath(data.battleScreen, data.ownCharacter, skill, (float)data.myX, (float)data.myY, aim);
            }
        }

        aim.x += roundToInt((randomUnit() * 2.0f - 1.0f) * AIM_JITTER);
        aim.y += roundToInt((randomUnit() * 2.0f - 1.0f) * AIM_JITTER);
        out = aim;
        return true;
    }

    const scanner::Enemy *pickTarget(uint32_t now)
    {
        const scanner::ScannerData &data = scanner::data();
        const int32_t myTeam = data.myTeamId;
        const float mx = (float)data.myX;
        const float my = (float)data.myY;
        const float speed = ownProjectileSpeed();
        const float range = ownRange(speed);

        const scanner::Enemy *best = nullptr;
        float bestDistance = 1e9f;
        const scanner::Enemy *calm = nullptr;
        float calmDistance = 1e9f;
        const scanner::Enemy *current = nullptr;
        float currentDistance = 1e9f;

        for (int32_t i = 0; i < data.enemyCount; i++)
        {
            const scanner::Enemy &enemy = data.enemies[i];
            if (enemy.ptr == 0) continue;
            if (enemy.isSpawnable) continue;
            if (isSkipped(enemy.brawlerName)) continue;
            if (enemy.teamId == myTeam) continue;
            if (!walls::losCheck(mx, my, (float)enemy.x, (float)enemy.y, walls::BLOCKS_PROJECTILES)) continue;

            const float dx = (float)enemy.x - mx;
            const float dy = (float)enemy.y - my;
            const float distance = sqrtf(dx * dx + dy * dy);
            if (range > 0.0f && distance > range) continue;

            const Predicted predicted = sample(enemy.gid, (float)enemy.x, (float)enemy.y, enemy.moveSpeed, now);
            if (!predicted.juke && distance < calmDistance)
            {
                calmDistance = distance;
                calm = &data.enemies[i];
            }
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &data.enemies[i];
            }
            if (g_hasTarget && enemy.gid == g_target.gid)
            {
                current = &data.enemies[i];
                currentDistance = distance;
            }
        }

        if (calm != nullptr && calmDistance <= bestDistance * 1.4f)
        {
            best = calm;
            bestDistance = calmDistance;
        }

        if (current != nullptr && best != nullptr && currentDistance <= bestDistance * STICKY_MARGIN)
        {
            return current;
        }

        return best;
    }

    void clearTarget()
    {
        g_hasTarget = false;
        g_targetPtr = 0;
        memset(&g_target, 0, sizeof(g_target));
    }

    typedef struct _TargetListener TargetListener;
    typedef struct _TargetListenerClass TargetListenerClass;

    struct _TargetListener
    {
        GObject parent;
    };

    struct _TargetListenerClass
    {
        GObjectClass parent_class;
    };

    static void target_listener_iface_init(gpointer g_iface, gpointer);

    static void target_listener_init(TargetListener *)
    {
    }

    static void target_listener_class_init(TargetListenerClass *)
    {
    }

    static void target_listener_on_leave(GumInvocationListener *, GumInvocationContext *context)
    {
        if (!g_enabled) return;
        if (g_targetPtr == 0) return;
        gum_invocation_context_replace_return_value(context, (gpointer)g_targetPtr);
    }

    static void target_listener_iface_init(gpointer g_iface, gpointer)
    {
        GumInvocationListenerInterface *iface = static_cast<GumInvocationListenerInterface *>(g_iface);
        iface->on_leave = target_listener_on_leave;
    }

    G_DEFINE_TYPE_EXTENDED(TargetListener, target_listener, G_TYPE_OBJECT, 0,
        G_IMPLEMENT_INTERFACE(GUM_TYPE_INVOCATION_LISTENER, target_listener_iface_init))

    typedef struct _SkillListener SkillListener;
    typedef struct _SkillListenerClass SkillListenerClass;

    struct _SkillListener
    {
        GObject parent;
    };

    struct _SkillListenerClass
    {
        GObjectClass parent_class;
    };

    static void skill_listener_iface_init(gpointer g_iface, gpointer);

    static void skill_listener_init(SkillListener *)
    {
    }

    static void skill_listener_class_init(SkillListenerClass *)
    {
    }

    static void skill_listener_on_enter(GumInvocationListener *, GumInvocationContext *context)
    {
        if (!g_enabled) return;
        if (g_targetPtr == 0) return;

        Solution aim = {};
        if (!aimAt(nowMs(), aim)) return;

        GumCpuContext *cpu = context->cpu_context;
        if (cpu == nullptr) return;
        cpu->x[1] = (guint64)aim.x;
        cpu->x[2] = (guint64)aim.y;
    }

    static void skill_listener_iface_init(gpointer g_iface, gpointer)
    {
        GumInvocationListenerInterface *iface = static_cast<GumInvocationListenerInterface *>(g_iface);
        iface->on_enter = skill_listener_on_enter;
    }

    G_DEFINE_TYPE_EXTENDED(SkillListener, skill_listener, G_TYPE_OBJECT, 0,
        G_IMPLEMENT_INTERFACE(GUM_TYPE_INVOCATION_LISTENER, skill_listener_iface_init))
}

void aimbot::setupAimbot(uintptr_t baseAddress)
{
    if (g_hooks) return;
    if (baseAddress == 0) return;

    GumInterceptor *interceptor = gum_interceptor_obtain();

    TargetListener *targetListener = reinterpret_cast<TargetListener *>(g_object_new(target_listener_get_type(), NULL));
    (void)gum_interceptor_attach(interceptor,
        GSIZE_TO_POINTER(baseAddress + offsets::BattleScreen_getClosestTargetForAutoshoot),
        GUM_INVOCATION_LISTENER(targetListener),
        NULL);

    SkillListener *skillListener = reinterpret_cast<SkillListener *>(g_object_new(skill_listener_get_type(), NULL));
    (void)gum_interceptor_attach(interceptor,
        GSIZE_TO_POINTER(baseAddress + offsets::BattleScreen_activateSkill),
        GUM_INVOCATION_LISTENER(skillListener),
        NULL);

    g_hooks = true;
}

void aimbot::updateAimbot(bool enabled)
{
    g_enabled = enabled;
    if (!enabled)
    {
        clearTarget();
        return;
    }

    const scanner::ScannerData &data = scanner::data();
    if (data.lastUpdate == 0)
    {
        clearTarget();
        return;
    }

    const scanner::Enemy *pick = pickTarget(nowMs());
    if (pick == nullptr)
    {
        clearTarget();
        return;
    }

    g_target = *pick;
    g_hasTarget = true;
    g_targetPtr = pick->ptr;
}

void aimbot::resetAimbot()
{
    clearTarget();
    memset(g_track, 0, sizeof(g_track));
}

bool aimbot::hasTarget()
{
    return g_hasTarget && g_targetPtr != 0;
}
