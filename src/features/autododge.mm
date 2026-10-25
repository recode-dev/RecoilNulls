#include "data.hpp"
#include "autododge.h"
#include "../utils/walls.h"
#include "../core/commands.h"

#include <math.h>
#include <string.h>

__thread int rcl_in_drive = 0;
int32_t rcl_pl_mine[12];
int rcl_active = 0;
int rcl_moving = 0;
int rcl_cand_now[3];
int rcl_cand_seen = 0;
int32_t rcl_prev_x = 0;
int32_t rcl_prev_y = 0;

#define AD_MAX_THREATS 16
#define AD_DIR_MAX 48
#define AD_RANGE_FALLBACK 2800.0f

typedef struct
{
    float x;
    float y;
} ad_dir_t;

typedef struct
{
    float x;
    float y;
    float vx;
    float vy;
    float rad;
    float pathLen;
    float left;
    float until;
} ad_threat_t;

static const float AD_FIT_GROW = 0.5f;
static const float AD_FIT_PAD = 80.0f;

static const float AD_AWARE = 3200.0f;
static const uint64_t AD_TICK_MS = 4;
static const uint64_t AD_TICK_MS_IDLE = 48;
static const float AD_SKIN = 50.0f;
static const uint64_t AD_LOCK_MS = 70;
static const float AD_REACH = 600.0f;
static const float AD_HORIZON_S = 1.4f;
static const float AD_KEEP_BAND = 100.0f;
static const float AD_MOMENTUM = 55.0f;
static const float AD_ENGAGE = 100.0f;
static const uint64_t AD_RELEASE_GRACE_MS = 200;
static const float AD_WALL_HIT = 9000.0f;
static const int AD_PROBE_N = 3;
static const float AD_PROBE_T = 0.35f;
static const float AD_WALL_BODY = 240.0f;
static const uint64_t AD_SEND_MIN_MS = 80;
static const float AD_SEND_MIN_DIST = 8.0f;
static const float AD_MIN_POS = 50.0f;
static const float AD_DEFAULT_SPEED = 720.0f;
static const float AD_DEFAULT_RADIUS = 60.0f;

typedef struct
{
    uintptr_t elem;
    int32_t x;
    int32_t y;
    int32_t px;
    int32_t py;
    int32_t spawnX;
    int32_t spawnY;
    int32_t gid;
    int32_t team;
    uint64_t ms;
    uint64_t pms;
    int hasPrev;
    float vx;
    float vy;
} ad_track_t;

static ad_track_t g_track[AD_MAX_THREATS];
static uintptr_t g_client = 0;
static uintptr_t g_manager = 0;

static ad_dir_t g_ring[AD_DIR_MAX];
static int g_dirCount = 0;
static int g_hasHeading = 0;
static ad_dir_t g_heading;
static int g_headingIdx = -1;
static uint64_t g_holdUntil = 0;
static uint64_t g_lastDangerTs = 0;
static uint64_t g_lastTick = 0;
static float g_lastSentX = 1e9f;
static float g_lastSentY = 1e9f;
static uint64_t g_lastSentAt = 0;
static uintptr_t g_lastOwn = 0;

static float ad_abs(float v)
{
    return v < 0.0f ? -v : v;
}

static float ad_hypot(float x, float y)
{
    return sqrtf(x * x + y * y);
}

static uint64_t ad_now(void)
{
    return (uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0);
}

static void ad_buildRing(void)
{
    int i = 0;

    g_dirCount = AD_DIR_MAX;

    for (i = 0; i < g_dirCount; i++)
    {
        float a = 3.14159265f * 2.0f * (float)i / (float)g_dirCount;

        g_ring[i].x = cosf(a);
        g_ring[i].y = sinf(a);
    }

    g_headingIdx = -1;
}

static void ad_clearHeading(void)
{
    g_hasHeading = 0;
    g_headingIdx = -1;
    g_holdUntil = 0;
}

static uintptr_t ad_read_ptr(uintptr_t addr)
{
    void *value = nullptr;

    if (!rcl_read_ptr(addr, &value))
    {
        return 0;
    }

    return (uintptr_t)value;
}

