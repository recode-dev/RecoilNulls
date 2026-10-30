#include "walls.h"
#include <cmath>
#include "../core/offsets.h"
#include "../core/scan.h"
static uintptr_t wall_map          = 0;
static int       wall_w            = 0;
static int       wall_h            = 0;
static int       wall_tile_span    = 0;
static int       wall_subtile_span = 0;
static int       wall_have         = 0;
static int wall_measure_tile_span()
{
    int span = game_tile_to_logic(1) - game_tile_to_logic(0);
    if (span <= 0) return 0;
    if (span % WALL_SUBTILES_PER_TILE) return 0;
    wall_tile_span    = span;
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
    if (subX >= wall_w * WALL_SUBTILES_PER_TILE ||
        subY >= wall_h * WALL_SUBTILES_PER_TILE)
    {
        return 0;
    }
    if (outX) *outX = subX;
    if (outY) *outY = subY;
    return 1;
}
float wall_logic_units(int tiles)
{
    if (wall_tile_span > 0) return static_cast<float>(tiles * wall_tile_span);
    return static_cast<float>(game_tile_to_logic(tiles) -
                              game_tile_to_logic(0));
}
void wall_cache_bind(uintptr_t tileMap)
{
    void*   tileArray = nullptr;
    int32_t mapW      = 0;
    int32_t mapH      = 0;
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
    if (!read_int(tileMap + static_cast<uintptr_t>(LOGICTILEMAP__WIDTH),
                  &mapW) ||
        mapW <= 0 || mapW > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(LOGICTILEMAP__HEIGHT),
                  &mapH) ||
        mapH <= 0 || mapH > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_ptr(tileMap + static_cast<uintptr_t>(LOGICTILEMAP__TILEARRAY),
                  &tileArray) ||
        !tileArray)
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
    return game_tile_is_passable(wall_map, subX, subY) ? 0 : 1;
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
    if (highX >= wall_w * WALL_SUBTILES_PER_TILE)
        highX = wall_w * WALL_SUBTILES_PER_TILE - 1;
    if (highY >= wall_h * WALL_SUBTILES_PER_TILE)
        highY = wall_h * WALL_SUBTILES_PER_TILE - 1;
    for (int sy = lowY; sy <= highY; sy++)
    {
        for (int sx = lowX; sx <= highX; sx++)
        {
            if (!game_tile_is_passable(wall_map, sx, sy)) return 1;
        }
    }
    return 0;
}
int wall_line_of_sight_clear(float ax, float ay, float bx, float by)
{
    static uintptr_t slot = 0;
    if (!wall_map) return 1;
    if (!slot) slot = callable(LOGICTILEMAP__ISPLAYERLINEOFSIGHTCLEAR);
    if (!slot) return 1;
    using los_t = int (*)(uintptr_t, int, int, int, int, int, int, int, int);
    return reinterpret_cast<los_t>(slot)(wall_map,
                                         static_cast<int>(ax),
                                         static_cast<int>(ay),
                                         static_cast<int>(bx),
                                         static_cast<int>(by),
                                         0,
                                         0,
                                         0,
                                         0) &
           1;
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
        if (!game_path_clear(wall_map,
                             wall_grid_x(ax + ox),
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
        float d = reach * 0.5f * static_cast<float>(i) /
                  static_cast<float>(WALL_PROBES);
        blocked +=
            wall_cell_blocked_wide(mx + dx * d, my + dy * d, radius) ? 1 : 0;
    }
    float pin_x =
        static_cast<float>(axis(mx + dx * reach, wall_cache_width())) - mx;
    float pin_y =
        static_cast<float>(axis(my + dy * reach, wall_cache_height())) - my;
    int   pin_ix = static_cast<int>(pin_x + (pin_x < 0.0f ? -0.5f : 0.5f));
    int   pin_iy = static_cast<int>(pin_y + (pin_y < 0.0f ? -0.5f : 0.5f));
    float room =
        static_cast<float>(game_distance(0, 0, pin_ix, pin_iy)) / reach;
    return static_cast<float>(blocked) / static_cast<float>(WALL_PROBES) +
           (1.0f - room);
}
