#ifndef UTILS_WALLS_H
#define UTILS_WALLS_H
#include <cstdint>
inline constexpr int WALL_MAX_MAP_TILES = 120;
inline constexpr int WALL_MAX_TOTAL_TILES =
    WALL_MAX_MAP_TILES * WALL_MAX_MAP_TILES;
inline constexpr int WALL_LAYER_THIN = 0;
inline constexpr int WALL_LAYER_NORMAL = 1;
inline constexpr int WALL_LAYER_BULKY = 2;
inline constexpr int WALL_LAYER_COUNT = 3;
inline constexpr uint64_t WALL_RETRY_MS = 2000;
inline constexpr float WALL_BULKY_RADIUS = 199.0f;
inline constexpr float WALL_SUBTILE_UNITS = 100.0f;
inline constexpr float WALL_TILE_UNITS = 300.0f;
inline constexpr float WALL_LOS_STEP = 50.0f;
float wall_tile_units();
float wall_subtile_units();
float wall_logic_units(int tiles);
int wall_layer_for_radius(float radius);
int wall_cell_is_passable(float x, float y);
int wall_cell_is_passable_layer(float x, float y, int layer);
int wall_cached_blocked_count();
int wall_cached_vision_blocked_count();
int wall_engine_blocked_total();
int wall_cache_width();
int wall_cache_height();
int wall_cache_build();
void wall_cache_drop();
void wall_cache_bind(uintptr_t tileMap);
int wall_cache_refresh_if_stale(uint64_t now_ms);
int wall_cache_local_trusted();
int wall_cache_layer();
void wall_cache_set_layer(int layer);
int wall_cell_blocked_at(float x, float y);
int wall_cell_blocked_layer(float x, float y, int layer);
int wall_cell_blocked_wide(float x, float y, float r);
int wall_vision_blocked_at(float x, float y);
int wall_line_of_sight_clear(float ax, float ay, float bx, float by);
int32_t axis(float v, int tiles);
float wall_pull(float mx, float my, float dx, float dy, float radius);
#endif