static int ad_valid_elem(uintptr_t elem)
{
    int32_t gid = 0;

    if (!rcl_pointer_plausible(elem))
    {
        return 0;
    }

    gid = rcl_gid(elem, nullptr);

    return gid > 0 && gid < 100000000;
}

static void ad_sync(uint64_t now)
{
    uintptr_t scene = 0;
    uintptr_t client = 0;
    uintptr_t manager = 0;
    uintptr_t array = 0;
    uintptr_t own = 0;
    int32_t state = -1;
    int32_t count = 0;
    int32_t ownIndex = -1;
    int32_t ownTeam = -1;
    uint8_t dead = 0;
    int seen[AD_MAX_THREATS];
    int i = 0;
    int k = 0;

    memset(seen, 0, sizeof(seen));

    scene = rcl_scene_object;
    if (!scene)
    {
        uintptr_t slot = (uintptr_t)rcl_read_global_ptr(RCL_STATE_RVA);

        if (slot && rcl_read_int(slot + RCL_STATE_ENUM_OFF, &state) && state == 5)
        {
            scene = ad_read_ptr(slot + RCL_SCENE_OFF);
        }
    }
    if (scene)
    {
        client = ad_read_ptr(scene + RCL_CLIENT_OFF);
    }
    if (!client && rcl_base)
    {
        uintptr_t mode = ((uintptr_t (*)(void))(rcl_base + RCL_GETBATTLE_RVA))();

        if (rcl_pointer_plausible(mode))
        {
            client = mode;
        }
    }
    if (!client)
    {
        return;
    }

    manager = ad_read_ptr(client + RCL_MGR_OFF);
    if (!manager)
    {
        manager = ad_read_ptr(client + RCL_MODE_MANAGER_OFF);
    }
    if (!manager)
    {
        return;
    }

    array = ad_read_ptr(manager + RCL_MGR_ARRAY_OFF);
    if (!array)
    {
        return;
    }
    if (!rcl_read_int(manager + RCL_MGR_COUNT_OFF, &count) || count <= 0 || count > 512)
    {
        return;
    }

    g_client = client;
    g_manager = manager;

    rcl_read_int(client + RCL_OWNIDX_OFF, &ownIndex);
    if (rcl_read_int(client + RCL_OWNTEAM_OFF, &ownTeam) && ownTeam >= 0 && ownTeam <= 15)
    {
        rcl_own_team_b = ownTeam;
    }

    if (ownIndex >= 0 && ownIndex < count)
    {
        uintptr_t elem = ad_read_ptr(array + (uintptr_t)ownIndex * 8ULL);

        if (elem && ad_valid_elem(elem))
        {
            own = elem;
        }
    }
    if (!own)
    {
        for (i = 0; i < count && i < 512; i++)
        {
            uintptr_t elem = ad_read_ptr(array + (uintptr_t)i * 8ULL);
            int32_t owner = -1;

            if (!elem || !ad_valid_elem(elem))
            {
                continue;
            }
            if (!rcl_read_int(elem + RCL_OBJ_OWNERINDEX_OFF, &owner))
            {
                continue;
            }
            if (owner == ownIndex)
            {
                own = elem;
                break;
            }
        }
    }
    if (!own)
    {
        return;
    }

    rcl_own_elem = own;
    if (rcl_read_byte(own + (uintptr_t)RCL_OBJ_DEADFLAG_OFF, &dead))
    {
        rcl_dead = dead ? 1 : 0;
    }

    for (i = 0; i < count && i < 512; i++)
    {
        uintptr_t elem = ad_read_ptr(array + (uintptr_t)i * 8ULL);
        int32_t gid = 0;
        int32_t x = 0;
        int32_t y = 0;
        int32_t team = -1;
        int slot = -1;

        if (!elem || !rcl_pointer_plausible(elem))
        {
            continue;
        }

        gid = rcl_gid(elem, nullptr);
        if (gid < 2000000)
        {
            continue;
        }
        if (!rcl_read_int(elem + RCL_OBJ_X_OFF, &x) || !rcl_read_int(elem + RCL_OBJ_Y_OFF, &y))
        {
            continue;
        }
        rcl_read_int(elem + RCL_OBJ_TEAM_OFF, &team);

        for (k = 0; k < AD_MAX_THREATS; k++)
        {
            if (g_track[k].elem == elem && g_track[k].gid != 0)
            {
                slot = k;
                break;
            }
        }
        if (slot < 0)
        {
            for (k = 0; k < AD_MAX_THREATS; k++)
            {
                if (g_track[k].gid == 0)
                {
                    slot = k;
                    break;
                }
            }
        }
        if (slot < 0)
        {
            continue;
        }

        if (g_track[slot].elem != elem || g_track[slot].gid == 0)
        {
            memset(&g_track[slot], 0, sizeof(g_track[slot]));
            g_track[slot].elem = elem;
            g_track[slot].spawnX = x;
            g_track[slot].spawnY = y;
            g_track[slot].px = x;
            g_track[slot].py = y;
            g_track[slot].pms = now;
            g_track[slot].ms = now;
            g_track[slot].hasPrev = 0;
        }
        else
        {
            g_track[slot].px = g_track[slot].x;
            g_track[slot].py = g_track[slot].y;
            g_track[slot].pms = g_track[slot].ms;
            g_track[slot].ms = now;
            g_track[slot].hasPrev = 1;

            if (now > g_track[slot].pms)
            {
                float dt = (float)(now - g_track[slot].pms);

                g_track[slot].vx = (float)(x - g_track[slot].px) * 1000.0f / dt;
                g_track[slot].vy = (float)(y - g_track[slot].py) * 1000.0f / dt;
            }
        }

        g_track[slot].x = x;
        g_track[slot].y = y;
        g_track[slot].gid = gid;
        g_track[slot].team = team;
        seen[slot] = 1;
    }

    for (i = 0; i < AD_MAX_THREATS; i++)
    {
        if (!seen[i])
        {
            g_track[i].gid = 0;
        }
    }
}

