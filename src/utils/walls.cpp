#include "walls.h"
#include <cmath>
#include "../core/offsets.h"
#include "../core/scan.h"
static uintptr_t wall_map          = 0;
static int       wall_w            = 0;
static int       wall_h            = 0;
static int       wall_subtile_span = 0;
static int       wall_have         = 0;
static float     wall_unit         = 0.0f;

namespace
{
constexpr int TILE_MEMO_SLOTS = 512;

struct TileMemo
{
    int32_t x;
    int32_t y;
    uint8_t blocked;
    uint8_t pad;
    int32_t stamp;
};

static TileMemo tile_memo[TILE_MEMO_SLOTS];

uint32_t memo_mix(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u;
    h ^= b * 0x85EBCA77u;
    h ^= h >> 13;
    return h;
}

int tile_blocked_cached(int32_t x, int32_t y)
{
    int32_t   stamp = static_cast<int32_t>(frame_index);
    TileMemo& e     = tile_memo[memo_mix(static_cast<uint32_t>(x), static_cast<uint32_t>(y)) &
                                (TILE_MEMO_SLOTS - 1)];
    if (e.stamp == stamp && e.x == x && e.y == y)
    {
        return e.blocked;
    }
    int blocked = game_tile_is_passable(wall_map, x, y) ? 0 : 1;
    e.x         = x;
    e.y         = y;
    e.blocked   = static_cast<uint8_t>(blocked);
    e.stamp     = stamp;
    return blocked;
}
}

static int wall_measure_tile_span()
{
    int span = game_tile_to_logic(1) - game_tile_to_logic(0);
    if (span <= 0) return 0;
    wall_unit = static_cast<float>(span);
    if (span % WALL_SUBTILES_PER_TILE) return 0;
    wall_subtile_span = span / WALL_SUBTILES_PER_TILE;
    return 1;
}

static int wall_subtile_of(float x, float y, int* outX, int* outY)
{
    int subX = 0;
    int subY = 0;
    if (!wall_have || wall_subtile_span <= 0) return 0;
    subX = static_cast<int>(x) / wall_subtile_span;
    subY = static_cast<int>(y) / wall_subtile_span;
    if (subX < 0 || subY < 0) return 0;
    if (subX >= wall_w * WALL_SUBTILES_PER_TILE || subY >= wall_h * WALL_SUBTILES_PER_TILE)
    {
        return 0;
    }
    if (outX) *outX = subX;
    if (outY) *outY = subY;
    return 1;
}

float wall_logic_units(int tiles)
{
    if (!wall_unit)
    {
        int span  = game_tile_to_logic(1) - game_tile_to_logic(0);
        wall_unit = span > 0 ? static_cast<float>(span) : 0.0f;
    }
    return wall_unit * static_cast<float>(tiles);
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
    if (wall_have && wall_w > 0 && wall_h > 0 && tileMap == wall_map) return;
    if (!read_int(tileMap + static_cast<uintptr_t>(LOGICTILEMAP__WIDTH), &mapW) || mapW <= 0 ||
        mapW > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(LOGICTILEMAP__HEIGHT), &mapH) || mapH <= 0 ||
        mapH > WALL_MAX_MAP_TILES)
    {
        return;
    }
    wall_w    = mapW;
    wall_h    = mapH;
    wall_have = wall_measure_tile_span();
}

int wall_cache_width()
{
    return wall_w;
}

int wall_cache_height()
{
    return wall_h;
}

int wall_cell_blocked_at(float x, float y)
{
    int subX = 0;
    int subY = 0;
    if (!wall_subtile_of(x, y, &subX, &subY)) return 0;
    return tile_blocked_cached(subX, subY);
}

int wall_cell_blocked_wide(float x, float y, float r)
{
    int lowX  = 0;
    int lowY  = 0;
    int highX = 0;
    int highY = 0;
    if (r <= 0.0f) return wall_cell_blocked_at(x, y);
    if (!wall_have || wall_subtile_span <= 0) return 0;
    lowX  = static_cast<int>(x - r) / wall_subtile_span;
    lowY  = static_cast<int>(y - r) / wall_subtile_span;
    highX = static_cast<int>(x + r) / wall_subtile_span;
    highY = static_cast<int>(y + r) / wall_subtile_span;
    if (lowX < 0) lowX = 0;
    if (lowY < 0) lowY = 0;
    if (highX >= wall_w * WALL_SUBTILES_PER_TILE) highX = wall_w * WALL_SUBTILES_PER_TILE - 1;
    if (highY >= wall_h * WALL_SUBTILES_PER_TILE) highY = wall_h * WALL_SUBTILES_PER_TILE - 1;
    for (int sy = lowY; sy <= highY; sy++)
    {
        for (int sx = lowX; sx <= highX; sx++)
        {
            if (tile_blocked_cached(sx, sy)) return 1;
        }
    }
    return 0;
}

float wall_clear_run(float x, float y, float dx, float dy, float max_run, float radius)
{
    if (max_run <= 0.0f) return 0.0f;
    if (!wall_have || wall_subtile_span <= 0) return max_run;
    const float step = wall_unit / static_cast<float>(WALL_SUBTILES_PER_TILE);
    float       run  = 0.0f;
    float       d    = 0.0f;
    while (d <= max_run)
    {
        const float t = d > max_run ? max_run : d;
        if (wall_cell_blocked_wide(x + dx * t, y + dy * t, radius)) break;
        run = t;
        if (t >= max_run) break;
        d += step;
    }
    return run;
}

int32_t axis(float v, int tiles)
{
    float limit = wall_logic_units(tiles) - 1.0f;
    if (v < 0.0f) return 0;
    if (v > limit) return static_cast<int32_t>(limit);
    return static_cast<int32_t>(roundf(v));
}
