#include "walls.h"
#include "../core/scan.h"
#include <array>
#include <cstring>

inline constexpr int WALL_TYPE_CACHE_MAX = 128;

static uint8_t wall_grid[WALL_MAX_TOTAL_TILES];
static std::array<uint8_t,
                  static_cast<size_t>(WALL_MAX_TOTAL_TILES) * sizeof(uintptr_t)>
    wall_raw{};
static uintptr_t wall_tiles = 0;
static int wall_tiles_w = 0;
static int wall_tiles_h = 0;
static int wall_w = 0;
static int wall_h = 0;
static int wall_have = 0;
static int wall_hole_n = 0;
static uintptr_t wall_map = 0;
static int wall_dirty = 1;
static uint64_t wall_fail_ms = 0;
static uintptr_t wall_type_key[WALL_TYPE_CACHE_MAX];
static uint8_t wall_type_bits[WALL_TYPE_CACHE_MAX];
static int wall_type_n = 0;
static int wall_type_next = 0;

static void wall_types_clear(void)
{
    wall_type_n = 0;
    wall_type_next = 0;
}

static int wall_type_bits_for(uintptr_t type, uint8_t* out)
{
    uint16_t packed = 0;
    uint8_t bits = 0;
    int i = 0;
    for (i = 0; i < wall_type_n; i++)
    {
        if (wall_type_key[i] == type)
        {
            *out = wall_type_bits[i];
            return 1;
        }
    }
    if (!read_bytes(
            type + static_cast<uintptr_t>(TILE_TYPE_MOVE_OFF), &packed, 2))
    {
        return 0;
    }
    if (packed & 0xff)
    {
        bits |= WALL_BLOCKS_MOVEMENT;
    }
    if (packed >> 8)
    {
        bits |= WALL_BLOCKS_PROJECTILES;
    }
    if (wall_type_n < WALL_TYPE_CACHE_MAX)
    {
        wall_type_key[wall_type_n] = type;
        wall_type_bits[wall_type_n] = bits;
        wall_type_n++;
    }
    else
    {
        wall_type_key[wall_type_next] = type;
        wall_type_bits[wall_type_next] = bits;
        wall_type_next = (wall_type_next + 1) % WALL_TYPE_CACHE_MAX;
    }
    *out = bits;
    return 1;
}

int wall_cache_w(void)
{
    return wall_w;
}

int wall_cache_h(void)
{
    return wall_h;
}

int wall_ready(void)
{
    return wall_have;
}

int wall_holes(void)
{
    return wall_hole_n;
}

void wall_drop(void)
{
    wall_tiles = 0;
    wall_tiles_w = 0;
    wall_tiles_h = 0;
    wall_map = 0;
    wall_w = 0;
    wall_h = 0;
    wall_have = 0;
    wall_hole_n = 0;
    wall_dirty = 1;
    wall_fail_ms = 0;
    wall_types_clear();
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
        wall_hole_n = 0;
        wall_dirty = 1;
        wall_fail_ms = 0;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(TILEMAP_W_OFF), &mapW) ||
        mapW <= 0 || mapW > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(TILEMAP_H_OFF), &mapH) ||
        mapH <= 0 || mapH > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_ptr(tileMap + static_cast<uintptr_t>(TILEMAP_ARRAY_OFF),
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

int wall_build(void)
{
    int width = wall_tiles_w;
    int height = wall_tiles_h;
    int total = 0;
    int holes = 0;
    int i = 0;
    wall_have = 0;
    wall_hole_n = 0;
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
                    wall_raw.data(),
                    static_cast<size_t>(total) * sizeof(uintptr_t)))
    {
        return 0;
    }
    wall_types_clear();
    for (i = 0; i < total; i++)
    {
        uintptr_t tile = 0;
        uintptr_t type = 0;
        uint8_t bits = 0;
        wall_grid[i] = 0;
        memcpy(&tile,
               wall_raw.data() + static_cast<size_t>(i) * sizeof(uintptr_t),
               sizeof(uintptr_t));
        if (!tile)
        {
            continue;
        }
        // read_ptr принимает void** (out-param) — static_cast не применим
        if (!pointer_plausible(tile) ||
            !read_ptr(tile, reinterpret_cast<void**>(&type)))
        {
            holes++;
            continue;
        }
        if (!type)
        {
            continue;
        }
        if (!pointer_plausible(type) || !wall_type_bits_for(type, &bits))
        {
            holes++;
            continue;
        }
        wall_grid[i] = bits;
    }
    if (holes)
    {
        wall_hole_n = holes;
        return 0;
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
    tx = static_cast<int>(x / WALL_TILE_SIZE);
    ty = static_cast<int>(y / WALL_TILE_SIZE);
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
    cx = static_cast<int>(ax / WALL_TILE_SIZE);
    cy = static_cast<int>(ay / WALL_TILE_SIZE);
    tx = static_cast<int>(bx / WALL_TILE_SIZE);
    ty = static_cast<int>(by / WALL_TILE_SIZE);
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
