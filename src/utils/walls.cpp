#include "walls.h"
#include "../core/scan.h"
uintptr_t tiles = 0;
int w = 0;
int h = 0;
int cell(int tx, int ty, int* proj, int* move)
{
    void* tile = nullptr;
    void* type = nullptr;
    *proj = -1;
    *move = -1;
    if (!tiles)
    {
        return 0;
    }
    if (tx < 0 || ty < 0 || tx >= w || ty >= h)
    {
        return 0;
    }
    if (!read_ptr(tiles + (uintptr_t)(ty * w + tx) * (uintptr_t)TILE_PTR_STRIDE,
                  &tile))
    {
        return 0;
    }
    if (!tile)
    {
        *proj = 0;
        *move = 0;
        return 1;
    }
    if (!read_ptr((uintptr_t)tile, &type) || !type)
    {
        return 0;
    }
    uint16_t packed = 0;
    if (!read_bytes((uintptr_t)type + TILE_TYPE_MOVE_OFF, &packed, 2))
    {
        return 0;
    }
    *move = (packed & 0xff) ? 1 : 0;
    *proj = (packed >> 8) ? 1 : 0;
    return 1;
}
static uint8_t wall_grid[WALL_MAX_TOTAL_TILES];
static int wall_w = 0;
static int wall_h = 0;
static int wall_have = 0;
static uintptr_t wall_map = 0;
static int wall_dirty = 1;
static uint64_t wall_fail_ms = 0;
int wall_cache_w(void)
{
    return wall_w;
}
int wall_cache_h(void)
{
    return wall_h;
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
        tiles = 0;
        w = 0;
        h = 0;
        wall_dirty = 1;
        wall_have = 0;
        wall_fail_ms = 0;
    }
    if (!read_int(tileMap + (uintptr_t)TILEMAP_W_OFF, &mapW) || mapW <= 0 ||
        mapW > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_int(tileMap + (uintptr_t)TILEMAP_H_OFF, &mapH) || mapH <= 0 ||
        mapH > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_ptr(tileMap + (uintptr_t)TILEMAP_ARRAY_OFF, &tileArray) ||
        !tileArray)
    {
        return;
    }
    if ((uintptr_t)tileArray != tiles || mapW != w || mapH != h)
    {
        tiles = (uintptr_t)tileArray;
        w = mapW;
        h = mapH;
        wall_dirty = 1;
        wall_have = 0;
    }
}
int wall_ready(void)
{
    return wall_have;
}
int wall_build(void)
{
    int width = 0;
    int height = 0;
    int total = 0;
    int i = 0;
    wall_have = 0;
    if (!tiles)
    {
        return 0;
    }
    width = w;
    height = h;
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
    for (i = 0; i < total; i++)
    {
        int tx = i % width;
        int ty = i / width;
        int proj = 0;
        int move = 0;
        uint8_t flags = 0;
        if (cell(tx, ty, &proj, &move))
        {
            if (move)
            {
                flags |= WALL_BLOCKS_MOVEMENT;
            }
            if (proj)
            {
                flags |= WALL_BLOCKS_PROJECTILES;
            }
        }
        wall_grid[i] = flags;
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
    if (now_ms - wall_fail_ms < WALL_RETRY_MS)
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
int wall_is_blocked_at(float x, float y, int mask)
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
    tx = (int)(x / WALL_TILE_SIZE);
    ty = (int)(y / WALL_TILE_SIZE);
    if (tx < 0 || tx >= wall_w || ty < 0 || ty >= wall_h)
    {
        return 0;
    }
    return (wall_grid[ty * wall_w + tx] & mask) ? 1 : 0;
}
int wall_is_blocked_wide(float x, float y, float r, int mask)
{
    float rr = 0.0f;
    if (wall_is_blocked_at(x, y, mask))
    {
        return 1;
    }
    rr = r > 0.0f ? r : 0.0f;
    if (rr <= 0.0f)
    {
        return 0;
    }
    if (wall_is_blocked_at(x + rr, y, mask))
    {
        return 1;
    }
    if (wall_is_blocked_at(x - rr, y, mask))
    {
        return 1;
    }
    if (wall_is_blocked_at(x, y + rr, mask))
    {
        return 1;
    }
    if (wall_is_blocked_at(x, y - rr, mask))
    {
        return 1;
    }
    return 0;
}
int wall_los(float ax, float ay, float bx, float by, int mask)
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
    int n = 0;
    if (!wall_have)
    {
        return 1;
    }
    cx = (int)(ax / WALL_TILE_SIZE);
    cy = (int)(ay / WALL_TILE_SIZE);
    tx = (int)(bx / WALL_TILE_SIZE);
    ty = (int)(by / WALL_TILE_SIZE);
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
    for (n = 0; n < steps; n++)
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
        if (wall_grid[cy * wall_w + cx] & mask)
        {
            return 0;
        }
    }
    return 1;
}