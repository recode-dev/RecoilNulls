#include "../data.hpp"

#include <utility>
#include <vector>

#include "dodge_core.hpp"

__thread int rcl_in_drive = 0;

int32_t rcl_pl_mine[12];

int rcl_active = 0;

int rcl_moving = 0;

int rcl_cand_now[3];

int rcl_cand_seen = 0;

namespace
{
    constexpr int RCL_BS_TICKS = 30;
    constexpr int RCL_BS_DELAY_TICKS = 2;
    constexpr int RCL_BS_MAX_PROJ = 6;
    constexpr float RCL_BS_PROJ_ZONE = 2200.0f;
    constexpr float RCL_BS_MIN_RADIUS = 45.0f;
    constexpr double RCL_BS_REACH = 800.0;

    // the core is heavy and the battle tick is 20 Hz: 30 Hz is plenty
    constexpr uint64_t RCL_BS_PERIOD_MS = 33;

    double g_angle = 0.0;
    int g_have_angle = 0;
    int g_solved = 0;
    int g_was_active = 0;
    uint64_t g_last_solve_ms = 0;
    rcl_dbg_t g_last_dbg;

    void rcl_bs_push(float mx, float my, double angleDeg)
    {
        double rad = angleDeg * M_PI / 180.0;
        double tx = round((double)mx + cos(rad) * RCL_BS_REACH);
        double ty = round((double)my + sin(rad) * RCL_BS_REACH);

        rcl_dbg.dx = (float)cos(rad);
        rcl_dbg.dy = (float)sin(rad);
        rcl_dbg.tx = (float)tx;
        rcl_dbg.ty = (float)ty;

        rcl_move_to((int32_t)tx, (int32_t)ty, mx, my);
    }
}

float rcl_own_radius(void)
{
    if (rcl_own_r > 1.0f)
    {
        float r = rcl_own_r;

        if (r > RCL_OWN_RADIUS_MAX) r = RCL_OWN_RADIUS_MAX;
        if (r < RCL_OWN_RADIUS_MIN) r = RCL_OWN_RADIUS_MIN;

        return r;
    }

    return (float)bs::HITBOX_STD;
}

static std::vector<bs::Projectile> rcl_bs_projectiles(float mx, float my)
{
    std::vector<bs::Projectile> out;

    out.reserve(RCL_BS_MAX_PROJ);

    for (int i = 0; i < 16; i++)
    {
        const rcl_proj_t *p = &rcl_projs[i];
        float dx = 0.0f;
        float dy = 0.0f;
        float spd = 0.0f;
        bs::Projectile q;

        if (!p->elem) continue;
        if (p->team >= 0 && p->team == rcl_own_team_b) continue;

        dx = (float)p->x - mx;
        dy = (float)p->y - my;
        if (dx * dx + dy * dy > RCL_BS_PROJ_ZONE * RCL_BS_PROJ_ZONE) continue;

        spd = sqrtf(p->vx * p->vx + p->vy * p->vy);
        if (spd < 1.0f) continue;

        q.id = i;
        q.radius = (p->radius > RCL_BS_MIN_RADIUS) ? (double)p->radius : (double)RCL_BS_MIN_RADIUS;
        q.indirect = (p->isThrower != 0);
        q.speed = (double)spd;
        q.vx = (double)p->vx;
        q.vy = (double)p->vy;
        q.spawnX = (double)p->spawnX;
        q.spawnY = (double)p->spawnY;
        q.castRange = 0.0;
        q.areaRadius = 0.0;
        q.areaTicks = 0;
        q.deathTick = -1;

        q.ticks.resize(RCL_BS_TICKS + 1);
        for (int k = 0; k <= RCL_BS_TICKS; k++)
        {
            q.ticks[k] = bs::V2((double)p->x + (double)p->vx * bs::DT * (double)k,
                                (double)p->y + (double)p->vy * bs::DT * (double)k);
        }

        out.push_back(std::move(q));

        if ((int)out.size() >= RCL_BS_MAX_PROJ) break;
    }

    return out;
}

