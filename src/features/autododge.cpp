#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

#include "autododge.hpp"
#include "../core/commands.hpp"
#include "../core/functions.hpp"
#include "../core/offsets.hpp"
#include "../core/scanner.hpp"
#include "../utils/walls.hpp"

namespace
{
    constexpr float DEFAULT_FIT_GROW = 0.5f;
    constexpr float DEFAULT_FIT_PAD = 80.0f;
    constexpr int32_t MOVE_INPUT_TYPE = 2;
    constexpr float RANGE_FALLBACK = 2800.0f;
    constexpr int32_t MAX_DIRECTIONS = 128;
    constexpr int32_t MAX_HAZARDS = 48;

    struct Tune
    {
        float AWARE = 3200.0f;
        uint32_t TICK_MS = 4;
        uint32_t TICK_MS_IDLE = 48;
        int32_t DIR_COUNT = 48;
        float SKIN = 50.0f;
        uint32_t LOCK_MS = 70;
        float REACH = 600.0f;
        float HORIZON_S = 1.4f;
        float KEEP_BAND = 100.0f;
        float MOMENTUM = 55.0f;
        float ENGAGE = 100.0f;
        uint32_t RELEASE_GRACE_MS = 200;
        float WALL_HIT = 9000.0f;
        int32_t PROBE_N = 3;
        float PROBE_T = 0.35f;
        float WALL_BODY = 240.0f;
        uint32_t SEND_MIN_MS = 80;
        float SEND_MIN_DIST = 8.0f;
        float MIN_POS = 50.0f;
    };

    struct Direction
    {
        float x;
        float y;
    };

    struct Heading
    {
        bool valid;
        float x;
        float y;
    };

    struct Hazard
    {
        float x;
        float y;
        float vx;
        float vy;
        float rad;
        float pathLen;
        float left;
    };

    struct MoveInput
    {
        int32_t x;
        int32_t y;
    };

    Tune g_tune;
    autododge::Options g_options;

    Direction g_ring[MAX_DIRECTIONS];
    int32_t g_dirCount = 0;

    Hazard g_hazards[MAX_HAZARDS];

    Heading g_heading = {false, 0.0f, 0.0f};
    int32_t g_headingIdx = -1;
    uint32_t g_holdUntil = 0;
    uint32_t g_lastDangerTs = 0;
    uint32_t g_lastTick = 0;
    float g_lastSentX = 1e9f;
    float g_lastSentY = 1e9f;
    uint32_t g_lastSentAt = 0;
    uint32_t g_seenRespawns = 0;

    bool timeBefore(uint32_t a, uint32_t b)
    {
        return (int32_t)(a - b) < 0;
    }

    void buildRing()
    {
        g_dirCount = g_tune.DIR_COUNT;
        for (int32_t i = 0; i < g_dirCount; i++)
        {
            const float angle = 6.283185307179586f * (float)i / (float)g_dirCount;
            g_ring[i].x = cosf(angle);
            g_ring[i].y = sinf(angle);
        }
        g_headingIdx = -1;
    }

    void syncTuning()
    {
        const float t = fmaxf(0.0f, fminf(100.0f, g_options.reactionSpeed)) / 100.0f;
        g_tune.TICK_MS = (uint32_t)fmaxf(4.0f, roundf(24.0f - t * 20.0f));
        g_tune.TICK_MS_IDLE = (uint32_t)fmaxf(16.0f, roundf(64.0f - t * 16.0f));
        g_tune.LOCK_MS = (uint32_t)fmaxf(70.0f, roundf(240.0f - t * 180.0f));

        int32_t count = g_options.directionPrecision;
        if (count < 8) count = 8;
        if (count > 128) count = 128;
        g_tune.DIR_COUNT = count;
        buildRing();

        g_tune.SKIN = fmaxf(0.0f, fminf(120.0f, (float)g_options.safetyMargin)) * 2.0f;
    }

    float ballRadius(const scanner::Projectile &shot)
    {
        return shot.radius > 0 ? (float)shot.radius : 100.0f;
    }

    float traveled(const scanner::Projectile &shot)
    {
        const float dx = (float)(shot.x - shot.spawnX);
        const float dy = (float)(shot.y - shot.spawnY);
        return sqrtf(dx * dx + dy * dy);
    }

    float lifeLeft(const scanner::Projectile &shot, float flown)
    {
        float range = (float)shot.castRange;
        if (range <= 0.0f || range >= 70000.0f) range = RANGE_FALLBACK;
        const float left = range - flown;
        return left > 0.0f ? left : 0.0f;
    }

