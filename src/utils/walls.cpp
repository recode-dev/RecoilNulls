#include "walls.h"

#include <cmath>
#include <cstring>

#include "../core/scan.h"

static uint8_t wall_grid[WALL_MAX_TOTAL_TILES];
static uintptr_t wall_tiles = 0;
static int wall_tiles_w = 0;
static int wall_tiles_h = 0;
static int wall_w = 0;
static int wall_h = 0;
static int wall_have = 0;
static uintptr_t wall_map = 0;
static int wall_dirty = 1;
static uint64_t wall_fail_ms = 0;
static uintptr_t wall_raw[WALL_MAX_TOTAL_TILES];
static int wall_engine_blocked = 0;

int wall_passable_at(float x, float y)
{
    if (!wall_map)
    {
        return 0;
    }
    int cell = game_tile_to_logic(1) / 3;
    if (cell <= 0)
    {
        return 0;
    }
    return game_tile_passable(
        wall_map, static_cast<int>(x) / cell, static_cast<int>(y) / cell);
}

int wall_blocked_count()
{
    int n = 0;
    for (int i = 0; i < wall_w * wall_h; i++)
    {
        if (wall_grid[i])
        {
            n++;
        }
    }
    return n;
}

float wall_units(int tiles)
{
    return static_cast<float>(game_tile_to_logic(tiles) -
                              game_tile_to_logic(0));
}

int wall_cache_w()
{
    return wall_w;
}

int wall_cache_h()
{
    return wall_h;
}

void wall_drop()
{
    wall_tiles = 0;
    wall_tiles_w = 0;
    wall_tiles_h = 0;
    wall_map = 0;
    wall_w = 0;
    wall_h = 0;
    wall_have = 0;
    wall_dirty = 1;
    wall_fail_ms = 0;
}

void wall_bind(uintptr_t tileMap)
{
    void* tileArray = nullptr;
    int32_t mapW = 0;
    int32_t mapH = 0;
    if (!tileMap)
    {
        return;
    }
    if (tileMap != wall_map)
    {
        wall_map = tileMap;
        wall_tiles = 0;
        wall_tiles_w = 0;
        wall_tiles_h = 0;
        wall_w = 0;
        wall_h = 0;
        wall_have = 0;
        wall_dirty = 1;
        wall_fail_ms = 0;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(TILEMAP_WIDTH_OFF), &mapW) ||
        mapW <= 0 || mapW > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(TILEMAP_HEIGHT_OFF),
                  &mapH) ||
        mapH <= 0 || mapH > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_ptr(tileMap + static_cast<uintptr_t>(TILEMAP_TILES_OFF),
                  &tileArray) ||
        !tileArray)
    {
        return;
    }
    if ((uintptr_t)tileArray != wall_tiles || mapW != wall_tiles_w ||
        mapH != wall_tiles_h)
    {
        wall_tiles = (uintptr_t)tileArray;
        wall_tiles_w = mapW;
        wall_tiles_h = mapH;
        wall_have = 0;
        wall_dirty = 1;
    }
}

int wall_build()
{
    int width = wall_tiles_w;
    int height = wall_tiles_h;
    int total = 0;
    int i = 0;
    wall_have = 0;
    wall_engine_blocked = 0;
    if (!wall_tiles)
    {
        return 0;
    }
    if (width <= 0 || width > WALL_MAX_MAP_TILES)
    {
        return 0;
    }
    if (height <= 0 || height > WALL_MAX_MAP_TILES)
    {
        return 0;
    }
    total = width * height;
    if (total > WALL_MAX_TOTAL_TILES)
    {
        return 0;
    }
    if (!read_bytes(wall_tiles,
                    wall_raw,
                    static_cast<size_t>(total) * sizeof(uintptr_t)))
    {
        return 0;
    }
    for (i = 0; i < total; i++)
    {
        uintptr_t tile = 0;
        uint64_t blockMask = 0;
        uint64_t hardMask = 0;
        wall_grid[i] = 0;
        memcpy(&tile,
               wall_raw + static_cast<size_t>(i) * sizeof(uintptr_t),
               sizeof(uintptr_t));
        if (!tile || !pointer_plausible(tile))
        {
            continue;
        }
        if (!read_bytes(tile + LOGICTILE_BLOCKMASK_OFF,
                        &blockMask,
                        sizeof(blockMask)) ||
            !read_bytes(tile + LOGICTILE_FOURTHMASK_OFF,
                        &hardMask,
                        sizeof(hardMask)))
        {
            continue;
        }
        if ((blockMask | hardMask) & 0x1ffULL)
        {
            wall_grid[i] = WALL_BLOCKS_MOVEMENT;
        }
        if (!game_tile_passable(
                wall_map, (i % width) * 3 + 1, (i / width) * 3 + 1))
        {
            wall_engine_blocked++;
        }
    }
    wall_w = width;
    wall_h = height;
    wall_have = 1;
    wall_dirty = 0;
    return 1;
}

