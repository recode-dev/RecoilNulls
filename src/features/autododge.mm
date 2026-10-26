#include "../data.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "../utils/walls.h"

__thread int rcl_in_drive = 0;

int32_t rcl_pl_mine[12];

int rcl_active = 0;

int rcl_moving = 0;

int rcl_cand_now[3];

int rcl_cand_seen = 0;

namespace bs {

constexpr double TICK_HZ      = 20.0;
constexpr double DT           = 1.0 / TICK_HZ;
[[maybe_unused]] constexpr double TILE_UNITS   = 300.0;
constexpr double HITBOX_STD   = 120.0;
[[maybe_unused]] constexpr double HITBOX_LARGE = 145.0;
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

[[maybe_unused]] static int clampTick(int v, int maxTick) {
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

[[maybe_unused]] static double clearanceAt(const V2& pos, const Projectile& pr, double charRadius) {
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

[[maybe_unused]] static double wallPenalty(const std::vector<V2>& path, const std::vector<Wall>& walls, double bodyR) {
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
    constexpr int AD_DIRS = 64;
    constexpr float AD_SAFETY = 28.0f;
    constexpr float AD_T_URGENT = 0.9f;
    constexpr float AD_T_FIELD = 1.8f;
    constexpr float AD_FIELD_RADIUS = 275.0f;
    constexpr float AD_PERP = 2.4f;
    constexpr float AD_AWAY = 1.0f;
    constexpr float AD_INTENT = 1.2f;
    constexpr float AD_CHAR_SPEED = 720.0f;
    constexpr float AD_MAX_DIST = 5000.0f;
    constexpr uint64_t AD_PERIOD_MS = 20;

    struct ad_proj
    {
        float x;
        float y;
        float dx;
        float dy;
        float speed;
        float radius;
    };

    ad_proj g_ad[16];
    int g_ad_n = 0;

    float g_dirx[AD_DIRS];
    float g_diry[AD_DIRS];
    int g_dir_init = 0;

    float g_last_dir_x = 0.0f;
    float g_last_dir_y = 0.0f;
    uint64_t g_last_dir_ms = 0;

    uint64_t g_ad_solve_ms = 0;
    int g_ad_active = 0;

    void ad_dirs_init(void)
    {
        if (g_dir_init) return;
        for (int i = 0; i < AD_DIRS; i++)
        {
            float a = (float)(M_PI * 2.0 * (double)i / (double)AD_DIRS);

            g_dirx[i] = cosf(a);
            g_diry[i] = sinf(a);
        }
        g_dir_init = 1;
    }

    void ad_norm(float x, float y, float *ox, float *oy)
    {
        float len = sqrtf(x * x + y * y);

        if (len < 1e-6f)
        {
            *ox = 1.0f;
            *oy = 0.0f;
            return;
        }
        *ox = x / len;
        *oy = y / len;
    }

    int ad_check_danger(float mx, float my, float mr)
    {
        for (int i = 0; i < g_ad_n; i++)
        {
            const ad_proj *p = &g_ad[i];
            float r = mr + p->radius + AD_SAFETY * 2.5f;
            float dx = mx - p->x;
            float dy = my - p->y;
            float vx = p->dx * p->speed;
            float vy = p->dy * p->speed;
            float c1 = 0.0f;
            float c2 = 0.0f;

            if (dx * dx + dy * dy <= r * r) return 1;

            c1 = dx * vx + dy * vy;
            if (c1 <= 0.0f) continue;

            c2 = vx * vx + vy * vy;
            if (c2 <= 0.0f) continue;

            {
                float t = c1 / c2;

                if (t <= AD_T_FIELD)
                {
                    float cxp = p->x + vx * t;
                    float cyp = p->y + vy * t;
                    float ex = mx - cxp;
                    float ey = my - cyp;

                    if (ex * ex + ey * ey <= r * r) return 1;
                }
            }
        }
        return 0;
    }

    float ad_threat_score(float dx, float dy, float mx, float my, float mr, float ix, float iy)
    {
        float score = 0.0f;

        for (int i = 0; i < g_ad_n; i++)
        {
            const ad_proj *p = &g_ad[i];
            float r = mr + p->radius + AD_SAFETY;
            float vx = p->dx * p->speed - dx * AD_CHAR_SPEED;
            float vy = p->dy * p->speed - dy * AD_CHAR_SPEED;
            float ex = p->x - mx;
            float ey = p->y - my;
            float a = vx * vx + vy * vy;
            float b = 2.0f * (ex * vx + ey * vy);
            float c = ex * ex + ey * ey;
            float minD2 = c;
            float danger = 0.0f;

            if (a > 1e-6f)
            {
                float t = -b / (2.0f * a);

                if (t > 0.0f && t <= AD_T_FIELD) minD2 = c + b * t + a * t * t;
                else if (t > AD_T_FIELD) minD2 = c + b * AD_T_FIELD + a * AD_T_FIELD * AD_T_FIELD;
            }
            if (minD2 < 0.0f) minD2 = 0.0f;

            danger = (minD2 < r * r) ? 2500.0f : (r * r) / (minD2 > 50.0f ? minD2 : 50.0f);
            score += danger;
        }

        if (ix != 0.0f || iy != 0.0f) score -= (dx * ix + dy * iy) * AD_INTENT * 15.0f;

        if (g_last_dir_ms && (uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0) - g_last_dir_ms < 400)
        {
            score -= (dx * g_last_dir_x + dy * g_last_dir_y) * 45.0f;
        }

        return score;
    }

    int ad_vel_unsafe(float dx, float dy, float mx, float my, float mr)
    {
        for (int i = 0; i < g_ad_n; i++)
        {
            const ad_proj *p = &g_ad[i];
            float r = mr + p->radius + AD_SAFETY;
            float vx = p->dx * p->speed - dx * AD_CHAR_SPEED;
            float vy = p->dy * p->speed - dy * AD_CHAR_SPEED;
            float ex = p->x - mx;
            float ey = p->y - my;
            float a = vx * vx + vy * vy;
            float b = 2.0f * (ex * vx + ey * vy);
            float c = ex * ex + ey * ey - r * r;
            float disc = 0.0f;

            if (c < 0.0f) return 1;
            if (a <= 1e-6f) continue;

            disc = b * b - 4.0f * a * c;
            if (disc < 0.0f) continue;

            {
                float t1 = (-b - sqrtf(disc)) / (2.0f * a);

                if (t1 > 0.0f && t1 <= AD_T_FIELD) return 1;
            }
        }
        return 0;
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

static void ad_collect(float mx, float my)
{
    g_ad_n = 0;

    for (int i = 0; i < 16; i++)
    {
        const rcl_proj_t *p = &rcl_projs[i];
        float dx = 0.0f;
        float dy = 0.0f;
        float len = 0.0f;
        float ex = 0.0f;
        float ey = 0.0f;

        if (!p->elem) continue;
        if (p->team >= 0 && p->team == rcl_own_team_b) continue;
        {
            int32_t deadFlag = 0;

            if (rcl_read_int(p->elem + (uintptr_t)RCL_PROJ_DEAD_OFF, &deadFlag) && deadFlag != 0)
            {
                continue;
            }
        }

        ex = (float)p->x - mx;
        ey = (float)p->y - my;
        if (ex * ex + ey * ey > AD_MAX_DIST * AD_MAX_DIST) continue;

        len = sqrtf(p->vx * p->vx + p->vy * p->vy);
        if (len < 1.0f) continue;

        dx = p->vx / len;
        dy = p->vy / len;

        g_ad[g_ad_n].x = (float)p->x;
        g_ad[g_ad_n].y = (float)p->y;
        g_ad[g_ad_n].dx = dx;
        g_ad[g_ad_n].dy = dy;
        g_ad[g_ad_n].speed = len;
        g_ad[g_ad_n].radius = (p->radius > 20.0f) ? p->radius : 45.0f;
        g_ad_n++;

        if (g_ad_n >= 16) break;
    }
}

static int ad_urgent(float mx, float my, float mr, float ix, float iy, float *ox, float *oy)
{
    float cx[32];
    float cy[32];
    int n = 0;
    int urgent = 0;
    int best = 0;
    float bestScore = 1e18f;

    for (int i = 0; i < g_ad_n && n < 32; i++)
    {
        const ad_proj *p = &g_ad[i];
        float dx = mx - p->x;
        float dy = my - p->y;
        float vx = p->dx * p->speed;
        float vy = p->dy * p->speed;
        float c1 = dx * vx + dy * vy;
        float c2 = 0.0f;
        float t = 0.0f;
        float r = 0.0f;
        float awx = 0.0f;
        float awy = 0.0f;

        if (c1 <= 0.0f) continue;
        c2 = vx * vx + vy * vy;
        if (c2 <= 0.0f) continue;

        t = c1 / c2;
        if (t > AD_T_URGENT) continue;

        {
            float px = p->x + vx * t;
            float py = p->y + vy * t;
            float ex = mx - px;
            float ey = my - py;

            r = mr + p->radius + AD_SAFETY;
            if (ex * ex + ey * ey > r * r) continue;
        }

        urgent++;
        ad_norm(dx, dy, &awx, &awy);

        {
            float nx = 0.0f;
            float ny = 0.0f;

            ad_norm(-p->dy * AD_PERP + awx * AD_AWAY + ix * 0.6f,
                    p->dx * AD_PERP + awy * AD_AWAY + iy * 0.6f, &nx, &ny);
            cx[n] = nx;
            cy[n] = ny;
            n++;

            ad_norm(p->dy * AD_PERP + awx * AD_AWAY + ix * 0.6f,
                    -p->dx * AD_PERP + awy * AD_AWAY + iy * 0.6f, &nx, &ny);
            cx[n] = nx;
            cy[n] = ny;
            n++;
        }
    }

    if (urgent == 0) return 0;

    ad_dirs_init();
    for (int i = 0; i < 16 && n < 32; i++)
    {
        cx[n] = g_dirx[i * (AD_DIRS / 16)];
        cy[n] = g_diry[i * (AD_DIRS / 16)];
        n++;
    }

    for (int i = 0; i < n; i++)
    {
        float s = ad_threat_score(cx[i], cy[i], mx, my, mr, ix, iy);

        if (s < bestScore)
        {
            bestScore = s;
            best = i;
        }
    }

    *ox = cx[best];
    *oy = cy[best];
    return urgent;
}

static void ad_best_dir(float mx, float my, float mr, float ix, float iy, float *ox, float *oy)
{
    int best = 0;
    float bestScore = 1e18f;

    ad_dirs_init();

    for (int i = 0; i < AD_DIRS; i++)
    {
        float s = ad_threat_score(g_dirx[i], g_diry[i], mx, my, mr, ix, iy);

        if (s < bestScore)
        {
            bestScore = s;
            best = i;
        }
    }

    *ox = g_dirx[best];
    *oy = g_diry[best];
}

static void ad_apply_vo(float *dx, float *dy, float mx, float my, float mr, float ix, float iy)
{
    int best = 0;
    float bestScore = 1e18f;
    int safe = 0;

    if (!ad_vel_unsafe(*dx, *dy, mx, my, mr)) return;

    ad_dirs_init();
    for (int i = 0; i < AD_DIRS; i++)
    {
        float s = 0.0f;

        if (ad_vel_unsafe(g_dirx[i], g_diry[i], mx, my, mr)) continue;
        safe = 1;
        s = ad_threat_score(g_dirx[i], g_diry[i], mx, my, mr, ix, iy);
        if (s < bestScore)
        {
            bestScore = s;
            best = i;
        }
    }

    if (!safe) return;

    *dx = g_dirx[best];
    *dy = g_diry[best];
}

void rcl_autododge(void)
{
    uint64_t now = (uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0);
    float mx = 0.0f;
    float my = 0.0f;
    float mr = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    int flip = 1;
    int sent = 0;

    if (!rcl_base) return;
    if (now > g_ad_solve_ms && (now - g_ad_solve_ms) < AD_PERIOD_MS) return;
    g_ad_solve_ms = now;

    rcl_dbg.hazards = 0;
    rcl_dbg.threats = 0;
    rcl_dbg.danger = 0;
    rcl_dbg.dirs = 0;
    rcl_dbg.sent = 0;
    rcl_dbg.thn = 0;
    rcl_dbg.note[0] = 0;

    if (!rcl_own_elem || rcl_dead)
    {
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "no own");
        rcl_active = 0;
        return;
    }

    mx = (float)rcl_own_x;
    my = (float)rcl_own_y;
    if (mx == 0.0f && my == 0.0f)
    {
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "no pos");
        rcl_active = 0;
        return;
    }

    mr = rcl_own_radius();
    flip = (rcl_own_team_b == 1) ? -1 : 1;

    rcl_dbg.px = mx;
    rcl_dbg.py = my;
    rcl_dbg.radius = mr;
    rcl_dbg.speed = rcl_own_speed;

    ad_collect(mx, my);
    rcl_dbg.hazards = g_ad_n;

    if (g_ad_n == 0)
    {
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "no projectiles");
        rcl_active = 0;
        g_ad_active = 0;
        return;
    }

    if (!ad_check_danger(mx, my, mr))
    {
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "safe n=%d", g_ad_n);
        rcl_active = 0;
        g_ad_active = 0;
        return;
    }

    rcl_dbg.danger = 1;

    if (ad_urgent(mx, my, mr, 0.0f, 0.0f, &dirX, &dirY))
    {
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "urgent n=%d", g_ad_n);
    }
    else
    {
        ad_best_dir(mx, my, mr, 0.0f, 0.0f, &dirX, &dirY);
        snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "field n=%d", g_ad_n);
    }

    ad_apply_vo(&dirX, &dirY, mx, my, mr, 0.0f, 0.0f);

    g_last_dir_x = dirX;
    g_last_dir_y = dirY;
    g_last_dir_ms = now;

    sent = rcl_joystick_aim(dirX, dirY, flip);

    g_ad_active = 1;
    rcl_active = 1;
    rcl_moving = sent;
    rcl_dbg.dirs = 1;
    rcl_dbg.sent = sent;
    rcl_dbg.dx = dirX;
    rcl_dbg.dy = dirY;
    rcl_dbg.tx = mx + dirX * AD_FIELD_RADIUS;
    rcl_dbg.ty = my + dirY * AD_FIELD_RADIUS;
}

void rcl_autododge_reset(void)
{
    g_ad_solve_ms = 0;
    g_ad_active = 0;
    g_last_dir_ms = 0;
    rcl_active = 0;
    rcl_moving = 0;
}
