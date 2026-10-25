#include "../data.hpp"

#include <utility>
#include <vector>

#include "../utils/walls.h"

__thread int rcl_in_drive = 0;

int32_t rcl_pl_mine[12];

int rcl_active = 0;

int rcl_moving = 0;

int rcl_cand_now[3];

int rcl_cand_seen = 0;


#include <vector>
#include <optional>
#include <cmath>
#include <algorithm>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bs {

constexpr double TICK_HZ      = 20.0;
constexpr double DT           = 1.0 / TICK_HZ;
constexpr double TILE_UNITS   = 300.0;
constexpr double HITBOX_STD   = 120.0;
constexpr double HITBOX_LARGE = 145.0;
constexpr double JOY_DEADZONE = 0.06;
constexpr double INF          = std::numeric_limits<double>::infinity();

struct V2 {
    double x = 0.0, y = 0.0;
    V2() = default;
    V2(double x_, double y_) : x(x_), y(y_) {}
};

inline V2 operator-(const V2& a, const V2& b) { return {a.x - b.x, a.y - b.y}; }
inline V2 operator+(const V2& a, const V2& b) { return {a.x + b.x, a.y + b.y}; }
inline V2 operator*(const V2& a, double s) { return {a.x * s, a.y * s}; }
inline double dot(const V2& a, const V2& b) { return a.x * b.x + a.y * b.y; }
inline double cross(const V2& a, const V2& b) { return a.x * b.y - a.y * b.x; }
inline double len2(const V2& a) { return dot(a, a); }
inline double len(const V2& a) { return std::sqrt(len2(a)); }
inline V2 lerp(const V2& a, const V2& b, double t) { return a + (b - a) * t; }

inline V2 dirFromDeg(double deg) {
    const double r = deg * M_PI / 180.0;
    return {std::cos(r), std::sin(r)};
}

inline double normDeg(double d) {
    d = std::fmod(d, 360.0);
    return d < 0.0 ? d + 360.0 : d;
}

inline double angDiff(double a, double b) {
    const double d = std::fabs(a - b);
    return std::min(d, 360.0 - d);
}

struct Projectile {
    std::vector<V2> ticks;
    double damage       = 0.0;
    double radius       = 0.0;
    double areaRadius   = 0.0;
    int    areaTicks    = 0;
    int    deathTick    = -1;
    bool   indirect     = false;
    bool   piercesWalls = false;
    int    id           = 0;

    double vx = 0.0, vy = 0.0;
    double speed = 0.0;
    double castRange = 0.0;
    double spawnX = 0.0, spawnY = 0.0;
};

struct Wall {
    double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
};

struct Hit {
    int id = 0;
    double tick = 0.0;
};

struct Eval {
    double dmg     = 0.0;
    double first   = INF;
    double clear   = INF;
    double minimax = -INF;
    double wallPenalty = 0.0;
    std::vector<Hit> hits;
    bool blocked = false;
};

static std::optional<double> sweep(const V2& p0, const V2& p1, double rP,
                                   const V2& q0, const V2& q1, double rQ) {
    const double R = rP + rQ;
    const V2 r = p0 - q0;
    const V2 v = (p1 - p0) - (q1 - q0);
    const double c = len2(r) - R * R;
    if (c <= 0.0) return 0.0;
    const double a = len2(v);
    if (a < 1e-12) return std::nullopt;
    const double b = 2.0 * dot(r, v);
    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return std::nullopt;
    if (disc < 1e-9) disc = 0.0;
    const double s = std::sqrt(disc);
    const double enter = std::max(0.0, (-b - s) / (2.0 * a));
    const double exit  = std::min(1.0, (-b + s) / (2.0 * a));
    if (enter > exit) return std::nullopt;
    return enter;
}

static int clampTick(int v, int maxTick) {
    return std::max(0, std::min(v, maxTick));
}

static std::vector<V2> buildPath(const V2& start, double deg, int ticks,
                                 double speedUnitsPerSec, int inputDelayTicks) {
    const V2 d = dirFromDeg(deg);
    const double step = speedUnitsPerSec * DT;
    std::vector<V2> p(ticks + 1);
    p[0] = start;
    for (int i = 1; i <= ticks; ++i) {
        const int moveTicks = std::max(0, i - inputDelayTicks);
        p[i] = start + d * (step * static_cast<double>(moveTicks));
    }
    return p;
}