    bool isIgnored(const char *name)
    {
        if (name == nullptr || name[0] == 0) return false;
        if (g_options.ignoredCount <= 0) return false;

        char upperName[24];
        for (int32_t i = 0; i < 23; i++)
        {
            char c = name[i];
            if (c == 0)
            {
                upperName[i] = 0;
                break;
            }
            if (c >= 'a' && c <= 'z') upperName[i] = c - ('a' - 'A');
            else upperName[i] = c;
            upperName[i+1] = 0;
        }

        for (int32_t i = 0; i < g_options.ignoredCount; i++)
        {
            if (strcmp(upperName, g_options.ignoredBrawlers[i]) == 0) return true;
        }
        return false;
    }

    int32_t collectLive(float myX, float myY, float bodyR)
    {
        int32_t count = 0;

        const scanner::ScannerData &scan = scanner::data();
        if (scan.projectileCount <= 0) return 0;

        const float zoneSq = g_tune.AWARE * g_tune.AWARE;

        for (int32_t i = 0; i < scan.projectileCount; i++)
        {
            const scanner::Projectile &shot = scan.projectiles[i];
            if (shot.gid == 0) continue;
            if (scanner::isFriendlyOwner(shot.ownerIndex, shot.ownerTeam)) continue;
            if (isIgnored(shot.ownerName)) continue;

            const float dx = (float)shot.x - myX;
            const float dy = (float)shot.y - myY;
            if (dx * dx + dy * dy > zoneSq) continue;

            const bool indirect = shot.isIndirect;
            float vx = shot.vx;
            float vy = shot.vy;
            const float flown = traveled(shot);

            float spd = sqrtf(vx * vx + vy * vy);
            const bool blob = !shot.isBeam && !indirect && shot.speed > 0 && shot.speed < 1600;

            const float shotR = ballRadius(shot);
            const float rad = shotR + bodyR + DEFAULT_FIT_PAD + shotR * DEFAULT_FIT_GROW;

            float ux = 0.0f;
            float uy = 0.0f;
            if (spd >= 1.0f)
            {
                ux = vx / spd;
                uy = vy / spd;
            }

            const bool skipAlong = blob || indirect;
            float playerAlong = 0.0f;
            if (!skipAlong && spd >= 1.0f)
            {
                playerAlong = (myX - (float)shot.x) * ux + (myY - (float)shot.y) * uy;
                if (playerAlong < -50.0f) continue;

                const float along = playerAlong > 0.0f ? playerAlong : 0.0f;
                const float hitX = (float)shot.x + ux * along;
                const float hitY = (float)shot.y + uy * along;
                if (!walls::losCheck((float)shot.x, (float)shot.y, hitX, hitY, walls::BLOCKS_PROJECTILES)) continue;
            }

            const float left = lifeLeft(shot, flown);
            if (left <= 10.0f) continue;

            const float gap = sqrtf(dx * dx + dy * dy) - bodyR - shotR;
            const float gapFloor = gap > 0.0f ? gap : 0.0f;
            if (left < 0.85f * gapFloor) continue;
            if (!skipAlong && spd >= 1.0f && playerAlong > left + shotR) continue;

            if (count >= MAX_HAZARDS) break;

            Hazard &hazard = g_hazards[count++];
            hazard.x = (float)shot.x;
            hazard.y = (float)shot.y;
            hazard.vx = blob ? 0.0f : vx;
            hazard.vy = blob ? 0.0f : vy;
            hazard.rad = rad;
            hazard.pathLen = left + shotR;
            hazard.left = left;
        }
        return count;
    }

    float clearanceFor(const Hazard *hazards, int32_t count, float myX, float myY, float mvx, float mvy)
    {
        float minClear = 1e30f;
        const float horizon = g_tune.HORIZON_S;

        for (int32_t i = 0; i < count; i++)
        {
            const Hazard &threat = hazards[i];
            const float spd = sqrtf(threat.vx * threat.vx + threat.vy * threat.vy);

            float maxT = horizon;
            if (spd > 1.0f)
            {
                if (threat.left > 0.0f)
                {
                    const float bound = threat.left / spd;
                    if (bound < maxT) maxT = bound;
                }
                if (threat.pathLen > 0.0f)
                {
                    const float bound = threat.pathLen / spd;
                    if (bound < maxT) maxT = bound;
                }
            }

            const float relx = threat.x - myX;
            const float rely = threat.y - myY;
            const float vrx = threat.vx - mvx;
            const float vry = threat.vy - mvy;
            const float vv = vrx * vrx + vry * vry;

            float cx = relx;
            float cy = rely;
            if (vv >= 1e-6f)
            {
                float ts = -(relx * vrx + rely * vry) / vv;
                if (ts < 0.0f) ts = 0.0f;
                else if (ts > maxT) ts = maxT;
                cx = relx + vrx * ts;
                cy = rely + vry * ts;
            }

            const float clear = sqrtf(cx * cx + cy * cy) - threat.rad;
            if (clear < minClear) minClear = clear;
        }
        return minClear;
    }