static int ad_collectLive(float myX, float myY, float myRadius, ad_threat_t *out, int cap)
{
    float zoneSq = AD_AWARE * AD_AWARE;
    float bodyR = myRadius + AD_SKIN;
    int count = 0;
    int i = 0;

    for (i = 0; i < AD_MAX_THREATS; i++)
    {
        const ad_track_t *p = &g_track[i];
        float dx = 0.0f;
        float dy = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float spd = 0.0f;
        float shotR = 100.0f;
        float rad = 0.0f;
        float ux = 0.0f;
        float uy = 0.0f;
        float playerAlong = 0.0f;
        float left = 0.0f;
        float gap = 0.0f;
        float traveled = 0.0f;

        if (!p->gid)
        {
            continue;
        }
        if (count >= cap)
        {
            break;
        }

        dx = (float)p->x - myX;
        dy = (float)p->y - myY;
        if (dx * dx + dy * dy > zoneSq)
        {
            continue;
        }

        vx = p->vx;
        vy = p->vy;
        spd = ad_hypot(vx, vy);

        rad = shotR + bodyR + AD_FIT_PAD + shotR * AD_FIT_GROW;

        if (spd >= 1.0f)
        {
            ux = vx / spd;
            uy = vy / spd;

            playerAlong = (myX - (float)p->x) * ux + (myY - (float)p->y) * uy;
            if (playerAlong < -50.0f)
            {
                continue;
            }

            {
                float hitX = (float)p->x + ux * (playerAlong > 0.0f ? playerAlong : 0.0f);
                float hitY = (float)p->y + uy * (playerAlong > 0.0f ? playerAlong : 0.0f);

                if (!rcl_wall_los((float)p->x, (float)p->y, hitX, hitY, RCL_WALL_BLOCKS_PROJECTILES))
                {
                    continue;
                }
            }
        }

        traveled = ad_hypot((float)p->x - (float)p->spawnX, (float)p->y - (float)p->spawnY);
        left = AD_RANGE_FALLBACK - traveled;
        if (left <= 10.0f)
        {
            continue;
        }

        gap = ad_hypot((float)p->x - myX, (float)p->y - myY) - bodyR - shotR;
        if (left < 0.85f * (gap > 0.0f ? gap : 0.0f))
        {
            continue;
        }

        if (spd >= 1.0f && playerAlong > left + shotR)
        {
            continue;
        }

        out[count].x = (float)p->x;
        out[count].y = (float)p->y;
        out[count].vx = vx;
        out[count].vy = vy;
        out[count].rad = rad;
        out[count].pathLen = left + shotR;
        out[count].left = left;
        out[count].until = 0.0f;
        count++;
    }

    return count;
}