void rcl_autododge(void)
{
    uint64_t now = (uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0);
    float mx = 0.0f;
    float my = 0.0f;
    float radius = 0.0f;
    double speed = 0.0;
    double angle = 0.0;
    int sent = 0;

    if (!rcl_base) return;

    /* the solver is expensive and the battle tick is 20 Hz: solve on a fixed cadence */
    if (g_solved && now > g_last_solve_ms && (now - g_last_solve_ms) < RCL_BS_PERIOD_MS)
    {
        rcl_dbg = g_last_dbg;
        return;
    }
    g_last_solve_ms = now;

    rcl_dbg.hazards = 0;
    rcl_dbg.threats = 0;
    rcl_dbg.danger = 0;
    rcl_dbg.dirs = 0;
    rcl_dbg.sent = 0;
    rcl_dbg.thn = 0;
    rcl_dbg.note[0] = 0;
    g_last_dbg = rcl_dbg;

    if (!rcl_own_elem)
    {
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "no own elem");
        rcl_active = 0;
        g_last_dbg = rcl_dbg;
        return;
    }
    if (rcl_dead)
    {
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "dead");
        rcl_active = 0;
        g_last_dbg = rcl_dbg;
        return;
    }

    mx = (float)rcl_own_x;
    my = (float)rcl_own_y;

    if (mx == 0.0f && my == 0.0f)
    {
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "no own pos");
        rcl_active = 0;
        g_last_dbg = rcl_dbg;
        return;
    }

    radius = rcl_own_radius();
    speed = (double)rcl_own_speed;
    if (speed < 1.0) speed = 720.0;

    rcl_dbg.px = mx;
    rcl_dbg.py = my;
    rcl_dbg.radius = radius;
    rcl_dbg.speed = (float)speed;

    {
        std::vector<bs::Projectile> ps = rcl_bs_projectiles(mx, my);
        bs::DodgeResult res;

        g_solved = 1;
        rcl_dbg.hazards = (int)ps.size();
        rcl_dbg.threats = (int)ps.size();

        if (ps.empty())
        {
            snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "no projectiles");
            rcl_active = 0;
            rcl_moving = 0;
            g_have_angle = 0;
            g_was_active = 0;
            g_last_dbg = rcl_dbg;

            return;
        }

        res = bs::bestDodge(bs::V2((double)mx, (double)my), speed, ps, std::vector<bs::Wall>(),
                            (double)radius, RCL_BS_DELAY_TICKS, g_have_angle ? g_angle : 0.0);

        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "dmg=%.0f first=%.1f clear=%.0f mini=%.0f",
                 res.eval.dmg, res.eval.first, res.eval.clear, res.eval.minimax);

        rcl_dbg.ix = (float)res.eval.dmg;
        rcl_dbg.iy = (float)res.eval.first;

        angle = res.angle;
        g_angle = angle;
        g_have_angle = 1;

        if (res.deadzone)
        {
            /* release the prediction target once, then leave the stick alone */
            if (g_was_active)
            {
                rcl_move_to((int32_t)mx, (int32_t)my, mx, my);
                (void)rcl_enqueue((int32_t)mx, (int32_t)my);
                g_was_active = 0;
            }

            rcl_active = 1;
            rcl_moving = 0;
            rcl_dbg.danger = 0;
            rcl_dbg.sent = 0;
            g_last_dbg = rcl_dbg;

            return;
        }

        g_was_active = 1;
        rcl_dbg.danger = 1;

        rcl_bs_push(mx, my, angle);
        rcl_dbg.dirs = 1;

        {
            int32_t tx = (int32_t)round((double)mx + cos(angle * M_PI / 180.0) * RCL_BS_REACH);
            int32_t ty = (int32_t)round((double)my + sin(angle * M_PI / 180.0) * RCL_BS_REACH);

            sent = rcl_enqueue(tx, ty);
        }

        rcl_active = 1;
        rcl_moving = sent;
        rcl_dbg.sent = sent;
        g_last_dbg = rcl_dbg;
    }
}

void rcl_autododge_reset(void)
{
    g_angle = 0.0;
    g_have_angle = 0;
    g_solved = 0;
    g_was_active = 0;
    g_last_solve_ms = 0;
    rcl_active = 0;
    rcl_moving = 0;
}