    float wallAhead(float myX, float myY, const Direction &dir, float speed, float bodyR)
    {
        const float step = speed * g_tune.PROBE_T;
        float raw = 0.0f;

        for (int32_t s = 1; s <= g_tune.PROBE_N; s++)
        {
            const float px = myX + dir.x * step * (float)s;
            const float py = myY + dir.y * step * (float)s;
            if (walls::isBlockedWide(px, py, bodyR, walls::BLOCKS_MOVEMENT))
            {
                raw += g_tune.WALL_HIT * (float)(g_tune.PROBE_N - s + 1);
            }
        }
        return raw;
    }

    float clampToMap(float value, int32_t maxTiles)
    {
        const float limit = (float)maxTiles * walls::TILE_SIZE - 1.0f;
        if (limit <= 0.0f) return value;
        if (value < 0.0f) return 0.0f;
        if (value > limit) return limit;
        return value;
    }

    void writeMoveInput(uintptr_t clientInput, uintptr_t, void *ctx)
    {
        const MoveInput *input = static_cast<const MoveInput *>(ctx);
        *(int32_t *)(clientInput + offsets::ClientInput_x) = input->x;
        *(int32_t *)(clientInput + offsets::ClientInput_y) = input->y;
    }

    bool sendMove(uintptr_t logic, int32_t tx, int32_t ty)
    {
        if (logic == 0) return false;

        if (functions::LogicBattleModeClient_setClientPredictionMoveTo != nullptr)
        {
            functions::LogicBattleModeClient_setClientPredictionMoveTo(logic, tx, ty, 1);
        }

        MoveInput input;
        input.x = tx;
        input.y = ty;
        return sendCommand(MOVE_INPUT_TYPE, writeMoveInput, &input);
    }

    void clearHeading()
    {
        g_heading.valid = false;
        g_headingIdx = -1;
        g_holdUntil = 0;
    }
}

void autododge::setup()
{
    syncTuning();
    scanner::enableProjectileTracking();
}

void autododge::reset()
{
    clearHeading();
    g_lastDangerTs = 0;
    g_lastTick = 0;
    g_lastSentX = 1e9f;
    g_lastSentY = 1e9f;
    g_lastSentAt = 0;
    g_seenRespawns = (uint32_t)scanner::data().myRespawns;
}

void autododge::configure(const Options &options)
{
    float reactionSpeed = options.reactionSpeed;
    if (reactionSpeed < 0.0f) reactionSpeed = 0.0f;
    if (reactionSpeed > 100.0f) reactionSpeed = 100.0f;
    g_options.reactionSpeed = reactionSpeed;

    int32_t precision = options.directionPrecision;
    if (precision < 8) precision = 8;
    if (precision > 128) precision = 128;
    g_options.directionPrecision = precision;

    int32_t margin = options.safetyMargin;
    if (margin < 0) margin = 0;
    if (margin > 120) margin = 120;
    g_options.safetyMargin = margin;

    int32_t count = options.ignoredCount;
    if (count < 0) count = 0;
    if (count > 16) count = 16;
    g_options.ignoredCount = count;

    for (int32_t i = 0; i < count; i++)
    {
        strncpy(g_options.ignoredBrawlers[i], options.ignoredBrawlers[i], 23);
        g_options.ignoredBrawlers[i][23] = 0;
    }

    syncTuning();
}

bool autododge::dodgeDir(float &x, float &y)
{
    if (!g_heading.valid) return false;
    x = g_heading.x;
    y = g_heading.y;
    return true;
}

bool autododge::clampTarget(float tx, float ty, float &outX, float &outY)
{
    const int32_t width = walls::getWallCacheW();
    const int32_t height = walls::getWallCacheH();

    outX = tx;
    outY = ty;
    if (width <= 0 || height <= 0) return false;

    outX = clampToMap(tx, width);
    outY = clampToMap(ty, height);
    return true;
}

bool autododge::sendBattleMove(uintptr_t logic, float tx, float ty)
{
    return sendMove(logic, (int32_t)lroundf(tx), (int32_t)lroundf(ty));
}