[[maybe_unused]] static V2 posAt(const std::vector<V2>& path, double t) {
    const int n = static_cast<int>(path.size());
    if (t <= 0.0) return path[0];
    if (t >= n - 1) return path[n - 1];
    const int i = static_cast<int>(std::floor(t));
    return lerp(path[i], path[i + 1], t - i);
}

static std::optional<double> hitTick(const std::vector<V2>& path,
                                     const Projectile& pr,
                                     double charRadius) {
    if (pr.ticks.empty() && pr.speed <= 0.0) return std::nullopt;

    std::vector<V2> ticks;
    if (!pr.ticks.empty()) {
        ticks = pr.ticks;
    } else {
        const int n = static_cast<int>(path.size());
        ticks.resize(n);
        const double spd = std::max(pr.speed, 1.0);
        const V2 dir = (len2({pr.vx, pr.vy}) > 1e-12) ? V2{pr.vx / spd, pr.vy / spd} : V2{1, 0};
        for (int i = 0; i < n; ++i) {
            const double dist = spd * DT * i;
            ticks[i] = V2{pr.spawnX + dir.x * dist, pr.spawnY + dir.y * dist};
        }
    }
    if (ticks.size() == 1) ticks.push_back(ticks[0]);

    const int maxTick = static_cast<int>(ticks.size()) - 1;
    const int death = clampTick(pr.deathTick >= 0 ? pr.deathTick : maxTick, maxTick);
    const double R = charRadius + pr.radius;
    const int last = static_cast<int>(path.size()) - 1;

    std::optional<double> best;

    if (!pr.indirect) {
        if (len2(path[0] - ticks[0]) <= R * R) best = 0.0;
        const int end = std::min(last, death);
        for (int k = 0; k < end; ++k) {
            auto t = sweep(path[k], path[k + 1], 0.0, ticks[k], ticks[k + 1], R);
            if (t) {
                const double ht = static_cast<double>(k) + *t;
                if (!best || ht < *best) best = ht;
                break;
            }
        }
    }

    if (pr.areaRadius > 0.0) {
        const double Ra = charRadius + pr.areaRadius;
        const double Ra2 = Ra * Ra;
        const int deathSafe = std::min(death, maxTick);
        const V2 center = ticks[deathSafe];
        const int at = std::max(0, pr.areaTicks);

        if (len2(path[deathSafe] - center) <= Ra2) {
            const double ht = static_cast<double>(deathSafe);
            if (!best || ht < *best) best = ht;
        }

        if (at > 0) {
            const int lo = deathSafe;
            const int hi = std::min(deathSafe + at, last);
            for (int k = lo; k < hi; ++k) {
                auto t = sweep(path[k], path[k + 1], 0.0, center, center, Ra);
                if (t) {
                    const double ht = static_cast<double>(k) + *t;
                    if (!best || ht < *best) best = ht;
                    break;
                }
            }
        }
    }
    return best;
}

static double clearanceAt(const V2& pos, const Projectile& pr, double charRadius) {
    if (pr.ticks.empty() && pr.speed <= 0.0) return INF;
    const double R = charRadius + pr.radius;
    const int maxTick = static_cast<int>(pr.ticks.size()) - 1;
    if (maxTick < 0) return INF;
    const int death = clampTick(pr.deathTick >= 0 ? pr.deathTick : maxTick, maxTick);

    double minClear = INF;
    for (int k = 0; k <= death; ++k) {
        const V2& q = pr.ticks[k];
        const double d = len(pos - q) - R;
        if (d < minClear) minClear = d;
    }
    return minClear;
}

