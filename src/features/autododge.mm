#include "../data.hpp"
#include "../core/scan.h"
#include "../core/commands.h"
#include "../utils/walls.h"
#include <map>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdint.h>
#include <stdio.h>

// --- Constants ---
const float SAFETY_MARGIN = 28.0f;
const float T_URGENT      = 0.9f;
const float T_FIELD       = 1.8f;
const float FIELD_RADIUS  = 275.0f;
const int N_DIRECTIONS    = 64;
const float PERP_WEIGHT   = 2.4f;
const float AWAY_WEIGHT   = 1.0f;
const float INTENT_WEIGHT = 1.2f;
static float CHAR_SPEED   = 720.0f;

// --- Types ---
struct V2 {
    float x, y;
    V2(float x_ = 0, float y_ = 0) : x(x_), y(y_) {}
};

struct Projectile {
    float x, y;
    float dirX, dirY;
    float speed;
    float radius;
    uint64_t lastSeen;
    int id;
};

// --- State ---
static std::map<int, Projectile> g_proj_cache;
static V2 SAMPLES[64];
static bool g_samples_init = false;
static V2 g_lastSafeDir(0, 0);
static uint64_t g_lastSafeDirTime = 0;

// --- Math Helpers ---
static V2 normalize(float x, float y) {
    float l = sqrtf(x * x + y * y);
    if (l < 1e-6f) return V2(1.0f, 0.0f);
    return V2(x / l, y / l);
}

static void initSamples() {
    if (g_samples_init) return;
    for (int i = 0; i < N_DIRECTIONS; i++) {
        float angle = (2.0f * M_PI * i) / N_DIRECTIONS;
        SAMPLES[i] = V2(cosf(angle), sinf(angle));
    }
    g_samples_init = true;
}

static std::vector<V2> getDirectionSamples(int n) {
    if (n == N_DIRECTIONS) {
        std::vector<V2> out(SAMPLES, SAMPLES + N_DIRECTIONS);
        return out;
    }
    std::vector<V2> out;
    out.reserve(n);
    for (int i = 0; i < n; i++) {
        float angle = (2.0f * M_PI * i) / n;
        out.push_back(V2(cosf(angle), sinf(angle)));
    }
    return out;
}

// --- Core Logic ---

static bool isUrgentThreat(const Projectile& p, float myX, float myY, float myRadius) {
    float dx = myX - p.x;
    float dy = myY - p.y;
    float vx = p.dirX * p.speed;
    float vy = p.dirY * p.speed;

    float c1 = dx * vx + dy * vy;
    if (c1 <= 0) return false;
    float c2 = vx * vx + vy * vy;
    if (c2 <= 0) return false;
    float tHit = c1 / c2;
    if (tHit > T_URGENT) return false;

    float closestX = p.x + vx * tHit;
    float closestY = p.y + vy * tHit;

    float r = myRadius + p.radius + SAFETY_MARGIN;
    float ddx = myX - closestX;
    float ddy = myY - closestY;
    return (ddx * ddx + ddy * ddy) <= r * r;
}

static float threatScore(V2 dir, float myX, float myY, float myRadius, V2 intentDir, V2 lastSafeDir, uint64_t lastSafeDirAgeMs) {
    float score = 0;

    for (auto const& [id, p] : g_proj_cache) {
        float r = myRadius + p.radius + SAFETY_MARGIN;

        float vx = p.dirX * p.speed - dir.x * CHAR_SPEED;
        float vy = p.dirY * p.speed - dir.y * CHAR_SPEED;

        float dx = p.x - myX;
        float dy = p.y - myY;

        float a = vx * vx + vy * vy;
        float b = 2.0f * (dx * vx + dy * vy);
        float c = dx * dx + dy * dy;

        float minD2 = c;
        if (a > 1e-6f) {
            float tMin = -b / (2.0f * a);
            if (tMin > 0 && tMin <= T_FIELD) {
                minD2 = c + b * tMin + a * tMin * tMin;
            } else if (tMin > T_FIELD) {
                minD2 = c + b * T_FIELD + a * T_FIELD * T_FIELD;
            }
        }

        float danger = (minD2 < r * r) ? 2500.0f : (r * r) / std::max(minD2, 50.0f);
        score += danger;
    }

    if (intentDir.x != 0 || intentDir.y != 0) {
        float dotIntent = dir.x * intentDir.x + dir.y * intentDir.y;
        score -= dotIntent * INTENT_WEIGHT * 15.0f;
    }

    if ((lastSafeDir.x != 0 || lastSafeDir.y != 0) && lastSafeDirAgeMs < 400) {
        float dotInertia = dir.x * lastSafeDir.x + dir.y * lastSafeDir.y;
        score -= dotInertia * 45.0f;
    }

    return score;
}

