#ifndef UTILS_WALLS_H
#define UTILS_WALLS_H

#include <cstdint>

inline constexpr int WALL_SUBTILES_PER_TILE   = 3;
inline constexpr int WALL_SUBTILE_LOGIC_UNITS = 100;
inline constexpr int WALL_TILE_LOGIC_UNITS    = 300;
inline constexpr int WALL_LAYER_BIT_STRIDE    = 9;
inline constexpr int WALL_MAX_MASK_PLANES     = 4;
inline constexpr int WALL_PATHFINDER_LAYER    = 1;
inline constexpr int WALL_MAX_MAP_TILES       = 120;
inline constexpr int WALL_MAX_TOTAL_TILES =
    WALL_MAX_MAP_TILES * WALL_MAX_MAP_TILES;
inline constexpr int      WALL_POINTER_CHUNK = 64;
inline constexpr uint64_t WALL_REFRESH_MS    = 250;
inline constexpr uint64_t WALL_RETRY_MS      = 2000;

float   wall_logic_units(int tiles);
int     wall_cell_is_passable(float x, float y);
int     wall_cached_blocked_count();
int     wall_engine_blocked_total();
int     wall_cache_width();
int     wall_cache_height();
int     wall_cache_build();
void    wall_cache_drop();
void    wall_cache_bind(uintptr_t tileMap);
int     wall_cache_refresh_if_stale(uint64_t now_ms);
int     wall_cell_blocked_at(float x, float y);
int     wall_cell_blocked_wide(float x, float y, float r);
int     wall_line_of_sight_clear(float ax, float ay, float bx, float by);
int     wall_path_clear(float ax, float ay, float bx, float by);
int32_t axis(float v, int tiles);
float   wall_pull(float mx, float my, float dx, float dy, float radius);

#endif