static float ad_clearanceFor(const ad_threat_t *threats, int count, float myX, float myY, float mvx, float mvy)
{
    float minClear = 1e9f;
    int i = 0;

    for (i = 0; i < count; i++)
    {
        const ad_threat_t *t = &threats[i];
        float spd = ad_hypot(t->vx, t->vy);
        float maxT = AD_HORIZON_S;
        float relx = 0.0f;
        float rely = 0.0f;
        float vrx = 0.0f;
        float vry = 0.0f;
        float vv = 0.0f;
        float cx = 0.0f;
        float cy = 0.0f;
        float clear = 0.0f;

        if (spd > 1.0f)
        {
            if (t->left > 0.0f) maxT = t->left / spd < maxT ? t->left / spd : maxT;
            if (t->pathLen > 0.0f) maxT = t->pathLen / spd < maxT ? t->pathLen / spd : maxT;
        }
        if (t->until > 0.0f && t->until < maxT)
        {
            maxT = t->until;
        }

        relx = t->x - myX;
        rely = t->y - myY;
        vrx = t->vx - mvx;
        vry = t->vy - mvy;
        vv = vrx * vrx + vry * vry;

        if (vv < 1e-6f)
        {
            cx = relx;
            cy = rely;
        }
        else
        {
            float ts = -(relx * vrx + rely * vry) / vv;

            if (ts < 0.0f) ts = 0.0f;
            else if (ts > maxT) ts = maxT;

            cx = relx + vrx * ts;
            cy = rely + vry * ts;
        }

        clear = sqrtf(cx * cx + cy * cy) - t->rad;
        if (clear < minClear)
        {
            minClear = clear;
        }
    }

    return minClear;
}

static float ad_wallAhead(float myX, float myY, const ad_dir_t *dir, float speed, float bodyR)
{
    float step = speed * AD_PROBE_T;
    float raw = 0.0f;
    int s = 0;

    for (s = 1; s <= AD_PROBE_N; s++)
    {
        float px = myX + dir->x * step * (float)s;
        float py = myY + dir->y * step * (float)s;

        if (rcl_wall_is_blocked_wide(px, py, bodyR, RCL_WALL_BLOCKS_MOVEMENT))
        {
            raw += AD_WALL_HIT * (float)(AD_PROBE_N - s + 1);
        }
    }

    return raw;
}

static int ad_sendMove(float myX, float myY, float tx, float ty)
{
    if (!isfinite(tx) || !isfinite(ty))
    {
        return 0;
    }

    return rcl_move_to((int32_t)lroundf(tx), (int32_t)lroundf(ty), myX, myY) > 0;
}

void rcl_autododge_reset(void)
{
    ad_clearHeading();
    g_lastDangerTs = 0;
    g_lastTick = 0;
    g_lastSentX = 1e9f;
    g_lastSentY = 1e9f;
    g_lastSentAt = 0;
    g_lastOwn = 0;
}