static bool isVelocityUnsafe(V2 dir, float myX, float myY, float myRadius) {
    for (auto const& [id, p] : g_proj_cache) {
        float r = myRadius + p.radius + SAFETY_MARGIN;

        float vx = p.dirX * p.speed - dir.x * CHAR_SPEED;
        float vy = p.dirY * p.speed - dir.y * CHAR_SPEED;
        float dx = p.x - myX;
        float dy = p.y - myY;

        float a = vx * vx + vy * vy;
        float b = 2.0f * (dx * vx + dy * vy);
        float c = dx * dx + dy * dy - r * r;

        if (c < 0) return true;
        if (a > 1e-6f) {
            float disc = b * b - 4.0f * a * c;
            if (disc >= 0) {
                float t1 = (-b - sqrtf(disc)) / (2.0f * a);
                if (t1 > 0 && t1 <= T_FIELD) return true;
            }
        }
    }
    return false;
}

static V2 getUrgentDodgeDir(float myX, float myY, float myRadius, V2 intentDir, uint64_t now) {
    std::vector<V2> candidates;
    int urgentCount = 0;

    for (auto const& [id, p] : g_proj_cache) {
        if (!isUrgentThreat(p, myX, myY, myRadius)) continue;
        urgentCount++;

        V2 away = normalize(myX - p.x, myY - p.y);
        V2 perp1 = normalize(-p.dirY, p.dirX);
        V2 perp2 = normalize(p.dirY, -p.dirX);
        float intentW = 0.6f;

        // Formula: perp*2.4 + away*1.0 + intent*0.6
        candidates.push_back(normalize(
            perp1.x * PERP_WEIGHT + away.x * AWAY_WEIGHT + intentDir.x * intentW,
            perp1.y * PERP_WEIGHT + away.y * AWAY_WEIGHT + intentDir.y * intentW
        ));
        candidates.push_back(normalize(
            perp2.x * PERP_WEIGHT + away.x * AWAY_WEIGHT + intentDir.x * intentW,
            perp2.y * PERP_WEIGHT + away.y * AWAY_WEIGHT + intentDir.y * intentW
        ));
    }

    if (urgentCount == 0) return V2(0, 0);

    // Add 16 uniform samples as fallback
    std::vector<V2> samples = getDirectionSamples(16);
    candidates.insert(candidates.end(), samples.begin(), samples.end());

    V2 best = candidates[0];
    float bestScore = 1e18f;
    uint64_t age = now - g_lastSafeDirTime;

    for (const auto& c : candidates) {
        float s = threatScore(c, myX, myY, myRadius, intentDir, g_lastSafeDir, age);
        if (s < bestScore) {
            bestScore = s;
            best = c;
        }
    }
    return best;
}

static V2 chooseBestDirection(float myX, float myY, float myRadius, V2 intentDir, uint64_t now) {
    V2 best = (intentDir.x != 0 || intentDir.y != 0) ? intentDir : SAMPLES[0];
    float bestScore = 1e18f;
    uint64_t age = now - g_lastSafeDirTime;

    for (int i = 0; i < N_DIRECTIONS; i++) {
        float s = threatScore(SAMPLES[i], myX, myY, myRadius, intentDir, g_lastSafeDir, age);
        if (s < bestScore) {
            bestScore = s;
            best = SAMPLES[i];
        }
    }
    return best;
}

