#pragma once
#include <cstdint>
namespace ad {
constexpr float OWN_HITBOX = 120.0f;
constexpr int THREAT_MAX = 32;
constexpr int PROJ_SLOTS = 48;
constexpr uint64_t TICK_MS = 50;
constexpr uint64_t PROJ_TTL_MS = TICK_MS * 20;
constexpr uint8_t B_BEAM = 1, B_IND = 2, B_PIERCE = 4;
struct Threat {
    float x, y, vx, vy, rad, tt;
    int32_t pid;
    char name[40];
};
struct Proj {
    int32_t pid;
    uint64_t seen;
    float x, y, ux, uy, speed, rad;
    float min_dmin;
    uint8_t flags;
    bool vel;
    char name[40];
};
extern Threat threats[THREAT_MAX];
extern int threat_n;
extern Proj projs[PROJ_SLOTS];
extern uintptr_t char_vt;
extern float self_speed;
float body_rad();
void evict_stale(uint64_t now);
void collect_chars(int32_t own_gid, float mx, float my);
void collect_projs(uint64_t now, float mx, float my);
}
