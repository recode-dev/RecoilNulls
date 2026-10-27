#ifndef UTILS_WALLS_H
#define UTILS_WALLS_H
#include <stdint.h>
#define WALL_TILE_SIZE 300.0f
#define WALL_BLOCKS_MOVEMENT 128
#define WALL_BLOCKS_PROJECTILES 64
#define WALL_MAX_MAP_TILES 120
#define WALL_MAX_TOTAL_TILES (WALL_MAX_MAP_TILES * WALL_MAX_MAP_TILES)
#define WALL_RETRY_MS 2000
int wall_cache_w(void);
int wall_cache_h(void);
int wall_build(void);
void wall_drop(void);
void wall_bind(uintptr_t tileMap);
int wall_ready(void);
int wall_holes(void);
int wall_maybe_refresh(uint64_t now_ms);
int wall_is_blocked_at(float x, float y, int mask);
int wall_is_blocked_wide(float x, float y, float r, int mask);
int wall_los(float ax, float ay, float bx, float by, int mask);
#endif