static V2 applyVO(V2 dir, float myX, float myY, float myRadius, V2 intentDir, uint64_t now) {
    if (!isVelocityUnsafe(dir, myX, myY, myRadius)) return dir;

    V2 best = dir;
    float bestScore = 1e18f;
    bool foundSafe = false;
    uint64_t age = now - g_lastSafeDirTime;

    for (int i = 0; i < N_DIRECTIONS; i++) {
        if (isVelocityUnsafe(SAMPLES[i], myX, myY, myRadius)) continue;
        foundSafe = true;
        float s = threatScore(SAMPLES[i], myX, myY, myRadius, intentDir, g_lastSafeDir, age);
        if (s < bestScore) {
            bestScore = s;
            best = SAMPLES[i];
        }
    }
    return foundSafe ? best : dir;
}

static bool checkDanger(float myX, float myY, float myRadius) {
    for (auto const& [id, p] : g_proj_cache) {
        float r = myRadius + p.radius + SAFETY_MARGIN * 2.5f;
        float dx = myX - p.x;
        float dy = myY - p.y;
        if (dx * dx + dy * dy <= r * r) return true;

        float vx = p.dirX * p.speed;
        float vy = p.dirY * p.speed;
        float c1 = dx * vx + dy * vy;
        if (c1 > 0) {
            float c2 = vx * vx + vy * vy;
            if (c2 > 0) {
                float tHit = c1 / c2;
                if (tHit <= T_FIELD) {
                    float cx = p.x + vx * tHit;
                    float cy = p.y + vy * tHit;
                    if ((myX - cx) * (myX - cx) + (myY - cy) * (myY - cy) <= r * r) return true;
                }
            }
        }
    }
    return false;
}

// --- Integration ---

extern rcl_dbg_t rcl_dbg;
extern int rcl_active;
extern int rcl_moving;

static uintptr_t rcl_battlescreen(void) {
    uintptr_t ctrl = rcl_controller();
    if (!ctrl) return 0;
    void* val = nullptr;
    if (!rcl_read_ptr(ctrl + RCL_BATTLESCREEN_OFF, &val)) return 0;
    return (uintptr_t)val;
}