void rcl_autododge(void)
{
    ad_threat_t hazards[AD_MAX_THREATS];
    uintptr_t own = 0;
    uint64_t now = ad_now();
    uint64_t tickMs = 0;
    int32_t myXi = 0;
    int32_t myYi = 0;
    float myX = 0.0f;
    float myY = 0.0f;
    float speed = AD_DEFAULT_SPEED;
    float myRadius = AD_DEFAULT_RADIUS;
    int hazardCount = 0;
    float stayClear = 0.0f;
    int inDanger = 0;
    int prevIdx = -1;
    ad_dir_t prevDir;
    int havePrev = 0;
    int bestIdx = 0;
    float bestScore = -1e9f;
    float prevScore = -1e9f;
    int chosenIdx = 0;
    float chosenScore = -1e9f;
    float tx = 0.0f;
    float ty = 0.0f;
    float ddx = 0.0f;
    float ddy = 0.0f;
    int moved = 0;
    int stale = 0;
    int i = 0;

    memset(&prevDir, 0, sizeof(prevDir));

    if (g_dirCount == 0)
    {
        ad_buildRing();
    }

    tickMs = g_hasHeading ? AD_TICK_MS : AD_TICK_MS_IDLE;
    if (now - g_lastTick < tickMs)
    {
        return;
    }
    g_lastTick = now;

    ad_sync(now);
    own = rcl_own_elem;

    if (!own || rcl_dead)
    {
        ad_clearHeading();
        return;
    }
    if (own != g_lastOwn)
    {
        g_lastOwn = own;
        ad_clearHeading();
    }

    if (!rcl_read_int(own + RCL_OBJ_X_OFF, &myXi) || !rcl_read_int(own + RCL_OBJ_Y_OFF, &myYi))
    {
        return;
    }

    myX = (float)myXi;
    myY = (float)myYi;

    if (ad_abs(myX) < AD_MIN_POS && ad_abs(myY) < AD_MIN_POS)
    {
        return;
    }

    hazardCount = ad_collectLive(myX, myY, myRadius, hazards, AD_MAX_THREATS);

    if (hazardCount == 0)
    {
        if (g_hasHeading && now - g_lastDangerTs > AD_RELEASE_GRACE_MS)
        {
            ad_clearHeading();
        }
        return;
    }

    stayClear = ad_clearanceFor(hazards, hazardCount, myX, myY, 0.0f, 0.0f);
    inDanger = stayClear < AD_ENGAGE;

    if (inDanger)
    {
        g_lastDangerTs = now;
    }

    if (!inDanger && (!g_hasHeading || now - g_lastDangerTs > AD_RELEASE_GRACE_MS))
    {
        if (g_hasHeading)
        {
            ad_clearHeading();
        }
        return;
    }

    if (g_hasHeading && g_headingIdx >= 0 && g_headingIdx < g_dirCount)
    {
        prevIdx = g_headingIdx;
        prevDir = g_ring[prevIdx];
        havePrev = 1;
    }

    for (i = 0; i < g_dirCount; i++)
    {
        const ad_dir_t *d = &g_ring[i];
        float s = ad_clearanceFor(hazards, hazardCount, myX, myY, speed * d->x, speed * d->y);

        s -= ad_wallAhead(myX, myY, d, speed, AD_WALL_BODY);

        if (havePrev)
        {
            s += AD_MOMENTUM * (d->x * prevDir.x + d->y * prevDir.y);
        }

        if (i == prevIdx)
        {
            prevScore = s;
        }
        if (s > bestScore)
        {
            bestScore = s;
            bestIdx = i;
        }
    }

    chosenIdx = bestIdx;
    chosenScore = bestScore;

    if (prevIdx >= 0 && now < g_holdUntil && chosenIdx != prevIdx)
    {
        if (prevScore + AD_KEEP_BAND >= bestScore)
        {
            chosenIdx = prevIdx;
            chosenScore = prevScore;
        }
        else
        {
            g_holdUntil = now + AD_LOCK_MS;
        }
    }
    else if (now >= g_holdUntil)
    {
        g_holdUntil = now + AD_LOCK_MS;
    }

    if (chosenScore <= stayClear)
    {
        if (g_hasHeading)
        {
            ad_clearHeading();
        }
        return;
    }

    g_headingIdx = chosenIdx;
    g_heading = g_ring[chosenIdx];
    g_hasHeading = 1;

    tx = roundf(myX + g_heading.x * AD_REACH);
    ty = roundf(myY + g_heading.y * AD_REACH);

    ddx = tx - g_lastSentX;
    ddy = ty - g_lastSentY;
    moved = (ddx * ddx + ddy * ddy) > (AD_SEND_MIN_DIST * AD_SEND_MIN_DIST);
    stale = (now - g_lastSentAt) > AD_SEND_MIN_MS;

    if (!moved && !stale)
    {
        return;
    }

    g_lastSentX = tx;
    g_lastSentY = ty;
    g_lastSentAt = now;

    ad_sendMove(myX, myY, tx, ty);
}
