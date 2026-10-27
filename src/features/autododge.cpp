#include "../core/commands.h"
#include "../core/scan.h"
#include "../utils/walls.h"
#include "dodge_core.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdint.h>
#include <vector>
typedef DodgeThreat Threat;
// The decision maths lives in dodge_core.h, the same file the offline
// simulation (sim/sim.cpp) includes, so the numbers below are the numbers that
// were measured -- nothing is tuned blind on the device.
// Chosen over 20s x 200 seeds, replayed on a second independent seed base:
//   DOOM_CLEAR = -160: the defensive bubble of a mid-range shot here is
//     radius + bodyR + PAD + radius*GROW ~= 410 while the real collision
//     radius is 250, so a best raw clearance under -160 means no heading can
//     get the body out of the shot any more -- it has already landed. The
//     shipped trace showed every 150deg+ reversal happening in exactly this
//     state: the character spins around the instant a hit registers, which is
//     the "разворот после того как отдоджил".
//   RELEASE_HOLD_MS = 900: remember the heading after a release instead of
//     forgetting it, so the next fight does not open with an instant 180deg
//     reversal.
//   SMOOTH = 0.7: rides on top of the two above without costing survival.
//   DECIDE_MS = 0: throttling decisions to 40ms was measured to LOSE hits
//     (2183/2204 vs the 1975/1988 baseline) -- reaction has to stay per-frame,
//     the smoothness has to come from the rules above instead.
struct AdTune
{
    DodgeTune core;
    // Threat extraction only: how much of the map we look at, how far ahead a
    // shot is considered live, the body skin, the glide length on release and
    // the "we are not on the map yet" guard.
    float AWARE;
    float REACT_S;
    float SKIN;
    float COAST_S;
    float MIN_POS;
};
static const AdTune TUNE = {
    {
        1.4f,    // HORIZON_S
        55.0f,   // MOMENTUM
        120.0f,  // SWITCH_MARGIN
        40.0f,   // SWITCH_MIN
        -0.2f,   // TURN_DOT
        0.7f,    // SMOOTH
        100.0f,  // ENGAGE
        200.0f,  // RELEASE_GRACE_MS
        600.0f,  // REACH
        9000.0f, // WALL_HIT
        6,       // PROBE_N
        0.15f,   // PROBE_T
        240.0f,  // WALL_BODY
        64,      // DIR_COUNT
        250.0f,  // SEND_MIN_MS
        0.99f,   // SEND_TURN_DOT
        0,       // DECIDE_MS
        0.0f,    // TURN_RATE
        1.0f,    // DEADBAND_COS
        900.0f,  // RELEASE_HOLD_MS
        0,       // WALL_GRAD
        1,       // STABLE_N
        0.0f,    // URGENT_MS
        -160.0f  // DOOM_CLEAR
    },
    3200.0f, // AWARE
    0.55f,   // REACT_S
    50.0f,   // SKIN
    0.2f,    // COAST_S
    50.0f    // MIN_POS
};
static const float DEFAULT_PAD = 80.0f;
static const float DEFAULT_GROW = 0.5f;
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
// Zero-initialised is fine: the core only trusts the fields it wrote itself,
// and engaged = 0 makes every other field inert.
static DodgeState g_st;
// The one wall hook the core needs. It probes the path on its own, so both the
// step and the gradient modes work without a game-specific second copy.
static int blockedAt(void*,
                     float px,
                     float py,
                     float,
                     float,
                     float,
                     float bodyR)
{
    if (!wall_ready())
        return 0;
    return wall_is_blocked_wide(px, py, bodyR, WALL_BLOCKS_MOVEMENT) ? 1 : 0;
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
    dodge_reset(g_st);
    ad_target_x = 0;
    ad_target_y = 0;
    active = 0;
    moving = 0;
}
void autododge(void)
{
    const DodgeTune& C = TUNE.core;
    if (!base || !own_elem || dead)
    {
        active = 0;
        return;
    }
    uint64_t now = us() / 1000;
    float mx = (float)own_x;
    float my = (float)own_y;
    float myRadius = own_r;
    if (fabsf(mx) < TUNE.MIN_POS && fabsf(my) < TUNE.MIN_POS) return;
    float speed = own_speed;
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
    DodgeDecision dec = dodge_decide(g_st,
                                     C,
                                     mx,
                                     my,
                                     speed,
                                     threats.empty() ? nullptr : &threats[0],
                                     (int)threats.size(),
                                     now,
                                     blockedAt,
                                     nullptr);
    if (dec.intent == DODGE_INTENT_MOVE)
    {
        active = 1;
        moving = 1;
        // The core owns the send policy: it only asks for a wire command once
        // the heading really turned or the last one went stale.
        if (!dec.resend)
            return;
        int32_t tx = clamp_axis(mx + dec.dirX * C.REACH, wall_cache_w());
        int32_t ty = clamp_axis(my + dec.dirY * C.REACH, wall_cache_h());
        move_to(tx, ty, mx, my);
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
        return;
    }
    if (dec.intent == DODGE_INTENT_RELEASE)
    {
        // Glide out along the heading the body was actually travelling on.
        // Unlike the old build, the core keeps that heading remembered for
        // RELEASE_HOLD_MS, so the next engage does not start with a reversal.
        float coast = speed * TUNE.COAST_S;
        int32_t tx = clamp_axis(mx + dec.dirX * coast, wall_cache_w());
        int32_t ty = clamp_axis(my + dec.dirY * coast, wall_cache_h());
        move_to(tx, ty, mx, my);
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
        active = 0;
        moving = 0;
        return;
    }
    active = 0;
    moving = 0;
}
