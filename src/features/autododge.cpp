#include "../core/commands.h"
#include "../core/scan.h"
#include "../utils/walls.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdint.h>
#include <stdio.h>
#include <vector>
static struct
{
    float AWARE = 3200.0f;
    uint32_t TICK_MS = 4;
    uint32_t TICK_MS_IDLE = 48;
    int DIR_COUNT = 48;
    float SKIN = 50.0f;
    uint32_t LOCK_MS = 70;
    float REACH = 600.0f;
    float HORIZON_S = 1.4f;
    float KEEP_BAND = 100.0f;
    float MOMENTUM = 55.0f;
    float ENGAGE = 100.0f;
    uint32_t RELEASE_GRACE_MS = 200;
    float WALL_HIT = 9000.0f;
    int PROBE_N = 3;
    float PROBE_T = 0.35f;
    float WALL_BODY = 240.0f;
    uint32_t SEND_MIN_MS = 80;
    float SEND_MIN_DIST = 8.0f;
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
    float left;
    float pathLen;
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
    float castRange;
    bool isIndirect;
    bool isBeam;
    bool hasVelocity;
};
__thread int in_move = 0;
int active = 0;
int moving = 0;
static std::map<int, ProjectileState> g_proj_cache;
static std::vector<V2> g_ring;
static int g_headingIdx = -1;
static uint64_t g_holdUntil = 0;
static uint64_t g_lastDangerTs = 0;
static uint64_t g_lastTick = 0;
static float g_lastSentX = 1e9f;
static float g_lastSentY = 1e9f;
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
        float spd = sqrtf(t.vx * t.vx + t.vy * t.vy);
        float maxT = horizon;
        if (spd > 1.0f)
        {
            if (t.left > 0) maxT = std::min(maxT, t.left / spd);
            if (t.pathLen > 0) maxT = std::min(maxT, t.pathLen / spd);
        }
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
            else if (ts > maxT)
                ts = maxT;
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
    if (!wall_ready()) return 0.0f;
    float step = speed * TUNE.PROBE_T;
    float penalty = 0.0f;
    for (int s = 1; s <= TUNE.PROBE_N; s++)
    {
        float px = myX + dir.x * step * (float)s;
        float py = myY + dir.y * step * (float)s;
        if (wall_is_blocked_wide(px, py, bodyR, WALL_BLOCKS_MOVEMENT))
        {
            penalty += TUNE.WALL_HIT * (float)(TUNE.PROBE_N - s + 1);
        }
    }
    return penalty;
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
    g_holdUntil = 0;
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
    g_holdUntil = 0;
    g_lastDangerTs = 0;
    g_lastSentX = 1e9f;
    g_lastSentY = 1e9f;
    g_lastSentAt = 0;
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
    uint32_t tickMs = (g_headingIdx >= 0) ? TUNE.TICK_MS : TUNE.TICK_MS_IDLE;
    if (now - g_lastTick < tickMs) return;
    g_lastTick = now;
    dbg.sent = 0;
    dbg.mapn = range_map_size();
    dbg.ranged = 0;
    buildRing();
    float mx = (float)own_x;
    float my = (float)own_y;
    float myRadius = own_r;
    if (fabsf(mx) < TUNE.MIN_POS && fabsf(my) < TUNE.MIN_POS) return;
    CHAR_SPEED = own_speed;
    std::vector<Threat> threats;
    int hits = 0;
    {
        std::map<int, bool> seen;
        float zoneSq = TUNE.AWARE * TUNE.AWARE;
        float bodyR = myRadius + TUNE.SKIN;
        if (manager_array_ptr && manager_count_v > 0 && manager_count_v < 512)
        {
            for (int i = 0; i < manager_count_v; i++)
            {
                uintptr_t objPtr = 0;
                if (!read_ptr(manager_array_ptr + (uintptr_t)i * 8ULL,
                              (void**)&objPtr) ||
                    !objPtr)
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
                if (dx_dist * dx_dist + dy_dist * dy_dist > zoneSq) continue;
                uint32_t deadFlag = 0;
                if (!read_int(objPtr + OBJ_DEADFLAG_OFF, (int32_t*)&deadFlag) ||
                    deadFlag != 0)
                    continue;
                seen[pid] = true;
                ProjectileState& state = g_proj_cache[pid];
                int32_t ownerIdx = -1;
                read_int(objPtr + OBJ_OWNERINDEX_OFF, &ownerIdx);
                if (state.lastSeen == 0)
                {
                    state.spawnX = px;
                    state.spawnY = py;
                    state.lastX = px_i;
                    state.lastY = py_i;
                    state.lastTs = now;
                    uint8_t ind = 0;
                    if (!read_byte(dataPtr + PROJ_ISTHROWER_OFF, &ind))
                    {
                        continue;
                    }
                    state.isIndirect = (ind != 0);
                    state.isBeam =
                        (call1i_local(addr_projbeam, dataPtr) != 0);
                    state.speed = (float)call1i_local(addr_projspeed, dataPtr);
                    state.radius = (float)call1i_local(addr_projradius, dataPtr);
                    state.castRange = range_for_proj(dataPtr, ownerIdx);
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
                    if (state.castRange <= 0.0f)
                    {
                        state.castRange = range_for_proj(dataPtr, ownerIdx);
                    }
                }
                state.lastSeen = now;
                float radius = state.radius > 0.0f ? state.radius : 100.0f;
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
                float traveled =
                    sqrtf((px - state.spawnX) * (px - state.spawnX) +
                          (py - state.spawnY) * (py - state.spawnY));
                float left = 1e6f;
                if (state.castRange > 0.0f)
                {
                    dbg.ranged++;
                    left = std::max(0.0f, state.castRange - traveled);
                    if (left <= 10.0f)
                    {
                        continue;
                    }
                    float gap = sqrtf(dx_dist * dx_dist + dy_dist * dy_dist) -
                                bodyR - radius;
                    if (left < 0.85f * std::max(0.0f, gap))
                    {
                        continue;
                    }
                }
                if (!blob && !state.isIndirect && speedNow >= 1.0f)
                {
                    float playerAlong = (mx - px) * ux + (my - py) * uy;
                    if (playerAlong < -50.0f) continue;
                    float hX = px + ux * std::max(playerAlong, 0.0f);
                    float hY = py + uy * std::max(playerAlong, 0.0f);
                    if (!wall_los(px, py, hX, hY, WALL_BLOCKS_PROJECTILES))
                        continue;
                    if (state.castRange > 0.0f && playerAlong > left + radius)
                        continue;
                }
                Threat t;
                t.x = px;
                t.y = py;
                t.vx = blob ? 0.0f : state.vx;
                t.vy = blob ? 0.0f : state.vy;
                t.rad = radius + bodyR + DEFAULT_PAD + radius * DEFAULT_GROW;
                t.pathLen = state.castRange > 0.0f ? left + radius : 1e6f;
                t.left = left;
                float ax = px - mx;
                float ay = py - my;
                float along = 0.0f;
                if (!blob && speedNow >= 1.0f)
                {
                    along = ax * ux + ay * uy;
                }
                if (along < 0.0f)
                {
                    along = 0.0f;
                }
                float hx = ax - ux * along;
                float hy = ay - uy * along;
                if (hx * hx + hy * hy <= t.rad * t.rad)
                {
                    hits++;
                }
                threats.push_back(t);
            }
        }
        for (auto it = g_proj_cache.begin(); it != g_proj_cache.end();)
        {
            if (seen.find(it->first) == seen.end() ||
                now - it->second.lastSeen > 450)
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
        dbg.threats = 0;
        if (g_headingIdx >= 0 && now - g_lastDangerTs > TUNE.RELEASE_GRACE_MS)
            clearHeading();
        return;
    }
    float stayClear = clearanceFor(threats, mx, my, 0, 0);
    bool inDanger = stayClear < TUNE.ENGAGE;
    if (inDanger) g_lastDangerTs = now;
    if (!inDanger &&
        (g_headingIdx < 0 || now - g_lastDangerTs > TUNE.RELEASE_GRACE_MS))
    {
        if (g_headingIdx >= 0) clearHeading();
        return;
    }
    int prevIdx = g_headingIdx;
    V2 prevDir = (prevIdx >= 0) ? g_ring[prevIdx] : V2(0, 0);
    float bodyR = TUNE.WALL_BODY;
    int bestIdx = 0;
    float bestScore = -1e18f;
    float prevScore = -1e18f;
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        const V2& d = g_ring[i];
        float s =
            clearanceFor(threats, mx, my, CHAR_SPEED * d.x, CHAR_SPEED * d.y);
        s -= wallAhead(mx, my, d, CHAR_SPEED, bodyR);
        if (prevIdx >= 0)
            s += TUNE.MOMENTUM * (d.x * prevDir.x + d.y * prevDir.y);
        if (i == prevIdx) prevScore = s;
        if (s > bestScore)
        {
            bestScore = s;
            bestIdx = i;
        }
    }
    int chosenIdx = bestIdx;
    float chosenScore = bestScore;
    if (prevIdx >= 0 && now < g_holdUntil && chosenIdx != prevIdx)
    {
        if (prevScore + TUNE.KEEP_BAND >= bestScore)
        {
            chosenIdx = prevIdx;
            chosenScore = prevScore;
        }
        else
        {
            g_holdUntil = now + TUNE.LOCK_MS;
        }
    }
    else if (now >= g_holdUntil)
    {
        g_holdUntil = now + TUNE.LOCK_MS;
    }
    if (chosenScore <= stayClear)
    {
        if (g_headingIdx >= 0) clearHeading();
        return;
    }
    V2 chosen = g_ring[chosenIdx];
    g_headingIdx = chosenIdx;
    int32_t tx = clamp_axis(mx + chosen.x * TUNE.REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + chosen.y * TUNE.REACH, wall_cache_h());
    float ddx = (float)tx - g_lastSentX;
    float ddy = (float)ty - g_lastSentY;
    bool moved =
        (ddx * ddx + ddy * ddy > TUNE.SEND_MIN_DIST * TUNE.SEND_MIN_DIST);
    bool stale = (now - g_lastSentAt > TUNE.SEND_MIN_MS);
    if (moved || stale)
    {
        g_lastSentX = (float)tx;
        g_lastSentY = (float)ty;
        g_lastSentAt = now;
        move_to(tx, ty, mx, my);
        enqueue(tx, ty);
        dbg.sent = 1;
    }
    active = 1;
    moving = 1;
    dbg.px = mx;
    dbg.py = my;
    dbg.dx = chosen.x;
    dbg.dy = chosen.y;
    dbg.tx = (float)tx;
    dbg.ty = (float)ty;
    dbg.hazards = (int)threats.size();
    dbg.threats = hits;
    snprintf(dbg.note,
             sizeof(dbg.note),
             "CLR:%.0f SCR:%.0f",
             stayClear,
             chosenScore);
}