int wall_maybe_refresh(uint64_t now_ms)
{
    if (!wall_dirty)
    {
        return 0;
    }
    if (now_ms - wall_fail_ms < static_cast<uint64_t>(WALL_RETRY_MS))
    {
        return 0;
    }
    if (wall_build())
    {
        return 1;
    }
    wall_fail_ms = now_ms;
    return 0;
}

int wall_is_blocked_at(float x, float y)
{
    int tx = 0;
    int ty = 0;
    if (!wall_have)
    {
        return 0;
    }
    if (wall_w <= 0 || wall_h <= 0)
    {
        return 0;
    }
    tx = game_logic_to_tile(static_cast<int>(x));
    ty = game_logic_to_tile(static_cast<int>(y));
    if (tx < 0 || tx >= wall_w || ty < 0 || ty >= wall_h)
    {
        return 0;
    }
    return wall_grid[ty * wall_w + tx] ? 1 : 0;
}

int wall_is_blocked_wide(float x, float y, float r)
{
    if (wall_is_blocked_at(x, y))
    {
        return 1;
    }
    if (r <= 0.0f)
    {
        return 0;
    }
    if (wall_is_blocked_at(x + r, y))
    {
        return 1;
    }
    if (wall_is_blocked_at(x - r, y))
    {
        return 1;
    }
    if (wall_is_blocked_at(x, y + r))
    {
        return 1;
    }
    if (wall_is_blocked_at(x, y - r))
    {
        return 1;
    }
    return 0;
}

int wall_los(float ax, float ay, float bx, float by)
{
    int cx = 0;
    int cy = 0;
    int tx = 0;
    int ty = 0;
    int dx = 0;
    int dy = 0;
    int sx = 0;
    int sy = 0;
    int err = 0;
    int steps = 0;
    if (!wall_have)
    {
        return 1;
    }
    cx = game_logic_to_tile(static_cast<int>(ax));
    cy = game_logic_to_tile(static_cast<int>(ay));
    tx = game_logic_to_tile(static_cast<int>(bx));
    ty = game_logic_to_tile(static_cast<int>(by));
    if (cx == tx && cy == ty)
    {
        return 1;
    }
    dx = tx - cx;
    if (dx < 0)
    {
        dx = -dx;
    }
    dy = ty - cy;
    if (dy < 0)
    {
        dy = -dy;
    }
    dy = -dy;
    sx = cx < tx ? 1 : -1;
    sy = cy < ty ? 1 : -1;
    err = dx + dy;
    steps = dx - dy + 2;
    for (int n = 0; n < steps; n++)
    {
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            cx += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            cy += sy;
        }
        if (cx == tx && cy == ty)
        {
            return 1;
        }
        if (cx < 0 || cx >= wall_w || cy < 0 || cy >= wall_h)
        {
            continue;
        }
        if (wall_grid[cy * wall_w + cx])
        {
            return 0;
        }
    }
    return 1;
}

int wall_engine_blocked_count()
{
    return wall_engine_blocked;
}

int32_t axis(float v, int tiles)
{
    float limit = wall_units(tiles) - 1.0f;
    if (v < 0.0f) return 0;
    if (v > limit) return (int32_t)limit;
    return (int32_t)roundf(v);
}

float wall_pull(float mx, float my, float dx, float dy, float radius)
{
    constexpr int PROBES = 3;
    float reach = radius * 6.0f;
    int blocked = 0;
    for (int i = 1; i <= PROBES; i++)
    {
        float d = reach * 0.5f * (float)i / (float)PROBES;
        blocked +=
            wall_is_blocked_wide(mx + dx * d, my + dy * d, radius) ? 1 : 0;
    }
    float pin_x = (float)axis(mx + dx * reach, wall_cache_w()) - mx;
    float pin_y = (float)axis(my + dy * reach, wall_cache_h()) - my;
    float room = sqrtf(pin_x * pin_x + pin_y * pin_y) / reach;
    return (float)blocked / (float)PROBES + (1.0f - room);
}
