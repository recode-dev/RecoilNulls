#include "walls.h"
#include <cmath>
#include <cstdint>
#include "../core/offsets.h"
#include "../core/scan.h"
static uintptr_t wall_map  = 0;
static int       wall_w    = 0;
static int       wall_h    = 0;
static int       wall_have = 0;

int32_t wall_logic_units(int tiles)
{
    return game_tile_to_logic(tiles) - game_tile_to_logic(0);
}

void wall_cache_bind(uintptr_t tileMap)
{
    int32_t mapW = 0;
    int32_t mapH = 0;
    if (!tileMap)
    {
        wall_map  = 0;
        wall_w    = 0;
        wall_h    = 0;
        wall_have = 0;
        return;
    }
    if (tileMap != wall_map)
    {
        wall_map  = tileMap;
        wall_w    = 0;
        wall_h    = 0;
        wall_have = 0;
    }
    if (wall_have && wall_w > 0 && wall_h > 0) return;
    if (!read_int(tileMap + static_cast<uintptr_t>(LOGICTILEMAP__WIDTH), &mapW) || mapW <= 0 ||
        mapW > WALL_MAX_MAP_TILES)
    {
        wall_have = 0;
        return;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(LOGICTILEMAP__HEIGHT), &mapH) || mapH <= 0 ||
        mapH > WALL_MAX_MAP_TILES)
    {
        wall_have = 0;
        return;
    }
    wall_w    = mapW;
    wall_h    = mapH;
    wall_have = 1;
}

int wall_cache_width()
{
    return wall_w;
}

int wall_cache_height()
{
    return wall_h;
}

uintptr_t wall_pathfinder()
{
    uintptr_t pathfinder = 0;
    if (!wall_map || !wall_have) return 0;
    if (!read_ptr(wall_map + static_cast<uintptr_t>(LOGICTILEMAP__PATHFINDER), &pathfinder))
    {
        return 0;
    }
    return pathfinder;
}

int32_t wall_free_run(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int size)
{
    int32_t   out[2] = {-1, -1};
    int32_t   run    = 0;
    int32_t   reach  = 0;
    uintptr_t pf     = wall_pathfinder();
    if (!pf) return 0;
    if (!game_first_collision(size, x1, y1, x2, y2, pf, out))
    {
        return game_vec2_length(x2 - x1, y2 - y1);
    }
    reach = game_vec2_length(out[0] - x1, out[1] - y1);
    run   = game_vec2_length(x2 - x1, y2 - y1);
    if (reach < run) return reach;
    return run;
}

int32_t axis(float v, int tiles)
{
    float limit = static_cast<float>(wall_logic_units(tiles) - 1);
    if (v < 0.0f) return 0;
    if (v > limit) return static_cast<int32_t>(limit);
    return static_cast<int32_t>(roundf(v));
}