static double clearanceContinuous(const std::vector<V2>& path,
                                  const Projectile& pr,
                                  double charRadius) {
    if (pr.ticks.size() < 2) return INF;
    const int maxTick = static_cast<int>(pr.ticks.size()) - 1;
    const int death = clampTick(pr.deathTick >= 0 ? pr.deathTick : maxTick, maxTick);
    const double R = charRadius + pr.radius;
    const int end = std::min(static_cast<int>(path.size()) - 1, death);
    double minClear = INF;

    for (int k = 0; k < end; ++k) {
        const V2 p0 = path[k], p1 = path[k + 1];
        const V2 q0 = pr.ticks[k], q1 = pr.ticks[k + 1];
        const V2 r = p0 - q0;
        const V2 v = (p1 - p0) - (q1 - q0);
        const double a = len2(v);
        double tStar = 0.0;
        if (a > 1e-12) {
            tStar = -dot(r, v) / a;
            tStar = std::max(0.0, std::min(1.0, tStar));
        }
        const V2 closest = lerp(p0, p1, tStar);
        const V2 projPos = lerp(q0, q1, tStar);
        const double d = len(closest - projPos) - R;
        if (d < minClear) minClear = d;
    }
    return minClear;
}

static double wallPenalty(const std::vector<V2>& path, const std::vector<Wall>& walls, double bodyR) {
    double penalty = 0.0;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        for (const auto& w : walls) {
            const double minX = std::min(path[i].x, path[i + 1].x) - bodyR;
            const double maxX = std::max(path[i].x, path[i + 1].x) + bodyR;
            const double minY = std::min(path[i].y, path[i + 1].y) - bodyR;
            const double maxY = std::max(path[i].y, path[i + 1].y) + bodyR;
            if (maxX < w.x || minX > w.x + w.w) continue;
            if (maxY < w.y || minY > w.y + w.h) continue;
            const double cx = std::max(w.x, std::min(path[i].x, w.x + w.w));
            const double cy = std::max(w.y, std::min(path[i].y, w.y + w.h));
            const double dist = std::sqrt((path[i].x - cx) * (path[i].x - cx) +
                                          (path[i].y - cy) * (path[i].y - cy));
            const double depth = std::max(0.0, bodyR - dist);
            penalty += depth * depth * 50.0;
        }
    }
    return penalty;
}

static Eval evalPath(const std::vector<V2>& path,
                     const std::vector<Projectile>& ps,
                     double charRadius,
                     const std::vector<Wall>& walls,
                     bool useFastClear) {
    Eval e;

    for (const auto& p : ps) {
        if (p.ticks.empty() && p.speed <= 0.0) continue;
        auto t = hitTick(path, p, charRadius);
        if (t) {
            e.dmg += p.damage;
            if (*t < e.first) e.first = *t;
            e.hits.push_back({p.id, *t});
        }
    }

    if (e.dmg == 0.0) {
        for (const auto& p : ps) {
            if (p.ticks.empty() && p.speed <= 0.0) continue;
            double c;
            if (useFastClear || p.ticks.size() < 2) {
                c = clearanceAt(path.empty() ? V2{0, 0} : path.back(), p, charRadius);
            } else {
                c = clearanceContinuous(path, p, charRadius);
            }
            if (c < e.clear) e.clear = c;
            if (c > e.minimax) e.minimax = c;
        }
    }

    e.wallPenalty = walls.empty() ? 0.0 : wallPenalty(path, walls, charRadius);

    if (e.wallPenalty > 0.0 && e.dmg == 0.0) {
        e.blocked = true;
    }

    return e;
}

static bool betterEval(const Eval& a, double angleA,
                       const Eval& b, double angleB,
                       double currentAngle) {
    if (a.dmg != b.dmg) return a.dmg < b.dmg;
    if (a.dmg == 0.0) {
        if (a.minimax != b.minimax) return a.minimax > b.minimax;
        if (a.clear != b.clear) return a.clear > b.clear;
    } else {
        if (a.first != b.first) return a.first > b.first;
    }
    if (a.wallPenalty != b.wallPenalty) return a.wallPenalty < b.wallPenalty;
    return angDiff(angleA, currentAngle) < angDiff(angleB, currentAngle);
}

