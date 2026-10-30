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
int wall_line_of_sight_clear(float ax, float ay, float bx, float by);
int wall_path_clear(float ax, float ay, float bx, float by);
int wall_segment_clear(float ax, float ay, float bx, float by, float radius);
int32_t axis(float v, int tiles);
float   wall_pull(float mx, float my, float dx, float dy, float radius);
#endif
