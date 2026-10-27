#ifndef FEATURES_AUTODODGE_DODGE_CORE_H
#define FEATURES_AUTODODGE_DODGE_CORE_H
// Pure decision core for autododge: no game memory, no iOS headers.
// autododge.cpp feeds it threats and executes its intent; the offline
// simulation includes this exact file, so tuning measured there is the
// tuning that ships.
#include <stdint.h>
#include <cmath>
#define DODGE_RING_MAX 256
#define DODGE_INTENT_IDLE 0
#define DODGE_INTENT_MOVE 1
#define DODGE_INTENT_RELEASE 2
struct DodgeV2
{
    float x, y;
};
struct DodgeThreat
{
    float x, y;
    float vx, vy;
    float rad;
};
// Blocks a body of radius bodyR at (px, py)? Non-zero = blocked. The core
// probes the path itself so both the step and the gradient modes share one
// game-independent hook.
typedef int (*DodgeBlockedFn)(void* ctx,
                              float px,
                              float py,
                              float dx,
                              float dy,
                              float speed,
                              float bodyR);
struct DodgeTune
{
    float HORIZON_S;
    float MOMENTUM;
    float SWITCH_MARGIN;
    float SWITCH_MIN;
    float TURN_DOT;
    float SMOOTH;
    float ENGAGE;
    float RELEASE_GRACE_MS;
    float REACH;
    float WALL_HIT;
    int PROBE_N;
    float PROBE_T;
    float WALL_BODY;
    int DIR_COUNT;
    float SEND_MIN_MS;
    float SEND_TURN_DOT;
    // Anti-jitter knobs. Legacy values reproduce the pre-fix behaviour so the
    // simulation can measure the old and the new logic on identical worlds.
    uint32_t DECIDE_MS;    // 0 = decide on every call (legacy)
    float TURN_RATE;       // deg/s, <= 0 = instant turn (legacy)
    float DEADBAND_COS;    // 1 = no deadband (legacy)
    float RELEASE_HOLD_MS; // 0 = forget heading on release (legacy)
    int WALL_GRAD;         // 0 = blocked-count step (legacy)
    // A raw argmax that flickers between two headings one tick apart is what
    // becomes a visible micro-jerk: every flick past SEND_TURN_DOT emits a new
    // command. STABLE_N demands that a new winner survive a few consecutive
    // decisions before we abandon a committed heading, and URGENT_MS lets all
    // of these limits be bypassed when something is about to connect, so the
    // smoothness never costs survival.
    int STABLE_N;     // 0/1 = commit immediately (legacy)
    float URGENT_MS;  // 0 = never bypass (legacy)
    // Clearance of the best direction, below which no direction can get the
    // body out of the real collision radius: the shot has already landed.
    // 0 = disabled (legacy). Traced on the shipped build: every 150deg+ flip
    // happened in exactly this state, which is why the player sees a violent
    // turn the moment a hit registers.
    float DOOM_CLEAR;
};
struct DodgeState
{
    DodgeV2 commitDir;
    DodgeV2 prevDir;
    int engaged;
    int headingIdx;
    uint64_t lastDangerTs;
    uint64_t lastGoodTs;
    uint64_t lastDecideTs;
    uint64_t holdUntilTs;
    int rawIdx;
    int rawRun;
    float sentDirX;
    float sentDirY;
    uint64_t lastSentAt;
};
struct DodgeDecision
{
    int intent;
    float dirX;
    float dirY;
    int resend;
    int changed;
};
static DodgeV2 g_dodgeRing[DODGE_RING_MAX];
static int g_dodgeRingN = 0;
static void dodge_build_ring(int n)
{
    int i = 0;
    if (n < 1)
        n = 1;
    if (n > DODGE_RING_MAX)
        n = DODGE_RING_MAX;
    if (g_dodgeRingN == n)
        return;
    for (; i < n; i++)
    {
        float a = (float)M_PI * 2.0f * (float)i / (float)n;
        g_dodgeRing[i].x = cosf(a);
        g_dodgeRing[i].y = sinf(a);
    }
    g_dodgeRingN = n;
}
static void dodge_reset(DodgeState& st)
{
    st.commitDir = DodgeV2{0.0f, 0.0f};
    st.prevDir = DodgeV2{0.0f, 0.0f};
    st.engaged = 0;
    st.headingIdx = -1;
    st.lastDangerTs = 0;
    st.lastGoodTs = 0;
    st.lastDecideTs = 0;
    st.holdUntilTs = 0;
    st.rawIdx = -1;
    st.rawRun = 0;
    st.sentDirX = 0.0f;
    st.sentDirY = 0.0f;
    st.lastSentAt = 0;
}
static float dodge_clearance(const DodgeThreat* th,
                             int n,
                             float mx,
                             float my,
                             float mvx,
                             float mvy,
                             float horizon)
{
    float minClear = 1e18f;
    int i = 0;
    for (; i < n; i++)
    {
        const DodgeThreat& t = th[i];
        float relx = t.x - mx;
        float rely = t.y - my;
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
        if (clear < minClear)
            minClear = clear;
    }
    return minClear;
}
static float dodge_wall_cost(const DodgeTune& T,
                             DodgeBlockedFn wall,
                             void* ctx,
                             float mx,
                             float my,
                             float dx,
                             float dy,
                             float speed)
{
    float step = 0.0f;
    float dist = 0.0f;
    int s = 0;
    if (!wall)
        return 0.0f;
    step = speed * T.PROBE_T;
    if (step <= 0.0f)
        return 0.0f;
    for (s = 1; s <= T.PROBE_N; s++)
    {
        dist = step * (float)s;
        if (wall(ctx, mx + dx * dist, my + dy * dist, dx, dy, speed,
                 T.WALL_BODY))
        {
            if (!T.WALL_GRAD)
            {
                int blocked = 0;
                int k = 0;
                for (k = s; k <= T.PROBE_N; k++)
                {
                    float kd = step * (float)k;
                    if (wall(ctx, mx + dx * kd, my + dy * kd, dx, dy, speed,
                             T.WALL_BODY))
                        blocked++;
                }
                return T.WALL_HIT * (float)blocked / (float)T.PROBE_N;
            }
            // Continuous gradient: worst right at the wall, easing off with
            // distance. Keeps the argmax from flipping as the body slides.
            return T.WALL_HIT * (1.0f - (float)s / (float)(T.PROBE_N + 1));
        }
    }
    return 0.0f;
}
// Returns 1 when a live heading was actually dropped; 0 when nothing was
// engaged. coast receives the heading the body was travelling along, which is
// the direction to glide out of the fight in.
static int dodge_release(DodgeState& st,
                         const DodgeTune& T,
                         uint64_t now,
                         DodgeV2& coast)
{
    if (!st.engaged)
    {
        coast = st.commitDir;
        return 0;
    }
    coast = st.commitDir;
    st.engaged = 0;
    st.headingIdx = -1;
    if (T.RELEASE_HOLD_MS > 0.0f)
    {
        st.prevDir = st.commitDir;
        st.holdUntilTs = now + (uint64_t)T.RELEASE_HOLD_MS;
    }
    else
    {
        st.commitDir = DodgeV2{0.0f, 0.0f};
        st.prevDir = DodgeV2{0.0f, 0.0f};
        st.holdUntilTs = 0;
    }
    return 1;
}
// Release a live heading and publish the glide direction to the caller.
static void dodge_drop(DodgeState& st,
                       const DodgeTune& T,
                       uint64_t now,
                       DodgeDecision& out)
{
    DodgeV2 coast = DodgeV2{0.0f, 0.0f};
    if (dodge_release(st, T, now, coast))
    {
        out.intent = DODGE_INTENT_RELEASE;
        out.dirX = coast.x;
        out.dirY = coast.y;
        out.changed = 1;
    }
    else
    {
        out.intent = DODGE_INTENT_IDLE;
        out.dirX = coast.x;
        out.dirY = coast.y;
    }
}
// Is something going to connect soon unless we move now? Sampled along the
// heading the body is actually travelling on: a threat that closes to inside
// the defensive bubble in under URGENT_MS wins the right to an instant turn.
static int dodge_urgent(const DodgeThreat* th,
                        int n,
                        float mx,
                        float my,
                        float speed,
                        const DodgeV2& dir,
                        const DodgeTune& T)
{
    int i = 0;
    for (; i < n; i++)
    {
        const DodgeThreat& t = th[i];
        float relx = t.x - mx;
        float rely = t.y - my;
        float vrx = t.vx - speed * dir.x;
        float vry = t.vy - speed * dir.y;
        float vv = vrx * vrx + vry * vry;
        if (vv < 1e-6f)
            continue;
        float ts = -(relx * vrx + rely * vry) / vv;
        if (ts < 0.0f)
            ts = 0.0f;
        if (ts * 1000.0f >= T.URGENT_MS)
            continue;
        float cx = relx + vrx * ts;
        float cy = rely + vry * ts;
        float miss = sqrtf(cx * cx + cy * cy);
        if (miss < t.rad + T.ENGAGE * 0.4f)
            return 1;
    }
    return 0;
}
// Smoothing + deadband + turn-rate limit applied to the raw winner.
static DodgeV2 dodge_shape(const DodgeTune& T,
                           const DodgeV2& prev,
                           int hasPrev,
                           const DodgeV2& chosen,
                           float dt,
                           int urgent)
{
    DodgeV2 dir = chosen;
    if (hasPrev)
    {
        float dot = prev.x * chosen.x + prev.y * chosen.y;
        if (!urgent && T.DEADBAND_COS < 1.0f && dot >= T.DEADBAND_COS)
            return prev;
        if (T.SMOOTH > 0.0f && T.SMOOTH < 1.0f)
        {
            float bx = prev.x * T.SMOOTH + chosen.x * (1.0f - T.SMOOTH);
            float by = prev.y * T.SMOOTH + chosen.y * (1.0f - T.SMOOTH);
            float len = sqrtf(bx * bx + by * by);
            if (len > 0.001f)
            {
                dir.x = bx / len;
                dir.y = by / len;
            }
        }
        if (!urgent && T.TURN_RATE > 0.0f && dt > 0.0f)
        {
            float cross = prev.x * dir.y - prev.y * dir.x;
            float dot2 = prev.x * dir.x + prev.y * dir.y;
            float want = atan2f(cross, dot2);
            float limit = T.TURN_RATE * 0.017453292519943295f * dt;
            if (want > limit)
                want = limit;
            else if (want < -limit)
                want = -limit;
            float c = cosf(want), s = sinf(want);
            dir.x = prev.x * c - prev.y * s;
            dir.y = prev.x * s + prev.y * c;
        }
        float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
        if (len > 0.001f)
        {
            dir.x /= len;
            dir.y /= len;
        }
    }
    return dir;
}
static float dodge_norm(DodgeV2& v)
{
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len > 0.001f)
    {
        v.x /= len;
        v.y /= len;
    }
    return len;
}
static DodgeDecision dodge_decide(DodgeState& st,
                                  const DodgeTune& T,
                                  float mx,
                                  float my,
                                  float speed,
                                  const DodgeThreat* th,
                                  int n,
                                  uint64_t now,
                                  DodgeBlockedFn wall,
                                  void* wallCtx)
{
    DodgeDecision out;
    dodge_build_ring(T.DIR_COUNT);
    out.intent = st.engaged ? DODGE_INTENT_MOVE : DODGE_INTENT_IDLE;
    out.dirX = st.commitDir.x;
    out.dirY = st.commitDir.y;
    out.resend = 0;
    out.changed = 0;
    if (T.DECIDE_MS && st.lastDecideTs &&
        (now - st.lastDecideTs) < (uint64_t)T.DECIDE_MS)
    {
        return out;
    }
    uint64_t prevDecide = st.lastDecideTs;
    st.lastDecideTs = now;
    if (n == 0)
    {
        if (now - st.lastDangerTs > (uint64_t)T.RELEASE_GRACE_MS)
            dodge_drop(st, T, now, out);
        return out;
    }
    float stayClear = dodge_clearance(th, n, mx, my, 0.0f, 0.0f, T.HORIZON_S);
    bool inDanger = stayClear < T.ENGAGE;
    if (inDanger)
        st.lastDangerTs = now;
    if (!inDanger &&
        (!st.engaged ||
         now - st.lastDangerTs > (uint64_t)T.RELEASE_GRACE_MS))
    {
        dodge_drop(st, T, now, out);
        return out;
    }
    bool hasPrev = (st.commitDir.x != 0.0f || st.commitDir.y != 0.0f);
    if (hasPrev && T.RELEASE_HOLD_MS > 0.0f && !st.engaged &&
        now > st.holdUntilTs)
    {
        hasPrev = false;
    }
    DodgeV2 prevDir = st.commitDir;
    int bestIdx = 0;
    float bestScore = -1e18f;
    float prevScore = -1e18f;
    if (hasPrev)
    {
        prevScore = dodge_clearance(th,
                                    n,
                                    mx,
                                    my,
                                    speed * prevDir.x,
                                    speed * prevDir.y,
                                    T.HORIZON_S);
        prevScore -=
            dodge_wall_cost(T, wall, wallCtx, mx, my, prevDir.x, prevDir.y,
                            speed);
        prevScore += T.MOMENTUM;
    }
    // Best raw clearance over every heading, before the wall penalty and the
    // momentum bonus are mixed in. Feeds the doomed-hold test below.
    float bestRaw = -1e18f;
    for (int i = 0; i < T.DIR_COUNT; i++)
    {
        const DodgeV2& d = g_dodgeRing[i];
        float dotp = d.x * prevDir.x + d.y * prevDir.y;
        bool forward = dotp >= T.TURN_DOT;
        float s = 0.0f;
        s = dodge_clearance(th,
                            n,
                            mx,
                            my,
                            speed * d.x,
                            speed * d.y,
                            T.HORIZON_S);
        if (s > bestRaw)
            bestRaw = s;
        if (hasPrev && prevScore > 0.0f && !forward)
            continue;
        s -= dodge_wall_cost(T, wall, wallCtx, mx, my, d.x, d.y, speed);
        if (hasPrev)
            s += T.MOMENTUM * dotp;
        if (s > bestScore)
        {
            bestScore = s;
            bestIdx = i;
        }
    }
    // Doomed hold. Once every heading leaves the body inside the real collision
    // radius, the shot has already landed: steering is meaningless, but the
    // argmax is still free to swing 170deg, and that swing is exactly what the
    // player reports as "разворот после того как отдоджил" -- the character
    // spins around at the moment the hit registers. Keep flying the heading we
    // committed to and let the threat list expire normally.
    if (T.DOOM_CLEAR < 0.0f && bestRaw < T.DOOM_CLEAR)
    {
        if (hasPrev)
        {
            out.intent = DODGE_INTENT_MOVE;
            out.dirX = st.commitDir.x;
            out.dirY = st.commitDir.y;
        }
        return out;
    }
    DodgeV2 chosen = g_dodgeRing[bestIdx];
    float chosenScore = bestScore;
    int urgent =
        (T.URGENT_MS > 0.0f && hasPrev)
            ? dodge_urgent(th, n, mx, my, speed, prevDir, T)
            : 0;
    if (T.STABLE_N > 1)
    {
        if (bestIdx != st.rawIdx)
        {
            st.rawIdx = bestIdx;
            st.rawRun = 1;
        }
        else
        {
            st.rawRun++;
        }
        // Hold the committed heading until the new winner proves itself over a
        // few decisions. Single-tick argmax flicker never reaches the wire.
        if (!urgent && hasPrev && st.rawRun < T.STABLE_N)
        {
            chosen = prevDir;
            chosenScore = prevScore;
        }
    }
    if (hasPrev)
    {
        float dot = chosen.x * prevDir.x + chosen.y * prevDir.y;
        float margin = (prevScore > 0.0f)
                           ? T.SWITCH_MARGIN * (2.0f - dot)
                           : T.SWITCH_MIN;
        bool rescue = (prevScore <= 0.0f && bestScore > 0.0f);
        if (!rescue && bestScore <= prevScore + margin)
        {
            chosen = prevDir;
            chosenScore = prevScore;
        }
    }
    if (chosenScore <= stayClear)
    {
        if (now - st.lastGoodTs > (uint64_t)T.RELEASE_GRACE_MS)
            dodge_drop(st, T, now, out);
        return out;
    }
    float dt = prevDecide ? (float)(now - prevDecide) / 1000.0f : 0.04f;
    if (dt > 0.25f)
        dt = 0.25f;
    DodgeV2 dir = dodge_shape(T, prevDir, hasPrev, chosen, dt, urgent);
    if (hasPrev &&
        dodge_wall_cost(T, wall, wallCtx, mx, my, dir.x, dir.y, speed) > 0.0f)
    {
        dir = chosen;
    }
    dodge_norm(dir);
    st.commitDir = dir;
    st.headingIdx = bestIdx;
    st.engaged = 1;
    st.lastGoodTs = now;
    float sentDot = dir.x * st.sentDirX + dir.y * st.sentDirY;
    bool turned = (st.sentDirX == 0.0f && st.sentDirY == 0.0f) ||
                  (sentDot < T.SEND_TURN_DOT);
    bool stale = (now - st.lastSentAt > (uint64_t)T.SEND_MIN_MS);
    if (turned || stale)
    {
        st.sentDirX = dir.x;
        st.sentDirY = dir.y;
        st.lastSentAt = now;
        out.resend = 1;
        out.changed = 1;
    }
    out.intent = DODGE_INTENT_MOVE;
    out.dirX = dir.x;
    out.dirY = dir.y;
    return out;
}
#endif