static std::vector<double> candidateAngles(const std::vector<Projectile>& ps,
                                           const V2& pos,
                                           double currentAngle) {
    std::vector<double> out;
    out.reserve(64);
    out.push_back(normDeg(currentAngle));

    for (int a = 0; a < 360; a += 30)
        out.push_back(static_cast<double>(a));

    for (const auto& p : ps) {
        if (p.ticks.empty() && p.speed <= 0.0) continue;
        V2 t;
        if (!p.ticks.empty()) {
            const int idx = std::min(p.deathTick >= 0 ? p.deathTick : static_cast<int>(p.ticks.size()) - 1,
                                     static_cast<int>(p.ticks.size()) - 1);
            t = p.ticks[idx];
        } else {
            t = V2{p.spawnX, p.spawnY};
        }
        const double base = std::atan2(t.y - pos.y, t.x - pos.x) * 180.0 / M_PI;
        for (double off : {-20.0, -10.0, 0.0, 10.0, 20.0})
            out.push_back(normDeg(base + off));
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

struct DodgeResult {
    double angle = 0.0;
    Eval eval;
    bool deadzone = false;
};

static DodgeResult bestDodge(const V2& pos,
                             double charSpeed,
                             const std::vector<Projectile>& projectiles,
                             const std::vector<Wall>& walls = {},
                             double charRadius = HITBOX_STD,
                             int inputDelayTicks = 2,
                             double currentAngleDeg = 0.0) {
    std::vector<Projectile> ps;
    for (const auto& p : projectiles)
        if (!p.ticks.empty() || p.speed > 0.0) ps.push_back(p);

    if (ps.empty()) return {currentAngleDeg, {}, true};

    int maxT = 0;
    for (const auto& p : ps) {
        int idx;
        if (!p.ticks.empty()) {
            idx = std::min(p.deathTick >= 0 ? p.deathTick : static_cast<int>(p.ticks.size()) - 1,
                           static_cast<int>(p.ticks.size()) - 1);
        } else {
            idx = static_cast<int>(p.castRange / (std::max(p.speed, 1.0) * DT));
        }
        maxT = std::max(maxT, idx + std::max(0, p.areaTicks));
    }
    const int totalTicks = std::min(maxT + inputDelayTicks + 1, 48);

    auto evalAt = [&](double deg, bool fastClear) -> std::pair<Eval, double> {
        const double d = normDeg(deg);
        auto path = buildPath(pos, d, totalTicks, charSpeed, inputDelayTicks);
        Eval e = evalPath(path, ps, charRadius, walls, fastClear);
        return {e, d};
    };

    auto cands = candidateAngles(ps, pos, currentAngleDeg);
    std::vector<std::pair<Eval, double>> scored;
    scored.reserve(cands.size());

    for (double a : cands)
        scored.push_back(evalAt(a, true));

    std::sort(scored.begin(), scored.end(), [&](const auto& a, const auto& b) {
        return betterEval(a.first, a.second, b.first, b.second, currentAngleDeg);
    });

    const int topN = std::min(5, static_cast<int>(scored.size()));
    for (int i = 0; i < topN; ++i) {
        scored[i] = evalAt(scored[i].second, false);
    }

    std::sort(scored.begin(), scored.begin() + topN, [&](const auto& a, const auto& b) {
        return betterEval(a.first, a.second, b.first, b.second, currentAngleDeg);
    });

    std::pair<Eval, double> best = scored.front();

    for (int s = 0; s < std::min(3, topN); ++s) {
        auto loc = scored[s];
        double radius = 2.0;
        for (double step : {2.0, 0.5, 0.1, 0.02}) {
            const int n = std::max(1, static_cast<int>(std::round((2.0 * radius) / step)));
            for (int i = 0; i <= n; ++i) {
                auto c = evalAt(loc.second - radius + i * step, false);
                if (betterEval(c.first, c.second, loc.first, loc.second, currentAngleDeg))
                    loc = c;
            }
            radius = step * 2.0;
        }
        if (betterEval(loc.first, loc.second, best.first, best.second, currentAngleDeg))
            best = loc;
    }

    const bool deadzone =
        (angDiff(best.second, currentAngleDeg) / 360.0 < JOY_DEADZONE) &&
        (best.first.dmg == 0.0) &&
        (best.first.wallPenalty == 0.0);

    if (deadzone) {
        auto exact = evalAt(currentAngleDeg, false);
        return {currentAngleDeg, exact.first, true};
    }

    return {best.second, best.first, false};
}

}

namespace
{
    constexpr int RCL_BS_TICKS = 30;
    constexpr int RCL_BS_DELAY_TICKS = 2;
    constexpr int RCL_BS_MAX_PROJ = 6;
    constexpr int RCL_BS_MAX_WALLS = 40;
    constexpr float RCL_BS_PROJ_ZONE = 2200.0f;
    constexpr float RCL_BS_WALL_ZONE = 900.0f;
    constexpr float RCL_BS_MIN_RADIUS = 45.0f;
    constexpr double RCL_BS_REACH = 800.0;

    // the core is heavy and the battle tick is 20 Hz: 30 Hz is plenty
    constexpr uint64_t RCL_BS_PERIOD_MS = 33;

    double g_angle = 0.0;
    int g_have_angle = 0;
    int g_solved = 0;
    int g_was_active = 0;
    uint64_t g_last_solve_ms = 0;
    uintptr_t g_tilemap_fn = 0;
    int g_tilemap_tried = 0;
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

    void rcl_bs_bind_map(void)
    {
        uintptr_t client = 0;
        uintptr_t map = 0;

        if (!g_tilemap_fn && !g_tilemap_tried)
        {
            g_tilemap_tried = 1;
            g_tilemap_fn = rcl_entry_callable(RCL_TILEMAP_GET_RVA);
        }
        if (!g_tilemap_fn)
        {
            return;
        }

        client = rcl_controller();
        if (!client)
        {
            return;
        }

        map = ((uintptr_t (*)(uintptr_t))g_tilemap_fn)(client);
        if (map && ((map & 7ULL) == 0) && rcl_pointer_plausible(map))
        {
            rcl_wall_bind(map);
        }
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
        q.areaRadius = (p->spawnAreaRadius > 0) ? (double)p->spawnAreaRadius : 0.0;
        q.areaTicks = (p->spawnAreaRadius > 0 && p->spawnAreaActiveTime > 0)
                          ? (p->spawnAreaActiveTime / 50)
                          : 0;
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

static std::vector<bs::Wall> rcl_bs_walls(float mx, float my)
{
    float raw[RCL_BS_MAX_WALLS * 4];
    std::vector<bs::Wall> out;
    int n = 0;

    rcl_bs_bind_map();

    n = rcl_walls_near(mx, my, RCL_BS_WALL_ZONE, raw, RCL_BS_MAX_WALLS);
    out.reserve((size_t)(n > 0 ? n : 0));

    for (int i = 0; i < n; i++)
    {
        bs::Wall w;

        w.x = (double)raw[i * 4 + 0];
        w.y = (double)raw[i * 4 + 1];
        w.w = (double)raw[i * 4 + 2];
        w.h = (double)raw[i * 4 + 3];
        out.push_back(w);
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

    g_solved = 1;

    if (!rcl_manager_ptr || rcl_manager_count_v <= 0)
    {
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "no manager");
        rcl_active = 0;
        g_last_dbg = rcl_dbg;
        return;
    }

    {
        rcl_obj_t objects[64];
        int usable = rcl_collect(rcl_manager_ptr, objects, 64);

        if (usable > 0)
        {
            rcl_roster(rcl_own_elem, rcl_own_index_v, rcl_own_team_b, objects, usable);
        }

        rcl_proj_scan(rcl_manager_ptr, rcl_manager_count_v, objects, usable);
    }

    {
        std::vector<bs::Projectile> ps = rcl_bs_projectiles(mx, my);
        std::vector<bs::Wall> walls = rcl_bs_walls(mx, my);
        bs::DodgeResult res;

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

        res = bs::bestDodge(bs::V2((double)mx, (double)my), speed, ps, walls, (double)radius,
                            RCL_BS_DELAY_TICKS, g_have_angle ? g_angle : 0.0);

        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note),
                 "dmg=%.0f first=%.1f clear=%.0f wall=%.0f nw=%d", res.eval.dmg, res.eval.first,
                 res.eval.clear, res.eval.wallPenalty, (int)walls.size());

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
