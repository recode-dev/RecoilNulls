#include "../core/commands.h"
#include "../core/scan.h"
#include "../core/scstring.h"
#define PROJ_DATA_BLOCK_SIZE (PROJECTILEDATA_RADIUS_OFF + sizeof(int32_t))
#include "../utils/walls.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <cstdint>
#include <vector>
#include <array>

int active = 0;
int moving = 0;
int32_t ad_target_x = INT32_MAX;
int32_t ad_target_y = INT32_MAX;

static struct
{
    float AWARE = 4400.0f;
    float REACT_S = 0.35f;
    int DIR_COUNT = 32;
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

namespace
{
constexpr float CLEAR_INF = std::numeric_limits<float>::infinity();
constexpr float TTC_INF = std::numeric_limits<float>::infinity();
constexpr float VV_EPS = 1e-6f;
constexpr float LEN_EPS = 0.001f;
constexpr float DT_MIN = 0.002f;
constexpr float DT_MAX = 0.3f;
constexpr float POS_DELTA_MIN_SQ = 16.0f;
constexpr float PLAYER_ALONG_MIN = -50.0f;
constexpr float BLOB_SPEED_MAX = 1600.0f;
constexpr float VEL_ALPHA_NEW = 1.0f;
constexpr float VEL_ALPHA_EXIST = 0.4f;
constexpr float HITBOX_FALLBACK = 120.0f;
constexpr float SPEED_FALLBACK = 720.0f;
constexpr float ORIGIN_EPS = 1.0f;
constexpr float BODY_EXTRA_R = 120.0f;
constexpr float DANGER_RADIUS = 180.0f;
constexpr float TTC_MIN = 0.09f;
constexpr float DANGER_BLEND = 0.65f;
constexpr float DANGER_WALL = 0.9f;
constexpr float DANGER_SUM_WEIGHT = 0.15f;
constexpr float DANGER_MAX = 1.0f;
constexpr float DANGER_PICK_EPS = 0.01f;
constexpr float INTEREST_ESCAPE_W = 0.7f;
constexpr float INTEREST_GOAL_W = 0.3f;
constexpr float INTEREST_BACK_DOT = -0.5f;
constexpr float INTEREST_BACK_PENALTY = 0.2f;
constexpr float ESCAPE_DIST_BIAS = 100.0f;
constexpr float POS_CHANGE_MIN_SQ = 64.0f;
constexpr float QUADRATIC_K = 2.0f;
constexpr float CLEAR_WEIGHT_MAX = 10.0f;
constexpr float HALF = 0.5f;
constexpr float SPEED_MIN = 1.0f;

struct Threat
{
    float x, y, vx, vy, rad;
};
struct Body
{
    float x, y, rad;
};

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

static std::vector<ProjectileState> proj_cache_storage;
static std::map<int, size_t> proj_cache_index;

static std::array<float, 256> ringX, ringY;
static std::vector<Body> bodies;
static std::vector<Threat> threats;
static int headingIdx = -1;
static uint64_t lastDangerTs = 0, lastGoodTs = 0, lastSentAt = 0;
static float commitDirX = 0.0f, commitDirY = 0.0f;
static float CHAR_SPEED = SPEED_FALLBACK;

static std::array<float, 256> prevDangerMap{};
static bool hasPrevDanger = false;

static void buildRing()
{
    static int lastCount = -1;
    if (lastCount == TUNE.DIR_COUNT) return;
    lastCount = TUNE.DIR_COUNT;
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        float a = (float)M_PI * QUADRATIC_K * (float)i / (float)TUNE.DIR_COUNT;
        ringX[i] = cosf(a);
        ringY[i] = sinf(a);
    }
    headingIdx = -1;
    hasPrevDanger = false;
}

static float clearanceFor(const std::vector<Threat>& threats,
                          const std::vector<Body>& bodies,
                          float mx,
                          float my,
                          float mvx,
                          float mvy)
{
    float minClearSq = CLEAR_INF;
    for (const auto& t : threats)
    {
        float relx = t.x - mx;
        float rely = t.y - my;
        float vrx = t.vx - mvx;
        float vry = t.vy - mvy;
        float vv = vrx * vrx + vry * vry;
        float ts = vv > VV_EPS ? -(relx * vrx + rely * vry) / vv : 0.0f;
        ts = ts < 0.0f ? 0.0f : (ts > TUNE.HORIZON_S ? TUNE.HORIZON_S : ts);
        float cx = relx + vrx * ts;
        float cy = rely + vry * ts;
        float clearSq = (cx * cx + cy * cy) - (t.rad * t.rad);
        if (clearSq < minClearSq) minClearSq = clearSq;
    }
    for (const auto& b : bodies)
    {
        float dx = b.x - mx;
        float dy = b.y - my;
        float r = b.rad + BODY_EXTRA_R;
        float clearSq = (dx * dx + dy * dy) - (r * r);
        if (clearSq < minClearSq) minClearSq = clearSq;
    }
    return minClearSq;
}

static float
timeToCollision(float mx, float my, float mvx, float mvy, const Threat& t)
{
    float relx = t.x - mx;
    float rely = t.y - my;
    float vrx = t.vx - mvx;
    float vry = t.vy - mvy;
    float vv = vrx * vrx + vry * vry;
    if (vv < VV_EPS) return TTC_INF;
    float b = QUADRATIC_K * (relx * vrx + rely * vry);
    float c = relx * relx + rely * rely - t.rad * t.rad;
    float disc = b * b - QUADRATIC_K * QUADRATIC_K * vv * c;
    if (disc < 0.0f) return TTC_INF;
    float sqrtDisc = sqrtf(disc);
    float t1 = (-b - sqrtDisc) / (QUADRATIC_K * vv);
    float t2 = (-b + sqrtDisc) / (QUADRATIC_K * vv);
    if (t1 > 0.0f) return t1;
    if (t2 > 0.0f) return t2;
    return TTC_INF;
}

static float
wallAhead(float mx, float my, float dirX, float dirY, float speed, float bodyR)
{
    if (!wall_ready()) return 0.0f;
    float step = speed * TUNE.PROBE_T;
    int blocked = 0;
    for (int s = 1; s <= TUNE.PROBE_N; s++)
    {
        if (wall_is_blocked_wide(mx + dirX * step * s,
                                 my + dirY * step * s,
                                 bodyR,
                                 WALL_BLOCKS_MOVEMENT))
            blocked++;
    }
    return TUNE.WALL_HIT * (float)blocked / (float)TUNE.PROBE_N;
}

static int32_t clamp_axis(float value, int tiles)
{
    if (tiles <= 0) return (int32_t)roundf(value);
    float limit = (float)tiles * WALL_TILE_SIZE - SPEED_MIN;
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
    hasPrevDanger = false;
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

static float
dangerFor(float mx, float my, float dx, float dy, float speed, float bodyR)
{
    float mvx = speed * dx;
    float mvy = speed * dy;
    float max_c = 0.0f;
    float sum_c = 0.0f;
    bool ttc_veto = false;

    for (const auto& t : threats)
    {
        float ttc = timeToCollision(mx, my, mvx, mvy, t);
        if (ttc < TTC_MIN)
        {
            ttc_veto = true;
            break;
        }

        float relx = t.x - mx;
        float rely = t.y - my;
        float vrx = t.vx - mvx;
        float vry = t.vy - mvy;
        float vv = vrx * vrx + vry * vry;
        float ts = vv > VV_EPS ? -(relx * vrx + rely * vry) / vv : 0.0f;
        ts = ts < 0.0f ? 0.0f : (ts > TUNE.HORIZON_S ? TUNE.HORIZON_S : ts);
        float cx = relx + vrx * ts;
        float cy = rely + vry * ts;
        float m = sqrtf(cx * cx + cy * cy) - t.rad;

        float c = (DANGER_RADIUS - m) / DANGER_RADIUS;
        if (c < 0.0f) c = 0.0f;
        if (c > max_c) max_c = c;
        sum_c += c;
    }

    if (ttc_veto) return DANGER_MAX;

    float wallPenalty = wallAhead(mx, my, dx, dy, speed, bodyR);
    if (wallPenalty > 0.0f) return DANGER_WALL;

    float danger = max_c + DANGER_SUM_WEIGHT * (sum_c - max_c);
    return danger > DANGER_MAX ? DANGER_MAX : danger;
}

static void fillInterestMap(std::array<float, 256>& interest,
                            float mx,
                            float my,
                            float prevX,
                            float prevY)
{
    float ex = 0.0f, ey = 0.0f;
    for (const auto& t : threats)
    {
        float dx = mx - t.x;
        float dy = my - t.y;
        float distSq = dx * dx + dy * dy;
        if (distSq > ORIGIN_EPS)
        {
            float dist = sqrtf(distSq);
            float invDist = SPEED_MIN / dist;
            float w = SPEED_MIN / (dist + ESCAPE_DIST_BIAS);
            ex += (dx * invDist) * w;
            ey += (dy * invDist) * w;
        }
    }
    float elen = sqrtf(ex * ex + ey * ey);
    if (elen > LEN_EPS)
    {
        ex /= elen;
        ey /= elen;
    }

    float gx = 0.0f, gy = 0.0f;
    if (wall_cache_w() > 0 && wall_cache_h() > 0)
    {
        gx = (wall_cache_w() * WALL_TILE_SIZE * HALF) - mx;
        gy = (wall_cache_h() * WALL_TILE_SIZE * HALF) - my;
        float glen = sqrtf(gx * gx + gy * gy);
        if (glen > LEN_EPS)
        {
            gx /= glen;
            gy /= glen;
        }
    }

    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        float dotEscape = ringX[i] * ex + ringY[i] * ey;
        float escapeScore = (dotEscape + SPEED_MIN) * HALF;

        float dotGoal = ringX[i] * gx + ringY[i] * gy;
        float goalScore = (dotGoal + SPEED_MIN) * HALF;

        float score =
            escapeScore * INTEREST_ESCAPE_W + goalScore * INTEREST_GOAL_W;

        if (prevX != 0.0f || prevY != 0.0f)
        {
            float dotPrev = ringX[i] * prevX + ringY[i] * prevY;
            if (dotPrev < INTEREST_BACK_DOT) score -= INTEREST_BACK_PENALTY;
        }
        interest[i] = score;
    }
}

static void snapTo4Way(float& dx, float& dy)
{
    if (fabsf(dx) >= fabsf(dy))
    {
        dx = dx >= 0.0f ? SPEED_MIN : -SPEED_MIN;
        dy = 0.0f;
    }
    else
    {
        dx = 0.0f;
        dy = dy >= 0.0f ? SPEED_MIN : -SPEED_MIN;
    }
}
}

