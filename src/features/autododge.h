#ifndef FEATURES_AUTODODGE_AUTODODGE_H
#define FEATURES_AUTODODGE_AUTODODGE_H
#include <cstdint>
inline constexpr int ENEMY_MAX = 4;
inline constexpr int ALLY_MAX  = 8;
extern int           in_move;
extern float         dodge_dir_x, dodge_dir_y;
extern int           enemy_n;
extern float         enemy_x[ENEMY_MAX];
extern float         enemy_y[ENEMY_MAX];
extern float         enemy_d[ENEMY_MAX];
extern int           ally_n;
extern float         ally_x[ALLY_MAX];
extern float         ally_y[ALLY_MAX];
extern float         ally_d[ALLY_MAX];
extern int           proj_own_n;
extern int           proj_enemy_n;
int32_t              projectile_radius(uintptr_t data);
int32_t              projectile_speed(uintptr_t data);
int32_t              character_collision_radius(uintptr_t data);
void                 reset_autododge_state();
void                 update_autododge();
namespace ad
{
extern float       own_hitbox;
constexpr int      THREAT_MAX  = 32;
constexpr int      PROJ_SLOTS  = 48;
constexpr uint64_t TICK_MS     = 50;
constexpr uint64_t PROJ_TTL_MS = TICK_MS * 20;
struct Threat
{
    float   x, y, vx, vy, rad, tt;
    int32_t pid;
};
struct Proj
{
    int32_t   pid;
    uint64_t  seen;
    uintptr_t data;
    float     x, y, ux, uy, speed, rad;
    int32_t   team;
    bool      vel;
};
extern Threat    threats[THREAT_MAX];
extern int       threat_n;
extern Proj      projs[PROJ_SLOTS];
extern uintptr_t char_vt;
extern float     self_speed;
void             evict_stale_projectiles(uint64_t now);
void collect_logic_objects(uint64_t now, int32_t own_gid, float mx, float my);
}
#endif