void rcl_autododge(void) {
    uint64_t now = rcl_us() / 1000; // ms
    if (!rcl_base || !rcl_own_elem || rcl_dead) {
        rcl_active = 0;
        return;
    }

    initSamples();

    float mx = (float)rcl_own_x;
    float my = (float)rcl_own_y;
    float myRadius = rcl_own_radius();

    // 1. Dynamic speed
    {
        uintptr_t def = 0;
        if (rcl_read_ptr(rcl_own_elem + RCL_ELEM_DEF_OFF, (void**)&def) && def) {
            uint32_t readSpd = 0;
            if (rcl_read_int(def + 0x1c4, (int32_t*)&readSpd)) {
                if (readSpd >= 300 && readSpd <= 8500) {
                    CHAR_SPEED = (float)readSpd;
                }
            }
        }
    }

    // 2. Projectile Scan & Filter
    {
        // analyzeProjectiles equivalent
        std::map<int, bool> seen;
        for (int i = 0; i < 16; i++) {
            rcl_proj_t* p = &rcl_projs[i];
            if (!p->elem) continue;

            // VTABLE check
            void* vtable = nullptr;
            if (!rcl_read_ptr(p->elem, &vtable) || !vtable) continue;
            if ((uintptr_t)vtable - rcl_base != RCL_CLASS_PROJ_RVA) continue;

            // LogicProjectileData VTABLE check
            void* def = nullptr;
            if (!rcl_read_ptr(p->elem + RCL_ELEM_DEF_OFF, &def) || !def) continue;
            void* defVt = nullptr;
            if (!rcl_read_ptr((uintptr_t)def, &defVt) || !defVt) continue;
            if ((uintptr_t)defVt - rcl_base != RCL_VTABLE_PROJ_DATA_RVA) continue;

            // Team check
            if (p->team >= 0 && p->team == rcl_own_team_b) continue;

            // Distance check
            float distSq = (float)(p->x - mx) * (p->x - mx) + (float)(p->y - my) * (p->y - my);
            if (distSq > 5000.0f * 5000.0f) continue;

            // Dead flag
            uint32_t dead = 0;
            if (!rcl_read_int(p->elem + 208, (int32_t*)&dead) || dead != 0) continue;

            int pid = p->gid;
            seen[pid] = true;
            Projectile& state = g_proj_cache[pid];
            float px = (float)p->x;
            float py = (float)p->y;

            if (state.lastSeen == 0) {
                // First appearance
                state.dirX = cosf(p->angle);
                state.dirY = sinf(p->angle);
            } else {
                float ddx = px - state.x;
                float ddy = py - state.y;
                float len = sqrtf(ddx * ddx + ddy * ddy);
                if (len > 6.0f) {
                    state.dirX = ddx / len;
                    state.dirY = ddy / len;
                }
            }
            state.x = px;
            state.y = py;
            state.speed = p->speed;
            state.radius = p->radius;
            state.lastSeen = now;
            state.id = pid;
        }

        // Garbage collection
        for (auto it = g_proj_cache.begin(); it != g_proj_cache.end(); ) {
            if (seen.find(it->first) == seen.end() || now - it->second.lastSeen > 450) {
                it = g_proj_cache.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 3. Joystick Intent
    V2 intentDir(0, 0);
    uintptr_t js = rcl_battlescreen();
    if (js) {
        uint8_t dragging = 0;
        if (rcl_read_byte(js + RCL_JOY_DRAGGING, &dragging) && dragging) {
            float cx, cy, tx, ty;
            if (rcl_read_float(js + RCL_JOY_CENTER_X, &cx) && rcl_read_float(js + RCL_JOY_CENTER_Y, &cy) &&
                rcl_read_float(js + RCL_JOY_TARGET_X, &tx) && rcl_read_float(js + RCL_JOY_TARGET_Y, &ty)) {
                float dx = tx - cx;
                float dy = ty - cy;
                if (dx * dx + dy * dy > 900.0f) {
                    intentDir = normalize(dx, dy);
                }
            }
        }
    }

    // 4. Decision
    if (!checkDanger(mx, my, myRadius)) {
        rcl_active = 0;
        rcl_moving = 0;
        return;
    }

    V2 urgent = getUrgentDodgeDir(mx, my, myRadius, intentDir, now);
    V2 baseDir;
    if (urgent.x != 0 || urgent.y != 0) {
        baseDir = urgent;
    } else {
        baseDir = chooseBestDirection(mx, my, myRadius, intentDir, now);
    }

    V2 safeDir = applyVO(baseDir, mx, my, myRadius, intentDir, now);

    g_lastSafeDir = safeDir;
    g_lastSafeDirTime = now;

    // 5. Emit
    float angle = atan2f(safeDir.y, safeDir.x);
    float teamFlip = (rcl_own_team_b == 1) ? -1.0f : 1.0f;
    float reach = 800.0f;
    int32_t tx = (int32_t)roundf(mx + cosf(angle) * reach * teamFlip);
    int32_t ty = (int32_t)roundf(my + sinf(angle) * reach * teamFlip);

    rcl_move_to(tx, ty, mx, my);
    int sent = rcl_enqueue(tx, ty);

    rcl_active = 1;
    rcl_moving = sent;

    // Debug
    rcl_dbg.px = mx;
    rcl_dbg.py = my;
    rcl_dbg.dx = safeDir.x;
    rcl_dbg.dy = safeDir.y;
    rcl_dbg.tx = (float)tx;
    rcl_dbg.ty = (float)ty;
    rcl_dbg.hazards = (int)g_proj_cache.size();
    snprintf(rcl_dbg.note, sizeof(rcl_dbg.note), "VO:%s SPD:%.0f", (isVelocityUnsafe(baseDir, mx, my, myRadius) ? "FAIL" : "OK"), CHAR_SPEED);
}

void rcl_autododge_reset(void) {
    g_proj_cache.clear();
    g_lastSafeDir = V2(0, 0);
    g_lastSafeDirTime = 0;
    rcl_active = 0;
    rcl_moving = 0;
}
