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

static const float DEFAULT_PAD = 80.0f;
static const float DEFAULT_GROW = 0.5f;

struct V2
{
    float x, y;
    V2(float x_ = 0, float y_ = 0) : x(x_), y(y_) {}
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
    char name[128];
    float spawnX, spawnY;
    int earlyTicks;
    uint64_t spawnedAt;
    uint64_t lastSeen;
    int32_t lastX, lastY;
    uint64_t lastTs;
    bool isIndirect;
    bool isBeam;
    bool hasVelocity;
};

__thread int in_move = 0;
int active = 0;
int moving = 0;
#define NO_TARGET 1000000000
int32_t ad_target_x = NO_TARGET;
int32_t ad_target_y = NO_TARGET;

static std::map<int, ProjectileState> proj_cache;
static std::vector<V2> ring;
static int headingIdx = -1;
static uint64_t lastDangerTs = 0;
static uint64_t lastGoodTs = 0;
static V2 commitDir = V2(0, 0);
static uint64_t lastSentAt = 0;
static float CHAR_SPEED = 720.0f;

static void buildRing()
{
    if (static_cast<int>(ring.size()) == TUNE.DIR_COUNT) return;
    ring.clear();
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        float a = static_cast<float>(M_PI) * 2.0f * static_cast<float>(i) /
                  static_cast<float>(TUNE.DIR_COUNT);
        ring.push_back(V2(cosf(a), sinf(a)));
    }
    headingIdx = -1;
}

static float clearanceFor(const std::vector<Threat>& threats,
                          float myX,
                          float myY,
                          float mvx,
                          float mvy)
{
    float minClear = 1e18f;
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
            else if (ts > TUNE.HORIZON_S)
                ts = TUNE.HORIZON_S;
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
    
    int blocked = 0;
    float step = speed * TUNE.PROBE_T;
    float effectiveBodyR = 120.0f + TUNE.SKIN;

    for (int s = 1; s <= TUNE.PROBE_N; s++)
    {
        float px = myX + dir.x * step * static_cast<float>(s);
        float py = myY + dir.y * step * static_cast<float>(s);
        if (wall_is_blocked_wide(px, py, effectiveBodyR, WALL_BLOCKS_MOVEMENT))
        {
            blocked++;
        }
    }
    return TUNE.WALL_HIT * static_cast<float>(blocked) /
           static_cast<float>(TUNE.PROBE_N);
}

static int32_t clamp_axis(float value, int tiles)
{
    float limit = 0.0f;
    if (tiles <= 0)
    {
        return static_cast<int32_t>(roundf(value));
    }
    limit = static_cast<float>(tiles) * WALL_TILE_SIZE - 1.0f;
    if (limit <= 0.0f)
    {
        return static_cast<int32_t>(roundf(value));
    }
    if (value < 0.0f)
    {
        return 0;
    }
    if (value > limit)
    {
        return static_cast<int32_t>(limit);
    }
    return static_cast<int32_t>(roundf(value));
}

