#include "../core/commands.h"
#include "../core/scan.h"
#include "../core/scstring.h"
#define PROJ_DATA_BLOCK_SIZE (PROJECTILEDATA_RADIUS_OFF + sizeof(int32_t))
#include "../utils/walls.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <cstdint>
#include <vector>

static struct
{
    float AWARE = 4400.0f;
    float REACT_S = 0.35f;
    int DIR_COUNT = 16;
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

static const float SAFE_GAP = 35.0f;

struct Threat { float x, y, vx, vy, rad; };
struct Body { float x, y, rad; };

struct ProjectileState
{
    float vx, vy, speed, radius;
    char name[128];
    float spawnX, spawnY;
    int earlyTicks;
    uint64_t spawnedAt, lastSeen, lastTs;
    int32_t lastX, lastY;
    bool isIndirect, isBeam, hasVelocity;
};

__thread int in_move = 0;
int active = 0, moving = 0;
int32_t ad_target_x = INT32_MAX, ad_target_y = INT32_MAX;

static std::map<int, ProjectileState> proj_cache;
static std::vector<float> ringX, ringY;
static std::vector<Body> bodies;
static int headingIdx = -1;
static uint64_t lastDangerTs = 0, lastGoodTs = 0, lastSentAt = 0;
static float commitDirX = 0.0f, commitDirY = 0.0f;
static float CHAR_SPEED = 720.0f;

static void buildRing()
{
    if ((int)ringX.size() == TUNE.DIR_COUNT) return;
    ringX.clear();
    ringY.clear();
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        float a = (float)M_PI * 2.0f * (float)i / (float)TUNE.DIR_COUNT;
        ringX.push_back(cosf(a));
        ringY.push_back(sinf(a));
    }
    headingIdx = -1;
}

static float clearanceFor(const std::vector<Threat>& threats, const std::vector<Body>& bodies, float mx, float my, float mvx, float mvy)
{
    float minClear = 1e18f;
    for (const auto& t : threats)
    {
        float relx = t.x - mx;
        float rely = t.y - my;
        float vrx = t.vx - mvx;
        float vry = t.vy - mvy;
        float vv = vrx * vrx + vry * vry;
        float ts = vv > 1e-6f ? -(relx * vrx + rely * vry) / vv : 0.0f;
        if (ts < 0.0f) ts = 0.0f;
        else if (ts > TUNE.HORIZON_S) ts = TUNE.HORIZON_S;
        float cx = relx + vrx * ts;
        float cy = rely + vry * ts;
        float clear = sqrtf(cx * cx + cy * cy) - t.rad;
        if (clear < minClear) minClear = clear;
    }
    for (const auto& b : bodies)
    {
        float dx = b.x - mx;
        float dy = b.y - my;
        float clear = sqrtf(dx * dx + dy * dy) - (b.rad + 120.0f);
        if (clear < minClear) minClear = clear;
    }
    return minClear;
}

static float wallAhead(float mx, float my, float dirX, float dirY, float speed, float bodyR)
{
    if (!wall_ready()) return 0.0f;
    float step = speed * TUNE.PROBE_T;
    int blocked = 0;
    for (int s = 1; s <= TUNE.PROBE_N; s++)
    {
        if (wall_is_blocked_wide(mx + dirX * step * s, my + dirY * step * s, bodyR, WALL_BLOCKS_MOVEMENT))
            blocked++;
    }
    return TUNE.WALL_HIT * (float)blocked / (float)TUNE.PROBE_N;
}

static int32_t clamp_axis(float value, int tiles)
{
    if (tiles <= 0) return (int32_t)roundf(value);
    float limit = (float)tiles * WALL_TILE_SIZE - 1.0f;
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

static void fillDangerMap(float* danger, const std::vector<Threat>& threats, const std::vector<Body>& bodies, float mx, float my, float bodyR)
{
    for (int i = 0; i < TUNE.DIR_COUNT; i++) danger[i] = 0.0f;

    for (const auto& t : threats)
    {
        float dx = t.x - mx;
        float dy = t.y - my;
        float dist = sqrtf(dx * dx + dy * dy);
        float clear = dist - t.rad;

        if (clear < SAFE_GAP)
        {
            for (int i = 0; i < TUNE.DIR_COUNT; i++)
            {
                float dot = (ringX[i] * dx + ringY[i] * dy) / (dist > 1e-6f ? dist : 1.0f);
                if (dot > 0.7f) danger[i] = 1.0f;
            }
        }
        else if (clear < SAFE_GAP * 3.0f)
        {
            float weight = 1.0f - (clear - SAFE_GAP) / (SAFE_GAP * 2.0f);
            for (int i = 0; i < TUNE.DIR_COUNT; i++)
            {
                float dot = (ringX[i] * dx + ringY[i] * dy) / (dist > 1e-6f ? dist : 1.0f);
                if (dot > 0.5f)
                {
                    float val = weight * dot;
                    if (val > danger[i]) danger[i] = val;
                }
            }
        }
    }

    for (const auto& b : bodies)
    {
        float dx = b.x - mx;
        float dy = b.y - my;
        float dist = sqrtf(dx * dx + dy * dy);
        float clear = dist - (b.rad + 120.0f);

        if (clear < SAFE_GAP)
        {
            for (int i = 0; i < TUNE.DIR_COUNT; i++)
            {
                float dot = (ringX[i] * dx + ringY[i] * dy) / (dist > 1e-6f ? dist : 1.0f);
                if (dot > 0.7f) danger[i] = 1.0f;
            }
        }
    }

    if (wall_ready())
    {
        for (int i = 0; i < TUNE.DIR_COUNT; i++)
        {
            if (wallAhead(mx, my, ringX[i], ringY[i], CHAR_SPEED, bodyR) > 0.0f)
            {
                if (danger[i] < 0.9f) danger[i] = 0.9f;
            }
        }
    }
}

static void fillInterestMap(float* interest, float prevDirX, float prevDirY)
{
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        float dot = ringX[i] * prevDirX + ringY[i] * prevDirY;
        interest[i] = (dot + 1.0f) * 0.5f;
    }
}

