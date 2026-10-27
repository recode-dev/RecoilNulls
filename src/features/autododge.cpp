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
    int DIR_COUNT = 64;
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

struct Threat
{
    float x, y;
    float vx, vy;
    float rad;
    float left;
};

struct ProjectileState
{
    float vx, vy;
    float speed;
    float radius;
    char name[128];
    float spawnX, spawnY;
    int earlyTicks;
    uint64_t spawnedAt, lastSeen, lastTs;
    int32_t lastX, lastY;
    bool isIndirect, isBeam, hasVelocity;
};

__thread int in_move = 0;
int active = 0, moving = 0;
#define NO_TARGET 1000000000
int32_t ad_target_x = NO_TARGET, ad_target_y = NO_TARGET;

static std::map<int, ProjectileState> proj_cache;
static std::vector<float> ringX, ringY;
static int headingIdx = -1;
static uint64_t lastDangerTs = 0, lastGoodTs = 0, lastSentAt = 0;
static float commitDirX = 0.0f, commitDirY = 0.0f;
static float CHAR_SPEED = 720.0f;

static constexpr float TICK_RATE_S = 0.0333f;

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

static float clearanceFor(const std::vector<Threat>& threats,
                          float mx,
                          float my,
                          float mvx,
                          float mvy)
{
    float minClear = 1e18f;
    float horizon = TUNE.HORIZON_S;

    for (const auto& t : threats)
    {
        float spd = sqrtf(t.vx * t.vx + t.vy * t.vy);
        float maxT = horizon;

        if (spd > 1.0f && t.left > 0.0f) maxT = std::min(maxT, t.left / spd);

        float relx = t.x - mx, rely = t.y - my;
        float vrx = t.vx - mvx, vry = t.vy - mvy;
        float vv = vrx * vrx + vry * vry;
        float cx, cy;

        if (vv < 1e-6f)
        {
            cx = relx;
            cy = rely;
        }
        else
        {
            float ts = -(relx * vrx + rely * vry) / vv;
            ts = std::max(0.0f, std::min(ts, maxT));
            cx = relx + vrx * ts;
            cy = rely + vry * ts;
        }

        float clear = sqrtf(cx * cx + cy * cy) - t.rad;
        if (clear < minClear) minClear = clear;
    }
    return minClear;
}

static float
wallAhead(float mx, float my, float dirX, float dirY, float speed, float bodyR)
{
    if (!wall_ready()) return 0.0f;
    float step = speed * TUNE.PROBE_T;
    int blocked = 0;
    for (int s = 1; s <= TUNE.PROBE_N; s++)
    {
        float px = mx + dirX * step * (float)s;
        float py = my + dirY * step * (float)s;
        if (wall_is_blocked_wide(px, py, bodyR, WALL_BLOCKS_MOVEMENT))
            blocked++;
    }
    return TUNE.WALL_HIT * (float)blocked / (float)TUNE.PROBE_N;
}

