#include "../core/commands.h"
#include "../core/scan.h"
#include "../utils/walls.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdint.h>
#include <vector>
static struct
{
    float AWARE = 3200.0f;
    float REACT_S = 0.55f;
    int DIR_COUNT = 64;
    float SKIN = 50.0f;
    float REACH = 600.0f;
    float HORIZON_S = 1.4f;
    float MOMENTUM = 55.0f;
    float SWITCH_MARGIN = 120.0f;
    float SWITCH_MIN = 40.0f;
    float TURN_DOT = -0.2f;
    float ENGAGE = 100.0f;
    uint32_t RELEASE_GRACE_MS = 200;
    float COAST_S = 0.2f;
    float WALL_HIT = 9000.0f;
    int PROBE_N = 6;
    float PROBE_T = 0.15f;
    float WALL_BODY = 240.0f;
    uint32_t SEND_MIN_MS = 250;
    float MIN_POS = 50.0f;
} TUNE;
static const float DEFAULT_PAD = 80.0f;
static const float DEFAULT_GROW = 0.5f;
struct V2
{
    float x, y;
    V2(float x_ = 0, float y_ = 0) : x(x_), y(y_)
    {
    }
};
struct Threat
{
    float x, y;
    float vx, vy;
    float rad;
};
struct ProjectileState
{
    float vx, vy;
    float speed;
    float radius;
    uint64_t lastSeen;
    int32_t lastX, lastY;
    uint64_t lastTs;
    float spawnX, spawnY;
    bool isIndirect;
    bool isBeam;
    bool hasVelocity;
};
__thread int in_move = 0;
int active = 0;
int moving = 0;
int32_t ad_target_x = 0;
int32_t ad_target_y = 0;
static std::map<int, ProjectileState> g_proj_cache;
static std::vector<V2> g_ring;
static int g_headingIdx = -1;
static V2 g_lastDir = V2(0, 0);
static uint64_t g_lastDangerTs = 0;
static uint64_t g_lastGoodTs = 0;
static int g_sentIdx = -1;
static float g_sentDirX = 0.0f;
static float g_sentDirY = 0.0f;
static uint64_t g_lastSentAt = 0;
static float CHAR_SPEED = 720.0f;
static void buildRing()
{
    if ((int)g_ring.size() == TUNE.DIR_COUNT) return;
    g_ring.clear();
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        float a = (float)M_PI * 2.0f * (float)i / (float)TUNE.DIR_COUNT;
        g_ring.push_back(V2(cosf(a), sinf(a)));
    }
    g_headingIdx = -1;
}
static float clearanceFor(const std::vector<Threat>& threats,
                          float myX,
                          float myY,
                          float mvx,
                          float mvy)
{
    float minClear = 1e18f;
    float horizon = TUNE.HORIZON_S;
    for (const auto& t : threats)
    {
        float relx = t.x - myX;
        float rely = t.y - myY;
        float vrx = t.vx - mvx;
        float vry = t.vy - mvy;
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
            if (ts < 0)
                ts = 0;
            else if (ts > horizon)
                ts = horizon;
            cx = relx + vrx * ts;
            cy = rely + vry * ts;
        }
        float clear = sqrtf(cx * cx + cy * cy) - t.rad;
        if (clear < minClear) minClear = clear;
    }
    return minClear;
}
static float wallAhead(float myX, float myY, V2 dir, float speed, float bodyR)
{
    int blocked = 0;
    float step = 0.0f;
    if (!wall_ready()) return 0.0f;
    step = speed * TUNE.PROBE_T;
    for (int s = 1; s <= TUNE.PROBE_N; s++)
    {
        float px = myX + dir.x * step * (float)s;
        float py = myY + dir.y * step * (float)s;
        if (wall_is_blocked_wide(px, py, bodyR, WALL_BLOCKS_MOVEMENT))
        {
            blocked++;
        }
    }
    return TUNE.WALL_HIT * (float)blocked / (float)TUNE.PROBE_N;
}
static int32_t clamp_axis(float value, int tiles)
{
    float limit = 0.0f;
    if (tiles <= 0)
    {
        return (int32_t)roundf(value);
    }
    limit = (float)tiles * WALL_TILE_SIZE - 1.0f;
    if (limit <= 0.0f)
    {
        return (int32_t)roundf(value);
    }
    if (value < 0.0f)
    {
        return 0;
    }
    if (value > limit)
    {
        return (int32_t)limit;
    }
    return (int32_t)roundf(value);
}
static void clearHeading()
{
    g_headingIdx = -1;
    g_sentIdx = -1;
}
static void releaseHeading(uint64_t now, float mx, float my)
{
    V2 dir = V2(0, 0);
    int32_t tx = 0;
    int32_t ty = 0;
    float coast = 0.0f;
    if (g_headingIdx < 0) return;
    dir = g_ring[g_headingIdx];
    coast = CHAR_SPEED * TUNE.COAST_S;
    clearHeading();
    tx = clamp_axis(mx + dir.x * coast, wall_cache_w());
    ty = clamp_axis(my + dir.y * coast, wall_cache_h());
    move_to(tx, ty, mx, my);
    enqueue(tx, ty);
    g_sentDirX = dir.x;
    g_sentDirY = dir.y;
    g_lastSentAt = now;
    ad_target_x = tx;
    ad_target_y = ty;
    active = 0;
    moving = 0;
}
static int32_t call1i_local(uintptr_t fn, uintptr_t self)
{
    if (!fn || !self || !base) return 0;
    if (!pointer_plausible(self)) return 0;
    uintptr_t rva = (fn >= base) ? (fn - base) : fn;
    uintptr_t target = callable(rva);
    if (!target) return 0;
    typedef int32_t (*fn_t)(uintptr_t);
    return ((fn_t)target)(self);
}
void autododge_reset(void)
{
    g_proj_cache.clear();
    g_headingIdx = -1;
    g_lastDir = V2(0, 0);
    g_lastDangerTs = 0;
    g_lastGoodTs = 0;
    g_sentIdx = -1;
    g_sentDirX = 0.0f;
    g_sentDirY = 0.0f;
    g_lastSentAt = 0;
    ad_target_x = 0;
    ad_target_y = 0;
    active = 0;
    moving = 0;
}
void autododge(void)
{
    if (!base || !own_elem || dead)
    {
        active = 0;
        return;
    }
    uint64_t now = us() / 1000;
    buildRing();
    float mx = (float)own_x;
    float my = (float)own_y;
    float myRadius = own_r;
    if (fabsf(mx) < TUNE.MIN_POS && fabsf(my) < TUNE.MIN_POS) return;
    CHAR_SPEED = own_speed;
    static std::vector<Threat> threats;
    threats.clear();
    {
        float zoneSq = TUNE.AWARE * TUNE.AWARE;
        float bodyR = myRadius + TUNE.SKIN;
        if (obj_snapshot_n > 0)
        {
            for (int i = 0; i < obj_snapshot_n; i++)
            {
                uintptr_t objPtr = obj_snapshot[i];
                if (!objPtr)
                    continue;
                if (!pointer_plausible(objPtr)) continue;
                uintptr_t dataPtr = 0;
                if (!read_ptr(objPtr + ELEM_DEF_OFF, (void**)&dataPtr) ||
                    !dataPtr)
                    continue;
                if (!pointer_plausible(dataPtr)) continue;
                uintptr_t dataVtable = 0;
                if (!read_ptr(dataPtr, (void**)&dataVtable) || !dataVtable)
                    continue;
                if (!pointer_plausible(dataVtable)) continue;
                uintptr_t vtRva = dataVtable - base;
                if (vtRva < DC_RVA_LO || vtRva >= DC_RVA_LO + DC_RVA_SIZE)
                    continue;
                if (dataVtable - base != VTABLE_PROJ_DATA_RVA) continue;
                int32_t pid = 0;
                if (!read_int(objPtr + OBJ_GLOBALID_OFF, &pid)) continue;
                if (pid == 0) continue;
                int32_t teamId = -1;
                if (!read_int(objPtr + OBJ_TEAM_OFF, &teamId) ||
                    teamId == own_team_b)
                    continue;
                int32_t px_i = 0, py_i = 0;
                if (!read_int(objPtr + OBJ_X_OFF, &px_i) ||
                    !read_int(objPtr + OBJ_Y_OFF, &py_i))
                    continue;
                float px = (float)px_i;
                float py = (float)py_i;
                float dx_dist = px - mx;
                float dy_dist = py - my;
                uint32_t deadFlag = 0;
                if (!read_int(objPtr + OBJ_DEADFLAG_OFF, (int32_t*)&deadFlag) ||
                    deadFlag != 0)
                    continue;
                ProjectileState& state = g_proj_cache[pid];
                if (state.lastSeen == 0)
                {
                    state.spawnX = px;
                    state.spawnY = py;
                    state.lastX = px_i;
                    state.lastY = py_i;
                    state.lastTs = now;
                    int32_t ind = 0;
                    int32_t projSpeed = 0;
                    int32_t projRadius = 0;
                    if (!read_int(objPtr + PROJECTILE_ISINDIRECT_OFF, &ind))
                    {
                        continue;
                    }
                    if (!read_int(dataPtr + PROJECTILEDATA_SPEED_OFF, &projSpeed))
                    {
                        continue;
                    }
                    if (!read_int(dataPtr + PROJECTILEDATA_RADIUS_OFF,
                                  &projRadius))
                    {
                        continue;
                    }
                    state.isIndirect = (ind != 0);
                    state.isBeam =
                        (call1i_local(addr_projbeam, dataPtr) != 0);
                    state.speed = (float)projSpeed;
                    state.radius = (float)projRadius;
                }
                else
                {
                    float dt = (float)(now - state.lastTs) / 1000.0f;
                    float ddx = (float)px_i - (float)state.lastX;
                    float ddy = (float)py_i - (float)state.lastY;
                    if (dt > 0.002f && dt < 0.3f &&
                        ddx * ddx + ddy * ddy >= 16.0f)
                    {
                        float observedX = ddx / dt;
                        float observedY = ddy / dt;
                        float alpha = state.hasVelocity ? 0.4f : 1.0f;
                        state.vx = state.vx * (1.0f - alpha) + observedX * alpha;
                        state.vy = state.vy * (1.0f - alpha) + observedY * alpha;
                        state.hasVelocity = true;
                    }
                    state.lastX = px_i;
                    state.lastY = py_i;
                    state.lastTs = now;
                }
                state.lastSeen = now;
                if (dx_dist * dx_dist + dy_dist * dy_dist > zoneSq) continue;
                float radius = state.radius;
                bool blob = !state.isBeam && !state.isIndirect &&
                            state.speed > 0.0f && state.speed < 1600.0f;
                float speedNow = sqrtf(state.vx * state.vx + state.vy * state.vy);
                float ux = 0.0f;
                float uy = 0.0f;
                if (!blob && speedNow >= 1.0f)
                {
                    ux = state.vx / speedNow;
                    uy = state.vy / speedNow;
                }
                if (!blob && speedNow >= 1.0f)
                {
                    float playerAlong = (mx - px) * ux + (my - py) * uy;
                    if (playerAlong < -50.0f) continue;
                    if (playerAlong > TUNE.REACT_S * speedNow) continue;
                    if (!state.isIndirect)
                    {
                        float hX = px + ux * std::max(playerAlong, 0.0f);
                        float hY = py + uy * std::max(playerAlong, 0.0f);
                        if (!wall_los(px, py, hX, hY, WALL_BLOCKS_PROJECTILES))
                            continue;
                    }
                }
                Threat t;
                t.x = px;
                t.y = py;
                t.vx = blob ? 0.0f : state.vx;
                t.vy = blob ? 0.0f : state.vy;
                t.rad = radius + bodyR + DEFAULT_PAD + radius * DEFAULT_GROW;
                threats.push_back(t);
            }
        }
        for (auto it = g_proj_cache.begin(); it != g_proj_cache.end();)
        {
            if (now - it->second.lastSeen > 450)
            {
                it = g_proj_cache.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    if (threats.empty())
    {
        if (now - g_lastDangerTs > TUNE.RELEASE_GRACE_MS)
            releaseHeading(now, mx, my);
        return;
    }
    float stayClear = clearanceFor(threats, mx, my, 0, 0);
    bool inDanger = stayClear < TUNE.ENGAGE;
    if (inDanger) g_lastDangerTs = now;
    if (!inDanger &&
        (g_headingIdx < 0 || now - g_lastDangerTs > TUNE.RELEASE_GRACE_MS))
    {
        releaseHeading(now, mx, my);
        return;
    }
    int prevIdx = (g_sentIdx >= 0) ? g_sentIdx : g_headingIdx;
    V2 prevDir = (prevIdx >= 0) ? g_ring[prevIdx] : g_lastDir;
    bool hasPrevDir = (prevDir.x != 0.0f || prevDir.y != 0.0f);
    float bodyR = TUNE.WALL_BODY;
    int bestIdx = 0;
    float bestScore = -1e18f;
    float prevScore = -1e18f;
    if (prevIdx >= 0)
    {
        prevScore =
            clearanceFor(threats, mx, my, CHAR_SPEED * prevDir.x, CHAR_SPEED * prevDir.y);
        prevScore -= wallAhead(mx, my, prevDir, CHAR_SPEED, bodyR);
        if (hasPrevDir)
        {
            prevScore += TUNE.MOMENTUM;
        }
    }
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        const V2& d = g_ring[i];
        bool forward = (d.x * prevDir.x + d.y * prevDir.y) >= TUNE.TURN_DOT;
        float s = 0.0f;
        if (prevScore > 0.0f && hasPrevDir && !forward)
        {
            continue;
        }
        s = clearanceFor(threats, mx, my, CHAR_SPEED * d.x, CHAR_SPEED * d.y);
        s -= wallAhead(mx, my, d, CHAR_SPEED, bodyR);
        if (hasPrevDir)
        {
            s += TUNE.MOMENTUM * (d.x * prevDir.x + d.y * prevDir.y);
        }
        if (s > bestScore)
        {
            bestScore = s;
            bestIdx = i;
        }
    }
    int chosenIdx = bestIdx;
    float chosenScore = bestScore;
    if (prevIdx >= 0 && bestIdx != prevIdx)
    {
        const V2& next = g_ring[bestIdx];
        float dot = next.x * prevDir.x + next.y * prevDir.y;
        float margin = (prevScore > 0.0f)
                           ? TUNE.SWITCH_MARGIN * (2.0f - dot)
                           : TUNE.SWITCH_MIN;
        bool rescue = (prevScore <= 0.0f && bestScore > 0.0f);
        if (!rescue && bestScore <= prevScore + margin)
        {
            chosenIdx = prevIdx;
            chosenScore = prevScore;
        }
    }
    if (chosenScore <= stayClear)
    {
        if (now - g_lastGoodTs > TUNE.RELEASE_GRACE_MS)
        {
            releaseHeading(now, mx, my);
        }
        return;
    }
    V2 chosen = g_ring[chosenIdx];
    g_headingIdx = chosenIdx;
    g_lastDir = chosen;
    g_lastGoodTs = now;
    int32_t tx = clamp_axis(mx + chosen.x * TUNE.REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + chosen.y * TUNE.REACH, wall_cache_h());
    bool stale = (now - g_lastSentAt > TUNE.SEND_MIN_MS);
    if (chosenIdx != g_sentIdx || stale)
    {
        g_sentIdx = chosenIdx;
        g_sentDirX = chosen.x;
        g_sentDirY = chosen.y;
        g_lastSentAt = now;
        move_to(tx, ty, mx, my);
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    active = 1;
    moving = 1;
}