static int selectBestDirection(const float* danger, const float* interest)
{
    float minDanger = 1e18f;
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        if (danger[i] < minDanger) minDanger = danger[i];
    }

    int bestIdx = -1;
    float bestScore = -1e18f;

    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        if (danger[i] > minDanger + 0.01f) continue;
        float score = interest[i];
        if (score > bestScore)
        {
            bestScore = score;
            bestIdx = i;
        }
    }

    if (bestIdx < 0)
    {
        bestScore = -1e18f;
        for (int i = 0; i < TUNE.DIR_COUNT; i++)
        {
            float score = interest[i] - danger[i] * 10.0f;
            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = i;
            }
        }
    }

    return bestIdx;
}

void autododge_reset()
{
    proj_cache.clear();
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
    if (!base || !own_elem || dead)
    {
        active = 0;
        moving = 0;
        return;
    }

    uint64_t now = us() / 1000;
    buildRing();

    float mx = (float)own_x, my = (float)own_y;
    float myRadius = own_r < 1.0f ? 120.0f : own_r;
    if (fabsf(mx) < TUNE.MIN_POS && fabsf(my) < TUNE.MIN_POS) return;

    CHAR_SPEED = own_speed < 1.0f ? 720.0f : own_speed;
    static std::vector<Threat> threats;
    threats.clear();
    bodies.clear();
    float bodyR = myRadius + TUNE.SKIN;
    float zoneSq = TUNE.AWARE * TUNE.AWARE;

    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i]) continue;
        const uint8_t* obj = obj_block[i];
        uintptr_t objPtr = obj_snapshot[i], dataPtr = 0;
        memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));
        if (!dataPtr || !pointer_plausible(dataPtr)) continue;

        uint8_t data[PROJ_DATA_BLOCK_SIZE];
        if (!read_bytes(dataPtr, data, sizeof(data))) continue;

        uintptr_t dataVtable = 0;
        memcpy(&dataVtable, data, sizeof(dataVtable));
        if (!dataVtable || !pointer_plausible(dataVtable)) continue;
        uintptr_t vtRva = dataVtable - base;
        if (vtRva < DC_RVA_LO || vtRva >= DC_RVA_LO + DC_RVA_SIZE || dataVtable - base != VTABLE_PROJ_DATA_RVA) continue;

        int32_t pid = 0, teamId = 0, px_i = 0, py_i = 0;
        memcpy(&pid, obj + OBJ_GLOBALID_OFF, sizeof(pid));
        memcpy(&teamId, obj + OBJ_TEAM_OFF, sizeof(teamId));
        memcpy(&px_i, obj + OBJ_X_OFF, sizeof(px_i));
        memcpy(&py_i, obj + OBJ_Y_OFF, sizeof(py_i));

        float px = (float)px_i, py = (float)py_i;
        if (pid == 0 || teamId == own_team_b) continue;

        uint32_t deadFlag = 0;
        memcpy(&deadFlag, obj + OBJ_DEADFLAG_OFF, sizeof(deadFlag));
        if (deadFlag != 0) continue;

        if (px != 0.0f && py != 0.0f) bodies.push_back({px, py, myRadius});

        ProjectileState& state = proj_cache[pid];
        if (state.lastSeen == 0)
        {
            state.lastX = px_i;
            state.lastY = py_i;
            state.lastTs = now;
            int32_t ind = 0, projSpeed = 0, projRadius = 0;
            if (!read_int(objPtr + PROJECTILE_ISINDIRECT_OFF, &ind)) continue;

            memcpy(&projSpeed, data + PROJECTILEDATA_SPEED_OFF, sizeof(projSpeed));
            memcpy(&projRadius, data + PROJECTILEDATA_RADIUS_OFF, sizeof(projRadius));
            state.isIndirect = ind != 0;
            state.isBeam = call_fn(addr_projbeam, dataPtr, 0) != 0;
            state.speed = (float)projSpeed;
            state.radius = (float)projRadius;
            state.earlyTicks = (int)call_fn(addr_projearly, dataPtr, 0);
            state.spawnedAt = now - (uint64_t)state.earlyTicks * 16;
            state.spawnX = px;
            state.spawnY = py;
            state.name[0] = '\0';

            uintptr_t nameStr = call_fn(addr_dataname, dataPtr, 0);
            if (nameStr) read_scstring(nameStr, SCSTRING_DEFAULT_MAX_LENGTH, state.name, sizeof(state.name));
        }
        else
        {
            float dt = (float)(now - state.lastTs) / 1000.0f;
            float ddx = (float)px_i - (float)state.lastX;
            float ddy = (float)py_i - (float)state.lastY;
            if (dt > 0.002f && dt < 0.3f && ddx * ddx + ddy * ddy >= 16.0f)
            {
                float alpha = state.hasVelocity ? 0.4f : 1.0f;
                state.vx = state.vx * (1.0f - alpha) + (ddx / dt) * alpha;
                state.vy = state.vy * (1.0f - alpha) + (ddy / dt) * alpha;
                state.hasVelocity = true;
            }
            state.lastX = px_i;
            state.lastY = py_i;
            state.lastTs = now;
        }
        state.lastSeen = now;

        float dx = px - mx, dy = py - my;
        if (dx * dx + dy * dy > zoneSq) continue;

        bool blob = !state.isBeam && !state.isIndirect && state.speed > 0.0f && state.speed < 1600.0f;
        float speedNow = sqrtf(state.vx * state.vx + state.vy * state.vy);
        float ux = (blob || speedNow < 1.0f) ? 0.0f : state.vx / speedNow;
        float uy = (blob || speedNow < 1.0f) ? 0.0f : state.vy / speedNow;

        if (!blob && speedNow >= 1.0f)
        {
            float playerAlong = (mx - px) * ux + (my - py) * uy;
            if (playerAlong < -50.0f) continue;
            if (!state.isIndirect && !wall_los(px, py, px + ux * (playerAlong > 0.0f ? playerAlong : 0.0f), py + uy * (playerAlong > 0.0f ? playerAlong : 0.0f), WALL_BLOCKS_PROJECTILES)) continue;
        }
        threats.push_back({px, py, blob ? 0.0f : state.vx, blob ? 0.0f : state.vy, state.radius + myRadius});
    }

    for (auto it = proj_cache.begin(); it != proj_cache.end();)
    {
        if (now - it->second.lastSeen > 1024) it = proj_cache.erase(it);
        else ++it;
    }

    if (threats.empty() && bodies.empty())
    {
        if (now - lastDangerTs > TUNE.RELEASE_GRACE_MS) endDodge();
        return;
    }

    float stayClear = clearanceFor(threats, bodies, mx, my, 0.0f, 0.0f);
    bool inDanger = stayClear < TUNE.ENGAGE;
    if (inDanger) lastDangerTs = now;

    if (!inDanger && (headingIdx < 0 || now - lastDangerTs > TUNE.RELEASE_GRACE_MS))
    {
        endDodge();
        return;
    }

    bool hasPrev = commitDirX != 0.0f || commitDirY != 0.0f;
    float prevDirX = commitDirX, prevDirY = commitDirY;

    float dangerMap[128];
    float interestMap[128];

    fillDangerMap(dangerMap, threats, bodies, mx, my, bodyR);
    fillInterestMap(interestMap, prevDirX, prevDirY);

    int bestIdx = selectBestDirection(dangerMap, interestMap);

    float bestDirX = ringX[bestIdx];
    float bestDirY = ringY[bestIdx];

    if (hasPrev && !inDanger)
    {
        float dot = bestDirX * prevDirX + bestDirY * prevDirY;
        if (dot >= TUNE.TURN_DOT)
        {
            bestDirX = prevDirX;
            bestDirY = prevDirY;
        }
    }

    float outDirX = bestDirX, outDirY = bestDirY;
    if (hasPrev)
    {
        float smoothX = prevDirX * TUNE.SMOOTH + bestDirX * (1.0f - TUNE.SMOOTH);
        float smoothY = prevDirY * TUNE.SMOOTH + bestDirY * (1.0f - TUNE.SMOOTH);
        float len = sqrtf(smoothX * smoothX + smoothY * smoothY);
        if (len > 0.001f)
        {
            float nx = smoothX / len;
            float ny = smoothY / len;
            if (clearanceFor(threats, bodies, mx, my, CHAR_SPEED * nx, CHAR_SPEED * ny) > SAFE_GAP)
            {
                outDirX = nx;
                outDirY = ny;
            }
        }
    }

    commitDirX = outDirX;
    commitDirY = outDirY;
    headingIdx = bestIdx;
    lastGoodTs = now;

    int32_t tx = clamp_axis(mx + outDirX * TUNE.REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + outDirY * TUNE.REACH, wall_cache_h());
    int32_t ddx = tx - ad_target_x, ddy = ty - ad_target_y;

    if (ddx * ddx + ddy * ddy > 64 || now - lastSentAt > TUNE.SEND_MIN_MS)
    {
        lastSentAt = now;
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    active = 1;
    moving = 1;
}