void autododge::update(uint32_t nowMs)
{
    const scanner::ScannerData &scan = scanner::data();
    if (scan.lastUpdate == 0) return;

    if (scan.myRespawns != g_seenRespawns)
    {
        g_seenRespawns = scan.myRespawns;
        clearHeading();
        g_lastSentX = 1e9f;
        g_lastSentY = 1e9f;
        g_lastSentAt = 0;
        g_lastTick = 0;
        g_lastDangerTs = 0;
    }

    if (!scan.myAlive)
    {
        clearHeading();
        return;
    }

    const uint32_t tickMs = g_heading.valid ? g_tune.TICK_MS : g_tune.TICK_MS_IDLE;
    if ((uint32_t)(nowMs - g_lastTick) < tickMs) return;
    g_lastTick = nowMs;

    const float myX = (float)scan.myX;
    const float myY = (float)scan.myY;
    if (fabsf(myX) < g_tune.MIN_POS && fabsf(myY) < g_tune.MIN_POS) return;

    const float speed = scan.mySpeed > 0 ? (float)scan.mySpeed : 720.0f;
    const float myRadius = scan.myRadius > 0 ? (float)scan.myRadius : 60.0f;
    const float bodyRadius = myRadius + g_tune.SKIN;

    if (g_dirCount <= 0) return;

    const int32_t hazardCount = collectLive(myX, myY, bodyRadius);

    if (hazardCount == 0)
    {
        if (g_heading.valid && (uint32_t)(nowMs - g_lastDangerTs) > g_tune.RELEASE_GRACE_MS) clearHeading();
        return;
    }

    const float stayClear = clearanceFor(g_hazards, hazardCount, myX, myY, 0.0f, 0.0f);
    const bool inDanger = stayClear < g_tune.ENGAGE;
    if (inDanger) g_lastDangerTs = nowMs;

    if (!inDanger && (!g_heading.valid || (uint32_t)(nowMs - g_lastDangerTs) > g_tune.RELEASE_GRACE_MS))
    {
        if (g_heading.valid) clearHeading();
        return;
    }

    const int32_t prevIdx = (g_headingIdx >= 0 && g_headingIdx < g_dirCount) ? g_headingIdx : -1;
    const Direction *prevDir = prevIdx >= 0 ? &g_ring[prevIdx] : nullptr;

    int32_t bestIdx = 0;
    float bestScore = -1e30f;
    float prevScore = -1e30f;

    for (int32_t i = 0; i < g_dirCount; i++)
    {
        const Direction &dir = g_ring[i];
        float score = clearanceFor(g_hazards, hazardCount, myX, myY, speed * dir.x, speed * dir.y);
        score -= wallAhead(myX, myY, dir, speed, g_tune.WALL_BODY);
        if (prevDir != nullptr) score += g_tune.MOMENTUM * (dir.x * prevDir->x + dir.y * prevDir->y);

        if (i == prevIdx) prevScore = score;
        if (score > bestScore)
        {
            bestScore = score;
            bestIdx = i;
        }
    }

    int32_t chosenIdx = bestIdx;
    float chosenScore = bestScore;

    if (prevIdx >= 0 && timeBefore(nowMs, g_holdUntil) && chosenIdx != prevIdx)
    {
        if (prevScore + g_tune.KEEP_BAND >= bestScore)
        {
            chosenIdx = prevIdx;
            chosenScore = prevScore;
        }
        else
        {
            g_holdUntil = nowMs + g_tune.LOCK_MS;
        }
    }
    else if (!timeBefore(nowMs, g_holdUntil))
    {
        g_holdUntil = nowMs + g_tune.LOCK_MS;
    }

    if (chosenScore <= stayClear)
    {
        if (g_heading.valid) clearHeading();
        return;
    }

    const Direction &chosen = g_ring[chosenIdx];
    g_headingIdx = chosenIdx;
    g_heading.valid = true;
    g_heading.x = chosen.x;
    g_heading.y = chosen.y;

    const float targetX = roundf(myX + chosen.x * g_tune.REACH);
    const float targetY = roundf(myY + chosen.y * g_tune.REACH);

    const float ddx = targetX - g_lastSentX;
    const float ddy = targetY - g_lastSentY;
    const bool moved = ddx * ddx + ddy * ddy > g_tune.SEND_MIN_DIST * g_tune.SEND_MIN_DIST;
    const bool stale = (uint32_t)(nowMs - g_lastSentAt) > g_tune.SEND_MIN_MS;
    if (!moved && !stale) return;

    g_lastSentX = targetX;
    g_lastSentY = targetY;
    g_lastSentAt = nowMs;
    sendMove(scan.battleModeClient, (int32_t)lroundf(targetX), (int32_t)lroundf(targetY));
}
