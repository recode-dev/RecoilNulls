#ifndef UTILS_WALLS_H
#define UTILS_WALLS_H
#include <cstdint>
inline constexpr int WALL_SUBTILES_PER_TILE = 3;
inline constexpr int WALL_MAX_MAP_TILES     = 120;
int32_t              wall_logic_units(int tiles);
int                  wall_cache_width();
int                  wall_cache_height();
void                 wall_cache_bind(uintptr_t tileMap);
int                  wall_cell_blocked_at(int32_t x, int32_t y);
int                  wall_cell_blocked_wide(int32_t x, int32_t y, int32_t r);
int32_t
wall_clear_run(int32_t x, int32_t y, int32_t dx, int32_t dy, int32_t max_run, int32_t radius);
int32_t axis(float v, int tiles);
#endif
