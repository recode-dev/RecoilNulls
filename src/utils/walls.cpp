#include "walls.h"
#include <cmath>
#include "../core/offsets.h"
#include "../core/scan.h"
static uintptr_t wall_map          = 0;
static int       wall_w            = 0;
static int       wall_h            = 0;
static int       wall_subtile_span = 0;
static int       wall_have         = 0;

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
constexpr int   PATH_MEMO_SLOTS = 256;

struct PathMemo
{
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    uint8_t clear;
    uint8_t pad;
    int32_t stamp;
};

static PathMemo path_memo[PATH_MEMO_SLOTS];
constexpr int   LOS_MEMO_SLOTS = 128;

struct LosMemo
{
    int32_t ax;
    int32_t ay;
    int32_t bx;
    int32_t by;
    uint8_t clear;
    uint8_t pad;
    int32_t stamp;
};

static LosMemo los_memo[LOS_MEMO_SLOTS];

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

int path_clear_cached(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int32_t   stamp = static_cast<int32_t>(frame_index);
    PathMemo& e     = path_memo[(memo_mix(static_cast<uint32_t>(x0), static_cast<uint32_t>(y0)) ^
                             memo_mix(static_cast<uint32_t>(x1), static_cast<uint32_t>(y1))) &
                            (PATH_MEMO_SLOTS - 1)];
    if (e.stamp == stamp && e.x0 == x0 && e.y0 == y0 && e.x1 == x1 && e.y1 == y1)
    {
        return e.clear;
    }
    int clear = game_path_clear(wall_map, x0, y0, x1, y1) ? 1 : 0;
    e.x0      = x0;
    e.y0      = y0;
    e.x1      = x1;
    e.y1      = y1;
    e.clear   = static_cast<uint8_t>(clear);
    e.stamp   = stamp;
    return clear;
}

int los_clear_cached(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    using los_t            = int (*)(uintptr_t, int, int, int, int, int, int, int, int);
    static uintptr_t slot  = 0;
    int32_t          stamp = static_cast<int32_t>(frame_index);
    LosMemo&         e = los_memo[(memo_mix(static_cast<uint32_t>(ax), static_cast<uint32_t>(ay)) ^
                           memo_mix(static_cast<uint32_t>(bx), static_cast<uint32_t>(by))) &
                          (LOS_MEMO_SLOTS - 1)];
    if (e.stamp == stamp && e.ax == ax && e.ay == ay && e.bx == bx && e.by == by)
    {
        return e.clear;
    }
    if (!slot) slot = callable(LOGICTILEMAP__ISPLAYERLINEOFSIGHTCLEAR);
    if (!slot) return 1;
    int clear = reinterpret_cast<los_t>(slot)(wall_map, ax, ay, bx, by, 0, 0, 0, 0) & 1;
    e.ax      = ax;
    e.ay      = ay;
    e.bx      = bx;
    e.by      = by;
    e.clear   = static_cast<uint8_t>(clear);
    e.stamp   = stamp;
    return clear;
}
}

static int wall_measure_tile_span()
{
    int span = game_tile_to_logic(1) - game_tile_to_logic(0);
    if (span <= 0) return 0;
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
    static int   cached_tiles[4] = {-1, -1, -1, -1};
    static float cached_units[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int          slot            = static_cast<int>(static_cast<unsigned>(tiles) & 3u);
    if (cached_tiles[slot] == tiles) return cached_units[slot];
    float units        = static_cast<float>(game_tile_to_logic(tiles) - game_tile_to_logic(0));
    cached_tiles[slot] = tiles;
    cached_units[slot] = units;
    return units;
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

int wall_line_of_sight_clear(float ax, float ay, float bx, float by)
{
    if (!wall_map) return 1;
    return los_clear_cached(static_cast<int32_t>(ax),
                            static_cast<int32_t>(ay),
                            static_cast<int32_t>(bx),
                            static_cast<int32_t>(by));
}

int wall_path_clear(float ax, float ay, float bx, float by)
{
    if (!wall_have) return 1;
    return wall_line_of_sight_clear(ax, ay, bx, by);
}

static int wall_grid_x(float x)
{
    int limit = wall_w * WALL_SUBTILES_PER_TILE - 1;
    int cell  = static_cast<int>(x) / wall_subtile_span;
    if (cell < 0) return 0;
    if (cell > limit) return limit;
    return cell;
}

static int wall_grid_y(float y)
{
    int limit = wall_h * WALL_SUBTILES_PER_TILE - 1;
    int cell  = static_cast<int>(y) / wall_subtile_span;
    if (cell < 0) return 0;
    if (cell > limit) return limit;
    return cell;
}

int wall_segment_clear(float ax, float ay, float bx, float by, float radius)
{
    if (!wall_have || wall_subtile_span <= 0) return 1;
    float len = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
    float nx = 0.0f, ny = 0.0f;
    if (len > 0.5f && radius > 0.0f)
    {
        nx = -(by - ay) / len * radius;
        ny = (bx - ax) / len * radius;
    }
    const float lanes[3] = {0.0f, 1.0f, -1.0f};
    for (float lane : lanes)
    {
        float ox = nx * lane, oy = ny * lane;
        if (!path_clear_cached(wall_grid_x(ax + ox),
                               wall_grid_y(ay + oy),
                               wall_grid_x(bx + ox),
                               wall_grid_y(by + oy)))
        {
            return 0;
        }
        if (radius <= 0.0f) break;
    }
    return 1;
}

int32_t axis(float v, int tiles)
{
    float limit = wall_logic_units(tiles) - 1.0f;
    if (v < 0.0f) return 0;
    if (v > limit) return static_cast<int32_t>(limit);
    return static_cast<int32_t>(roundf(v));
}

float wall_pull(float mx, float my, float dx, float dy, float radius)
{
    constexpr int WALL_PROBES = 3;
    float         reach       = radius * 6.0f;
    int           blocked     = 0;
    for (int i = 1; i <= WALL_PROBES; i++)
    {
        float d = reach * 0.5f * static_cast<float>(i) / static_cast<float>(WALL_PROBES);
        blocked += wall_cell_blocked_wide(mx + dx * d, my + dy * d, radius) ? 1 : 0;
    }
    float pin_x = static_cast<float>(axis(mx + dx * reach, wall_cache_width())) - mx;
    float pin_y = static_cast<float>(axis(my + dy * reach, wall_cache_height())) - my;
    int   ix    = static_cast<int>(pin_x + (pin_x < 0.0f ? -0.5f : 0.5f));
    int   iy    = static_cast<int>(pin_y + (pin_y < 0.0f ? -0.5f : 0.5f));
    float room  = sqrtf(static_cast<float>(ix * ix + iy * iy)) / reach;
    return static_cast<float>(blocked) / static_cast<float>(WALL_PROBES) + (1.0f - room);
}