static int32_t clamp_axis(float value, int tiles)
{
    if (tiles <= 0) return (int32_t)roundf(value);
    float limit = (float)tiles * 300.0f - 1.0f;
    if (limit <= 0.0f) return (int32_t)roundf(value);
    return (value <= 0.0f)
               ? 0
               : ((value >= limit) ? (int32_t)limit : (int32_t)roundf(value));
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
    ad_target_x = NO_TARGET;
    ad_target_y = NO_TARGET;
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

void autododge_reset()
{
    proj_cache.clear();
    headingIdx = -1;
    lastDangerTs = 0;
    lastGoodTs = 0;
    lastSentAt = 0;
    commitDirX = 0.0f;
    commitDirY = 0.0f;
    ad_target_x = NO_TARGET;
    ad_target_y = NO_TARGET;
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
    float myRadius = own_r;
    if (myRadius < 1.0f) myRadius = 120.0f;
    if (fabsf(mx) < TUNE.MIN_POS && fabsf(my) < TUNE.MIN_POS) return;

    CHAR_SPEED = own_speed;
    if (CHAR_SPEED < 1.0f) CHAR_SPEED = 720.0f;

    static std::vector<Threat> threats;
    threats.clear();
    float bodyR = myRadius + TUNE.SKIN;
    float zoneSq = TUNE.AWARE * TUNE.AWARE;

    if (obj_count > 0)
    {
        for (int i = 0; i < obj_count; i++)
        {
            if (!obj_valid[i]) continue;
            const uint8_t* obj = obj_block[i];
            uintptr_t objPtr = obj_snapshot[i];
            uintptr_t dataPtr = 0;
            memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));

            if (!dataPtr || !pointer_plausible(dataPtr)) continue;
            uint8_t data[PROJ_DATA_BLOCK_SIZE];
            if (!read_bytes(dataPtr, data, sizeof(data))) continue;

            uintptr_t dataVtable = 0;
            memcpy(&dataVtable, data, sizeof(dataVtable));
            if (!dataVtable || !pointer_plausible(dataVtable)) continue;
            uintptr_t vtRva = dataVtable - base;
            if (vtRva < DC_RVA_LO || vtRva >= DC_RVA_LO + DC_RVA_SIZE ||
                dataVtable - base != VTABLE_PROJ_DATA_RVA)
                continue;

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

            ProjectileState& state = proj_cache[pid];
            if (state.lastSeen == 0)
            {
                state.lastX = px_i;
                state.lastY = py_i;
                state.lastTs = now;

                int32_t ind = 0, projSpeed = 0, projRadius = 0;
                if (!read_int(objPtr + PROJECTILE_ISINDIRECT_OFF, &ind))
                    continue;

                memcpy(&projSpeed,
                       data + PROJECTILEDATA_SPEED_OFF,
                       sizeof(projSpeed));
                memcpy(&projRadius,
                       data + PROJECTILEDATA_RADIUS_OFF,
                       sizeof(projRadius));
                state.isIndirect = (ind != 0);
                state.isBeam = (call_fn(addr_projbeam, dataPtr, 0) != 0);
                state.speed = (float)projSpeed;
                state.radius = (float)projRadius;
                state.earlyTicks = (int)call_fn(addr_projearly, dataPtr, 0);
                state.spawnedAt = now - (uint64_t)state.earlyTicks * 16;
                state.spawnX = px;
                state.spawnY = py;
                state.name[0] = '\0';

                uintptr_t nameStr = call_fn(addr_dataname, dataPtr, 0);
                if (nameStr)
                    read_scstring(nameStr,
                                  SCSTRING_DEFAULT_MAX_LENGTH,
                                  state.name,
                                  sizeof(state.name));
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

            float dx_dist = px - mx, dy_dist = py - my;
            if (dx_dist * dx_dist + dy_dist * dy_dist > zoneSq) continue;

            bool lockPath = state.isBeam || state.speed > 3500.0f;
            float vx = state.vx, vy = state.vy;

            if (lockPath && state.speed > 0.0f)
            {
                float flown = sqrtf((px - state.spawnX) * (px - state.spawnX) +
                                    (py - state.spawnY) * (py - state.spawnY));
                if (flown > 120.0f)
                {
                    vx = (px - state.spawnX) / flown * state.speed;
                    vy = (py - state.spawnY) / flown * state.speed;
                }
            }

            bool blob = state.isIndirect || state.speed < 1600.0f;
            float speedNow = sqrtf(vx * vx + vy * vy);
            float ux = (blob || speedNow < 1.0f) ? 0.0f : vx / speedNow;
            float uy = (blob || speedNow < 1.0f) ? 0.0f : vy / speedNow;
            float playerAlong = 0.0f;

            if (!blob && speedNow >= 1.0f)
            {
                playerAlong = (mx - px) * ux + (my - py) * uy;
                if (playerAlong < -50.0f) continue;
                if (!state.isIndirect &&
                    !wall_los(px,
                              py,
                              px + ux * std::max(playerAlong, 0.0f),
                              py + uy * std::max(playerAlong, 0.0f),
                              WALL_BLOCKS_PROJECTILES))
                    continue;
            }

            float maxLeft = state.speed * 1.5f;
            float traveled = sqrtf((px - state.spawnX) * (px - state.spawnX) +
                                   (py - state.spawnY) * (py - state.spawnY));
            float left = std::max(0.0f, maxLeft - traveled);
            if (left <= 10.0f) continue;

            float gap = sqrtf(dx_dist * dx_dist + dy_dist * dy_dist) - bodyR -
                        state.radius;
            if (left < 0.85f * std::max(0.0f, gap)) continue;
            if (!blob && speedNow >= 1.0f && playerAlong > left + state.radius)
                continue;

            float extDt = (float)(now - state.lastTs) / 1000.0f;
            if (extDt > 0.0f && extDt < TICK_RATE_S && state.hasVelocity)
            {
                px += vx * extDt;
                py += vy * extDt;
            }

            threats.push_back({px,
                               py,
                               blob ? 0.0f : vx,
                               blob ? 0.0f : vy,
                               state.radius + bodyR,
                               left});
        }
    }

    for (auto it = proj_cache.begin(); it != proj_cache.end();)
    {
        if (now - it->second.lastSeen > 1024)
            it = proj_cache.erase(it);
        else
            ++it;
    }

    if (threats.empty())
    {
        if (now - lastDangerTs > TUNE.RELEASE_GRACE_MS) endDodge();
        return;
    }

    float stayClear = clearanceFor(threats, mx, my, 0.0f, 0.0f);
    bool inDanger = stayClear < TUNE.ENGAGE;
    if (inDanger) lastDangerTs = now;
    if (!inDanger &&
        (headingIdx < 0 || now - lastDangerTs > TUNE.RELEASE_GRACE_MS))
    {
        endDodge();
        return;
    }

    bool hasPrev = (commitDirX != 0.0f || commitDirY != 0.0f);
    float prevDirX = commitDirX, prevDirY = commitDirY;
    int bestIdx = 0;
    float bestScore = -1e18f, prevScore = -1e18f;

    if (hasPrev)
    {
        prevScore =
            clearanceFor(
                threats, mx, my, CHAR_SPEED * prevDirX, CHAR_SPEED * prevDirY) -
            wallAhead(mx, my, prevDirX, prevDirY, CHAR_SPEED, bodyR) +
            TUNE.MOMENTUM;
    }

    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        float dx = ringX[i], dy = ringY[i];
        float dot = dx * prevDirX + dy * prevDirY;
        if (hasPrev && prevScore > 0.0f && dot < TUNE.TURN_DOT) continue;

        float s =
            clearanceFor(threats, mx, my, CHAR_SPEED * dx, CHAR_SPEED * dy) -
            wallAhead(mx, my, dx, dy, CHAR_SPEED, bodyR);
        if (hasPrev) s += TUNE.MOMENTUM * dot;

        if (s > bestScore)
        {
            bestScore = s;
            bestIdx = i;
        }
    }

    float chosenX = ringX[bestIdx], chosenY = ringY[bestIdx];
    float chosenScore = bestScore;

    if (hasPrev)
    {
        float dot = chosenX * prevDirX + chosenY * prevDirY;
        float margin = (prevScore > 0.0f) ? TUNE.SWITCH_MARGIN * (2.0f - dot)
                                          : TUNE.SWITCH_MIN;
        if (!(prevScore <= 0.0f && bestScore > 0.0f) &&
            bestScore <= prevScore + margin)
        {
            chosenX = prevDirX;
            chosenY = prevDirY;
            chosenScore = prevScore;
        }
    }

    if (chosenScore <= stayClear)
    {
        if (now - lastGoodTs > TUNE.RELEASE_GRACE_MS) endDodge();
        return;
    }

    float dirX = chosenX, dirY = chosenY;
    if (hasPrev)
    {
        float bx = prevDirX * TUNE.SMOOTH + chosenX * (1.0f - TUNE.SMOOTH);
        float by = prevDirY * TUNE.SMOOTH + chosenY * (1.0f - TUNE.SMOOTH);
        float len = sqrtf(bx * bx + by * by);
        if (len > 0.001f)
        {
            dirX = bx / len;
            dirY = by / len;
        }
        if (wallAhead(mx, my, dirX, dirY, CHAR_SPEED, bodyR) > 0.0f)
        {
            dirX = chosenX;
            dirY = chosenY;
        }
    }

    commitDirX = dirX;
    commitDirY = dirY;
    headingIdx = bestIdx;
    lastGoodTs = now;

    int32_t tx = clamp_axis(mx + dirX * TUNE.REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + dirY * TUNE.REACH, wall_cache_h());
    int32_t ddx = tx - ad_target_x, ddy = ty - ad_target_y;

    if ((ddx * ddx + ddy * ddy) > 64 || (now - lastSentAt > TUNE.SEND_MIN_MS))
    {
        lastSentAt = now;
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    active = 1;
    moving = 1;
}
