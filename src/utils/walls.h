#ifndef UTILS_WALLS_H
#define UTILS_WALLS_H
#include <cstdint>
inline constexpr int WALL_MAX_MAP_TILES      = 120;
inline constexpr int WALL_BODY_SUBTILE_UNITS = 199;
int32_t              wall_logic_units(int tiles);
int                  wall_cache_width();
int                  wall_cache_height();
void                 wall_cache_bind(uintptr_t tileMap);
uintptr_t            wall_pathfinder();
int32_t              wall_free_run(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int size);
int32_t              axis(float v, int tiles);
#endif