static void clearHeading()
{
    headingIdx = -1;
    commitDir = V2(0, 0);
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
    commitDir = V2(0, 0);
    lastSentAt = 0;
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

    float mx = static_cast<float>(own_x);
    float my = static_cast<float>(own_y);
    float myRadius = own_r;

    if (myRadius < 1.0f) myRadius = 120.0f;

    if (fabsf(mx) < TUNE.MIN_POS && fabsf(my) < TUNE.MIN_POS) return;

    CHAR_SPEED = own_speed;

    static std::vector<Threat> threats;
    threats.clear();

    {
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
                if (!dataPtr) continue;
                if (!pointer_plausible(dataPtr)) continue;
                
                uint8_t data[PROJ_DATA_BLOCK_SIZE];
                if (!read_bytes(dataPtr, data, sizeof(data))) continue;
                
                uintptr_t dataVtable = 0;
                memcpy(&dataVtable, data, sizeof(dataVtable));
                if (!dataVtable) continue;
                if (!pointer_plausible(dataVtable)) continue;
                
                uintptr_t vtRva = dataVtable - base;
                if (vtRva < DC_RVA_LO || vtRva >= DC_RVA_LO + DC_RVA_SIZE)
                    continue;
                if (dataVtable - base != VTABLE_PROJ_DATA_RVA) continue;
                
                int32_t pid = 0;
                memcpy(&pid, obj + OBJ_GLOBALID_OFF, sizeof(pid));
                if (pid == 0) continue;
                
                int32_t teamId = 0;
                memcpy(&teamId, obj + OBJ_TEAM_OFF, sizeof(teamId));
                if (teamId == own_team_b) continue;
                
                int32_t px_i = 0, py_i = 0;
                memcpy(&px_i, obj + OBJ_X_OFF, sizeof(px_i));
                memcpy(&py_i, obj + OBJ_Y_OFF, sizeof(py_i));
                float px = static_cast<float>(px_i);
                float py = static_cast<float>(py_i);
                
                float dx_dist = px - mx;
                float dy_dist = py - my;
                
                uint32_t deadFlag = 0;
                memcpy(&deadFlag, obj + OBJ_DEADFLAG_OFF, sizeof(deadFlag));
                if (deadFlag != 0) continue;
                
                ProjectileState& state = proj_cache[pid];
                if (state.lastSeen == 0)
                {
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
                    
                    memcpy(&projSpeed,
                           data + PROJECTILEDATA_SPEED_OFF,
                           sizeof(projSpeed));
                    memcpy(&projRadius,
                           data + PROJECTILEDATA_RADIUS_OFF,
                           sizeof(projRadius));
                    
                    state.isIndirect = (ind != 0);
                    state.isBeam = (call_fn(addr_projbeam, dataPtr, 0) != 0);
                    state.speed = static_cast<float>(projSpeed);
                    state.radius = static_cast<float>(projRadius);
                    state.earlyTicks =
                        static_cast<int>(call_fn(addr_projearly, dataPtr, 0));
                    state.spawnedAt =
                        now - static_cast<uint64_t>(state.earlyTicks) * 16;
                    state.name[0] = '\0';
                    
                    {
                        uintptr_t nameStr = call_fn(addr_dataname, dataPtr, 0);
                        if (nameStr)
                        {
                            read_scstring(nameStr,
                                          SCSTRING_DEFAULT_MAX_LENGTH,
                                          state.name,
                                          sizeof(state.name));
                        }
                    }
                    
                    state.spawnX = static_cast<float>(px_i);
                    state.spawnY = static_cast<float>(py_i);
                }
                else
                {
                    float dt = static_cast<float>(now - state.lastTs) / 1000.0f;
                    float ddx = static_cast<float>(px_i) -
                                static_cast<float>(state.lastX);
                    float ddy = static_cast<float>(py_i) -
                                static_cast<float>(state.lastY);
                    if (dt > 0.002f && dt < 0.3f &&
                        ddx * ddx + ddy * ddy >= 16.0f)
                    {
                        float observedX = ddx / dt;
                        float observedY = ddy / dt;
                        float alpha = state.hasVelocity ? 0.4f : 1.0f;
                        state.vx =
                            state.vx * (1.0f - alpha) + observedX * alpha;
                        state.vy =
                            state.vy * (1.0f - alpha) + observedY * alpha;
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
                            
                float speedNow =
                    sqrtf(state.vx * state.vx + state.vy * state.vy);
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
                
                t.rad = state.radius + myRadius + TUNE.SKIN;
                
                threats.push_back(t);
            }
        }
        
        for (auto it = proj_cache.begin(); it != proj_cache.end();)
        {
            if (now - it->second.lastSeen > 1024)
            {
                it = proj_cache.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    
    if (threats.empty())
    {
        if (now - lastDangerTs > TUNE.RELEASE_GRACE_MS) endDodge();
        return;
    }
    
    float stayClear = clearanceFor(threats, mx, my, 0, 0);
    bool inDanger = stayClear < TUNE.ENGAGE;
    
    if (inDanger) lastDangerTs = now;
    
    if (!inDanger &&
        (headingIdx < 0 || now - lastDangerTs > TUNE.RELEASE_GRACE_MS))
    {
        endDodge();
        return;
    }
    
    bool hasPrev = (commitDir.x != 0.0f || commitDir.y != 0.0f);
    V2 prevDir = commitDir;
    
    float bodyR = myRadius + TUNE.SKIN;
    int bestIdx = 0;
    float bestScore = -1e18f;
    float prevScore = -1e18f;
    
    if (hasPrev)
    {
        prevScore = clearanceFor(
            threats, mx, my, CHAR_SPEED * prevDir.x, CHAR_SPEED * prevDir.y);
        prevScore -= wallAhead(mx, my, prevDir, CHAR_SPEED, bodyR);
        prevScore += TUNE.MOMENTUM;
    }
    
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        const V2& d = ring[i];
        bool forward = (d.x * prevDir.x + d.y * prevDir.y) >= TUNE.TURN_DOT;
        float s = 0.0f;
        
        if (prevScore > 0.0f && hasPrev && !forward)
        {
            continue;
        }
        
        s = clearanceFor(threats, mx, my, CHAR_SPEED * d.x, CHAR_SPEED * d.y);
        s -= wallAhead(mx, my, d, CHAR_SPEED, bodyR);
        
        if (hasPrev)
        {
            s += TUNE.MOMENTUM * (d.x * prevDir.x + d.y * prevDir.y);
        }
        
        if (s > bestScore)
        {
            bestScore = s;
            bestIdx = i;
        }
    }
    
    V2 chosen = ring[bestIdx];
    float chosenScore = bestScore;
    
    if (hasPrev)
    {
        float dot = chosen.x * prevDir.x + chosen.y * prevDir.y;
        float margin = (prevScore > 0.0f) ? TUNE.SWITCH_MARGIN * (2.0f - dot)
                                          : TUNE.SWITCH_MIN;
        bool rescue = (prevScore <= 0.0f && bestScore > 0.0f);
        
        if (!rescue && bestScore <= prevScore + margin)
        {
            chosen = prevDir;
            chosenScore = prevScore;
        }
    }
    
    if (chosenScore <= stayClear)
    {
        if (now - lastGoodTs > TUNE.RELEASE_GRACE_MS)
        {
            endDodge();
        }
        return;
    }
    
    V2 dir = chosen;
    
    if (hasPrev)
    {
        float bx = prevDir.x * TUNE.SMOOTH + chosen.x * (1.0f - TUNE.SMOOTH);
        float by = prevDir.y * TUNE.SMOOTH + chosen.y * (1.0f - TUNE.SMOOTH);
        float len = sqrtf(bx * bx + by * by);
        
        if (len > 0.001f)
        {
            dir.x = bx / len;
            dir.y = by / len;
        }
        
        if (wallAhead(mx, my, dir, CHAR_SPEED, bodyR) > 0.0f)
        {
            dir = chosen;
        }
    }
    
    commitDir = dir;
    headingIdx = bestIdx;
    lastGoodTs = now;
    
    int32_t tx = clamp_axis(mx + dir.x * TUNE.REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + dir.y * TUNE.REACH, wall_cache_h());
    
    int32_t ddx = tx - ad_target_x;
    int32_t ddy = ty - ad_target_y;
    bool moved = (ddx * ddx + ddy * ddy) > (8 * 8);
    bool stale = (now - lastSentAt > TUNE.SEND_MIN_MS);
    
    if (moved || stale)
    {
        lastSentAt = now;
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    
    active = 1;
    moving = 1;
}