void autododge_reset()
{
    proj_cache_storage.clear();
    proj_cache_index.clear();
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
    hasPrevDanger = false;
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
    float myRadius = own_r < SPEED_MIN ? HITBOX_FALLBACK : own_r;
    if (fabsf(mx) < TUNE.MIN_POS && fabsf(my) < TUNE.MIN_POS) return;

    CHAR_SPEED = own_speed < SPEED_MIN ? SPEED_FALLBACK : own_speed;
    threats.clear();
    bodies.clear();
    threats.reserve(TUNE.DIR_COUNT);
    bodies.reserve(TUNE.DIR_COUNT / QUADRATIC_K);

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

        if (px != 0.0f && py != 0.0f) bodies.push_back({px, py, myRadius});

        auto it = proj_cache_index.find(pid);
        if (it == proj_cache_index.end())
        {
            ProjectileState state{};
            state.lastX = px_i;
            state.lastY = py_i;
            state.lastTs = now;
            int32_t ind = 0, projSpeed = 0, projRadius = 0;
            if (!read_int(objPtr + PROJECTILE_ISINDIRECT_OFF, &ind)) continue;

            memcpy(
                &projSpeed, data + PROJECTILEDATA_SPEED_OFF, sizeof(projSpeed));
            memcpy(&projRadius,
                   data + PROJECTILEDATA_RADIUS_OFF,
                   sizeof(projRadius));
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
            if (nameStr)
                read_scstring(nameStr,
                              SCSTRING_DEFAULT_MAX_LENGTH,
                              state.name,
                              sizeof(state.name));

            proj_cache_index[pid] = proj_cache_storage.size();
            proj_cache_storage.push_back(state);
            it = proj_cache_index.find(pid);
        }
        else
        {
            ProjectileState& state = proj_cache_storage[it->second];
            float dt = (float)(now - state.lastTs) / 1000.0f;
            float ddx = (float)px_i - (float)state.lastX;
            float ddy = (float)py_i - (float)state.lastY;
            if (dt > DT_MIN && dt < DT_MAX &&
                ddx * ddx + ddy * ddy >= POS_DELTA_MIN_SQ)
            {
                float alpha =
                    state.hasVelocity ? VEL_ALPHA_EXIST : VEL_ALPHA_NEW;
                state.vx = state.vx * (SPEED_MIN - alpha) + (ddx / dt) * alpha;
                state.vy = state.vy * (SPEED_MIN - alpha) + (ddy / dt) * alpha;
                state.hasVelocity = true;
            }
            state.lastX = px_i;
            state.lastY = py_i;
            state.lastTs = now;
        }
        ProjectileState& state = proj_cache_storage[it->second];
        state.lastSeen = now;

        float dx = px - mx, dy = py - my;
        if (dx * dx + dy * dy > zoneSq) continue;

        bool blob = !state.isBeam && !state.isIndirect && state.speed > 0.0f &&
                    state.speed < BLOB_SPEED_MAX;
        float speedNow = sqrtf(state.vx * state.vx + state.vy * state.vy);
        float ux = (blob || speedNow < SPEED_MIN) ? 0.0f : state.vx / speedNow;
        float uy = (blob || speedNow < SPEED_MIN) ? 0.0f : state.vy / speedNow;

        if (!blob && speedNow >= SPEED_MIN)
        {
            float playerAlong = (mx - px) * ux + (my - py) * uy;
            if (playerAlong < PLAYER_ALONG_MIN) continue;
            if (!state.isIndirect &&
                !wall_los(px,
                          py,
                          px + ux * (playerAlong > 0.0f ? playerAlong : 0.0f),
                          py + uy * (playerAlong > 0.0f ? playerAlong : 0.0f),
                          WALL_BLOCKS_PROJECTILES))
                continue;
        }
        threats.push_back({px,
                           py,
                           blob ? 0.0f : state.vx,
                           blob ? 0.0f : state.vy,
                           state.radius + myRadius});
    }

    {
        size_t writeIdx = 0;
        for (size_t i = 0; i < proj_cache_storage.size(); i++)
        {
            if (now - proj_cache_storage[i].lastSeen > 1024) continue;
            if (writeIdx != i)
                proj_cache_storage[writeIdx] = proj_cache_storage[i];
            writeIdx++;
        }
        proj_cache_storage.resize(writeIdx);
    }

    if (threats.empty() && bodies.empty())
    {
        if (now - lastDangerTs > TUNE.RELEASE_GRACE_MS) endDodge();
        return;
    }

    float stayClearSq = clearanceFor(threats, bodies, mx, my, 0.0f, 0.0f);
    bool inDanger = stayClearSq < (TUNE.ENGAGE * TUNE.ENGAGE);
    if (inDanger) lastDangerTs = now;

    if (!inDanger &&
        (headingIdx < 0 || now - lastDangerTs > TUNE.RELEASE_GRACE_MS))
    {
        endDodge();
        return;
    }

    bool hasPrev = commitDirX != 0.0f || commitDirY != 0.0f;
    float prevDirX = commitDirX, prevDirY = commitDirY;

    std::array<float, 256> dangerMap, interestMap;

    for (int i = 0; i < TUNE.DIR_COUNT; i++)
        dangerMap[i] = dangerFor(mx, my, ringX[i], ringY[i], CHAR_SPEED, bodyR);

    if (hasPrevDanger)
    {
        for (int i = 0; i < TUNE.DIR_COUNT; i++)
            dangerMap[i] = dangerMap[i] * DANGER_BLEND +
                           prevDangerMap[i] * (SPEED_MIN - DANGER_BLEND);
    }
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
        prevDangerMap[i] = dangerMap[i];
    hasPrevDanger = true;

    fillInterestMap(interestMap, mx, my, prevDirX, prevDirY);

    float minDanger = CLEAR_INF;
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
        if (dangerMap[i] < minDanger) minDanger = dangerMap[i];

    int bestIdx = -1;
    float bestScore = -CLEAR_INF;
    for (int i = 0; i < TUNE.DIR_COUNT; i++)
    {
        if (dangerMap[i] > minDanger + DANGER_PICK_EPS) continue;
        float score = interestMap[i];
        if (score > bestScore)
        {
            bestScore = score;
            bestIdx = i;
        }
    }

    if (bestIdx < 0)
    {
        bestScore = -CLEAR_INF;
        for (int i = 0; i < TUNE.DIR_COUNT; i++)
        {
            float score = interestMap[i] - dangerMap[i] * CLEAR_WEIGHT_MAX;
            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = i;
            }
        }
    }

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
        float smoothX =
            prevDirX * TUNE.SMOOTH + bestDirX * (SPEED_MIN - TUNE.SMOOTH);
        float smoothY =
            prevDirY * TUNE.SMOOTH + bestDirY * (SPEED_MIN - TUNE.SMOOTH);
        float len = sqrtf(smoothX * smoothX + smoothY * smoothY);
        if (len > LEN_EPS)
        {
            float nx = smoothX / len;
            float ny = smoothY / len;
            if (clearanceFor(
                    threats, bodies, mx, my, CHAR_SPEED * nx, CHAR_SPEED * ny) >
                (TUNE.ENGAGE * TUNE.ENGAGE))
            {
                outDirX = nx;
                outDirY = ny;
            }
        }
    }

    snapTo4Way(outDirX, outDirY);

    commitDirX = outDirX;
    commitDirY = outDirY;
    headingIdx = bestIdx;
    lastGoodTs = now;

    int32_t tx = clamp_axis(mx + outDirX * TUNE.REACH, wall_cache_w());
    int32_t ty = clamp_axis(my + outDirY * TUNE.REACH, wall_cache_h());
    int32_t ddx = tx - ad_target_x, ddy = ty - ad_target_y;

    if ((float)(ddx * ddx + ddy * ddy) > POS_CHANGE_MIN_SQ ||
        now - lastSentAt > TUNE.SEND_MIN_MS)
    {
        lastSentAt = now;
        enqueue(tx, ty);
        ad_target_x = tx;
        ad_target_y = ty;
    }
    active = 1;
    moving = 1;
}
