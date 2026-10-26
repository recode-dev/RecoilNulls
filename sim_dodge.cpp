#include <vector>
#include <optional>
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bs {

constexpr double TICK_HZ      = 20.0;
constexpr double DT           = 1.0 / TICK_HZ;
constexpr double HITBOX_STD   = 120.0;
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

} // namespace bs

void run_test(const std::string& name, const bs::V2& pos, double speed, const std::vector<bs::Projectile>& ps, const std::vector<bs::Wall>& walls = {}) {
    std::cout << "Test: " << name << std::endl;
    auto res = bs::bestDodge(pos, speed, ps, walls);
    std::cout << "  Angle: " << std::fixed << std::setprecision(1) << res.angle << " deg" << std::endl;
    std::cout << "  Dmg: " << res.eval.dmg << ", First: " << res.eval.first << ", Clear: " << res.eval.clear << ", Minimax: " << res.eval.minimax << std::endl;
    std::cout << "  Deadzone: " << (res.deadzone ? "Yes" : "No") << std::endl;
    std::cout << "--------------------------------" << std::endl;
}

int main() {
    bs::V2 pos(0, 0);
    double speed = 720.0;

    // Test 1: No projectiles
    run_test("No Projectiles", pos, speed, {});

    // Test 2: Projectile from right moving left
    {
        bs::Projectile p;
        p.id = 1;
        p.damage = 1000;
        p.radius = 100;
        p.speed = 1500;
        p.vx = -1500;
        p.vy = 0;
        p.spawnX = 1000;
        p.spawnY = 0;
        p.ticks.resize(50);
        for(int i=0; i<50; ++i) p.ticks[i] = bs::V2(1000 - 1500 * bs::DT * i, 0);
        run_test("Projectile from Right", pos, speed, {p});
    }

    // Test 3: Projectile from top moving down
    {
        bs::Projectile p;
        p.id = 2;
        p.damage = 1000;
        p.radius = 100;
        p.speed = 1500;
        p.vx = 0;
        p.vy = -1500;
        p.spawnX = 0;
        p.spawnY = 1000;
        p.ticks.resize(50);
        for(int i=0; i<50; ++i) p.ticks[i] = bs::V2(0, 1000 - 1500 * bs::DT * i);
        run_test("Projectile from Top", pos, speed, {p});
    }

    // Test 4: Projectile with area effect (e.g. landing at 0,0)
    {
        bs::Projectile p;
        p.id = 3;
        p.damage = 1500;
        p.radius = 50;
        p.areaRadius = 400;
        p.areaTicks = 20;
        p.deathTick = 10; // Lands in 10 ticks
        p.ticks.resize(20);
        for(int i=0; i<20; ++i) p.ticks[i] = bs::V2(500 - 50 * i, 0); // Moving towards 0,0
        run_test("Area Effect Projectile", pos, speed, {p});
    }

    // Test 5: Multiple projectiles
    {
        std::vector<bs::Projectile> ps;
        // P1 from right
        {
            bs::Projectile p;
            p.id = 4; p.damage = 1000; p.radius = 100; p.speed = 1500; p.vx = -1500; p.vy = 0; p.spawnX = 1000; p.spawnY = 0;
            p.ticks.resize(50); for(int i=0; i<50; ++i) p.ticks[i] = bs::V2(1000 - 1500 * bs::DT * i, 0);
            ps.push_back(p);
        }
        // P2 from left
        {
            bs::Projectile p;
            p.id = 5; p.damage = 1000; p.radius = 100; p.speed = 1500; p.vx = 1500; p.vy = 0; p.spawnX = -1000; p.spawnY = 0;
            p.ticks.resize(50); for(int i=0; i<50; ++i) p.ticks[i] = bs::V2(-1000 + 1500 * bs::DT * i, 0);
            ps.push_back(p);
        }
        run_test("Surrounded (Right & Left)", pos, speed, ps);
    }

    return 0;
}
