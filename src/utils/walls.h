#ifndef UTILS_WALLS_H
#define UTILS_WALLS_H
#include <cstdint>
inline constexpr int WALL_SUBTILES_PER_TILE = 3;
inline constexpr int WALL_MAX_MAP_TILES     = 120;
float                wall_logic_units(int tiles);
int                  wall_cache_width();
int                  wall_cache_height();
void                 wall_cache_bind(uintptr_t tileMap);
int                  wall_cell_blocked_at(float x, float y);
int                  wall_cell_blocked_wide(float x, float y, float r);
float   wall_clear_run(float x, float y, float dx, float dy, float max_run, float radius);
int32_t axis(float v, int tiles);
#endif
