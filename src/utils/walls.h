#ifndef UTILS_WALLS_H
#define UTILS_WALLS_H
#include <cstdint>
inline constexpr int WALL_BLOCKS_MOVEMENT = 128;
inline constexpr int WALL_MAX_MAP_TILES = 120;
inline constexpr int WALL_MAX_TOTAL_TILES =
    WALL_MAX_MAP_TILES * WALL_MAX_MAP_TILES;
inline constexpr int WALL_RETRY_MS = 2000;
float wall_units(int tiles);
int wall_passable_at(float x, float y);
int wall_blocked_count();
int wall_cache_w();
int wall_cache_h();
int wall_build();
void wall_drop();
void wall_bind(uintptr_t tileMap);
int wall_maybe_refresh(uint64_t now_ms);
int wall_is_blocked_at(float x, float y);
int wall_is_blocked_wide(float x, float y, float r);
int wall_los(float ax, float ay, float bx, float by);
int32_t axis(float v, int tiles);
float wall_pull(float mx, float my, float dx, float dy, float radius);
#endif
