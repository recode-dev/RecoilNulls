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

static float ad_ballR(const rcl_proj_t *p)
{
    return p->radius > 0.0f ? p->radius : 100.0f;
}

static float ad_traveled(const rcl_proj_t *p)
{
    return ad_hypot((float)p->x - (float)p->spawnX, (float)p->y - (float)p->spawnY);
}

static float ad_lifeLeft(const rcl_proj_t *p)
{
    float maxRange = p->castRange > 0 ? (float)p->castRange : AD_RANGE_FALLBACK;
    float left = maxRange - ad_traveled(p);

    return left > 0.0f ? left : 0.0f;
}

static int ad_collectLive(float myX, float myY, float myRadius, ad_threat_t *out, int cap)
{
    float zoneSq = AD_AWARE * AD_AWARE;
    float bodyR = myRadius + AD_SKIN;
    int count = 0;
    int i = 0;

    for (i = 0; i < 16; i++)
    {
        const rcl_proj_t *p = &rcl_projs[i];
        float dx = 0.0f;
        float dy = 0.0f;
        int indirect = 0;
        float vx = 0.0f;
        float vy = 0.0f;
        float spd = 0.0f;
        int blob = 0;
        float shotR = 0.0f;
        float rad = 0.0f;
        float ux = 0.0f;
        float uy = 0.0f;
        int skipAlong = 0;
        float playerAlong = 0.0f;
        float left = 0.0f;
        float gap = 0.0f;

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

        indirect = p->isThrower != 0;
        vx = p->vx;
        vy = p->vy;
        spd = ad_hypot(vx, vy);
        blob = !p->isBeam && !indirect && spd > 0.0f && spd < 1600.0f;

        shotR = ad_ballR(p);
        rad = shotR + bodyR + AD_FIT_PAD + shotR * AD_FIT_GROW;

        if (spd >= 1.0f)
        {
            ux = vx / spd;
            uy = vy / spd;
        }

        skipAlong = blob || indirect;

        if (!skipAlong && spd >= 1.0f)
        {
            float hitX = 0.0f;
            float hitY = 0.0f;

            playerAlong = (myX - (float)p->x) * ux + (myY - (float)p->y) * uy;
            if (playerAlong < -50.0f)
            {
                continue;
            }

            hitX = (float)p->x + ux * (playerAlong > 0.0f ? playerAlong : 0.0f);
            hitY = (float)p->y + uy * (playerAlong > 0.0f ? playerAlong : 0.0f);

            if (!rcl_wall_los((float)p->x, (float)p->y, hitX, hitY, RCL_WALL_BLOCKS_PROJECTILES))
            {
                continue;
            }
        }

        left = ad_lifeLeft(p);
        if (left <= 10.0f)
        {
            continue;
        }

        gap = ad_hypot((float)p->x - myX, (float)p->y - myY) - bodyR - shotR;
        if (left < 0.85f * (gap > 0.0f ? gap : 0.0f))
        {
            continue;
        }

        if (!skipAlong && spd >= 1.0f && playerAlong > left + shotR)
        {
            continue;
        }

        out[count].x = (float)p->x;
        out[count].y = (float)p->y;
        out[count].vx = blob ? 0.0f : vx;
        out[count].vy = blob ? 0.0f : vy;
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
    uintptr_t own = rcl_own_elem;
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

    tickMs = g_hasHeading ? AD_TICK_MS : AD_TICK_MS_IDLE;
    if (now - g_lastTick < tickMs)
    {
        return;
    }
    g_lastTick = now;

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
