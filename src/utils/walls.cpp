#include "walls.h"

#include <cmath>
#include <cstring>

#include "../core/offsets.h"
#include "../core/scan.h"

static constexpr int WALL_MASK_PLANES = 4;
static constexpr int WALL_SAMPLE_TILES = 24;
static constexpr int WALL_BITS_PER_TILE =
    WALL_SUBTILES_PER_TILE * WALL_SUBTILES_PER_TILE;
static constexpr int WALL_SUBTILE_TOTAL =
    WALL_MAX_TOTAL_TILES * WALL_BITS_PER_TILE;
static constexpr int WALL_SUBTILE_WORDS = (WALL_SUBTILE_TOTAL + 31) / 32;
static constexpr int WALL_TILE_WORDS = (WALL_MAX_TOTAL_TILES + 31) / 32;
static constexpr uint64_t WALL_SUBTILE_BITS = 0x1ffULL;
static constexpr float WALL_FAR_T = 1.0e30f;

struct wall_tile_window
{
    uintptr_t data;
    uint64_t planes[WALL_MASK_PLANES];
};

static uint32_t wall_movement_bits[WALL_SUBTILE_WORDS];
static uint32_t wall_projectile_bits[WALL_TILE_WORDS];
static uintptr_t wall_pointers[WALL_MAX_TOTAL_TILES];
static uintptr_t wall_map = 0;
static uintptr_t wall_tiles = 0;
static uintptr_t wall_flag_source = 0;
static uint16_t wall_flag_value = 0;
static int wall_tiles_w = 0;
static int wall_tiles_h = 0;
static int wall_w = 0;
static int wall_h = 0;
static int wall_tile_span = 0;
static int wall_subtile_span = 0;
static int wall_have = 0;
static int wall_dirty = 1;
static int wall_planes = WALL_MASK_PLANES;
static int wall_blocked_tiles = 0;
static int wall_script_blocked_tiles = 0;
static uint64_t wall_fail_ms = 0;
static uint64_t wall_next_ms = 0;

static inline int wall_bit_test(const uint32_t* words, int index)
{
    return static_cast<int>((words[index >> 5] >> (index & 31)) & 1u);
}

static inline void wall_bit_raise(uint32_t* words, int index)
{
    words[index >> 5] |= 1u << (index & 31);
}

static inline int wall_subtile_index(int subX, int subY)
{
    int tileX = subX / WALL_SUBTILES_PER_TILE;
    int tileY = subY / WALL_SUBTILES_PER_TILE;
    return (tileY * wall_w + tileX) * WALL_BITS_PER_TILE +
           (subY % WALL_SUBTILES_PER_TILE) * WALL_SUBTILES_PER_TILE +
           subX % WALL_SUBTILES_PER_TILE;
}

static int wall_measure_tile_span()
{
    int span = game_tile_to_logic(1) - game_tile_to_logic(0);
    if (span <= 0 || span % WALL_SUBTILES_PER_TILE) return 0;
    wall_tile_span = span;
    wall_subtile_span = span / WALL_SUBTILES_PER_TILE;
    return 1;
}

static uint16_t wall_tile_data_flags(uintptr_t tileData)
{
    uint16_t flags = 0;
    if (!tileData || !pointer_plausible(tileData)) return 0;
    if (tileData == wall_flag_source) return wall_flag_value;
    if (!read_bytes(tileData + LOGIC_TILE_DATA_BLOCKS_MOVEMENT_OFF, &flags, 2))
        return 0;
    wall_flag_source = tileData;
    wall_flag_value = flags;
    return flags;
}

static int wall_read_tile(uintptr_t tile, wall_tile_window* window)
{
    uint8_t raw[LOGIC_TILE_MASK_ARRAY_OFF +
                WALL_MASK_PLANES * LOGIC_TILE_MASK_PLANE_SIZE];
    int length = static_cast<int>(sizeof(raw));
    if (!tile || !window) return 0;
    if (!read_bytes(tile, raw, sizeof(raw)))
    {
        memset(raw, 0, sizeof(raw));
        length -= static_cast<int>(LOGIC_TILE_MASK_PLANE_SIZE);
        if (!read_bytes(tile, raw, static_cast<size_t>(length))) return 0;
    }
    memcpy(
        &window->data, raw + LOGIC_TILE_DATA_POINTER_OFF, sizeof(window->data));
    memcpy(window->planes,
           raw + LOGIC_TILE_MASK_ARRAY_OFF,
           sizeof(window->planes));
    return 1;
}

static uint64_t wall_plane_union(const wall_tile_window* window, int planes)
{
    uint64_t combined = 0;
    for (int i = 0; i < planes; i++)
    {
        combined |= window->planes[i];
    }
    return combined;
}

static void wall_build_planes(int planes)
{
    int total = wall_tiles_w * wall_tiles_h;
    int layerShift = WALL_PATHFINDER_LAYER * WALL_BITS_PER_TILE;
    memset(wall_movement_bits, 0, sizeof(wall_movement_bits));
    memset(wall_projectile_bits, 0, sizeof(wall_projectile_bits));
    wall_flag_source = 0;
    wall_flag_value = 0;
    wall_blocked_tiles = 0;
    wall_script_blocked_tiles = 0;
    for (int i = 0; i < total; i++)
    {
        uintptr_t tile = 0;
        wall_tile_window window;
        uint16_t flags = 0;
        uint32_t subtiles = 0;
        memcpy(&tile,
               reinterpret_cast<const uint8_t*>(wall_pointers) +
                   static_cast<size_t>(i) * sizeof(uintptr_t),
               sizeof(tile));
        if (!tile || !pointer_plausible(tile) || !wall_read_tile(tile, &window))
            continue;
        flags = wall_tile_data_flags(window.data);
        if (flags & 0xffu) wall_script_blocked_tiles++;
        if (flags >> 8) wall_bit_raise(wall_projectile_bits, i);
        if (planes > 0)
        {
            subtiles = static_cast<uint32_t>(
                (wall_plane_union(&window, planes) >> layerShift) &
                WALL_SUBTILE_BITS);
        }
        else if (flags & 0xffu)
        {
            subtiles = static_cast<uint32_t>(WALL_SUBTILE_BITS);
        }
        if (!subtiles) continue;
        wall_blocked_tiles++;
        for (int bit = 0; bit < WALL_BITS_PER_TILE; bit++)
        {
            if ((subtiles >> bit) & 1u)
            {
                wall_bit_raise(wall_movement_bits,
                               i * WALL_BITS_PER_TILE + bit);
            }
        }
    }
}

static int wall_count_mismatches(int* enginePassable)
{
    int width = wall_tiles_w;
    int total = width * wall_tiles_h;
    int stride = total / WALL_SAMPLE_TILES;
    int mismatches = 0;
    int sampled = 0;
    int passable = 0;
    if (stride < 1) stride = 1;
    for (int i = 0; i < total && sampled < WALL_SAMPLE_TILES;
         i += stride, sampled++)
    {
        int baseX = (i % width) * WALL_SUBTILES_PER_TILE;
        int baseY = (i / width) * WALL_SUBTILES_PER_TILE;
        for (int sy = 0; sy < WALL_SUBTILES_PER_TILE; sy++)
        {
            for (int sx = 0; sx < WALL_SUBTILES_PER_TILE; sx++)
            {
                int engine =
                    game_tile_is_passable(wall_map, baseX + sx, baseY + sy);
                int ours = 1 - wall_bit_test(
                                   wall_movement_bits,
                                   wall_subtile_index(baseX + sx, baseY + sy));
                if (engine) passable++;
                if ((engine != 0) != (ours != 0)) mismatches++;
            }
        }
    }
    if (enginePassable) *enginePassable = passable;
    return mismatches;
}

static int wall_planes_match_engine(int planes)
{
    int passable = 0;
    wall_build_planes(planes);
    if (wall_count_mismatches(&passable) == 0) return 1;
    return !passable;
}

static int wall_subtile_of(float x, float y, int* outX, int* outY)
{
    int subX = 0;
    int subY = 0;
    if (!wall_have || wall_subtile_span <= 0) return 0;
    subX = static_cast<int>(x) / wall_subtile_span;
    subY = static_cast<int>(y) / wall_subtile_span;
    if (subX < 0 || subY < 0) return 0;
    if (subX / WALL_SUBTILES_PER_TILE >= wall_w ||
        subY / WALL_SUBTILES_PER_TILE >= wall_h)
    {
        return 0;
    }
    if (outX) *outX = subX;
    if (outY) *outY = subY;
    return 1;
}

static int wall_trace_clear(int ax, int ay, int bx, int by, int projectile)
{
    int span = projectile ? wall_tile_span : wall_subtile_span;
    int gridW = projectile ? wall_w : wall_w * WALL_SUBTILES_PER_TILE;
    int gridH = projectile ? wall_h : wall_h * WALL_SUBTILES_PER_TILE;
    int cellX = ax / span;
    int cellY = ay / span;
    int lastX = bx / span;
    int lastY = by / span;
    int stepX = lastX > cellX ? 1 : (lastX < cellX ? -1 : 0);
    int stepY = lastY > cellY ? 1 : (lastY < cellY ? -1 : 0);
    float deltaX = static_cast<float>(bx - ax);
    float deltaY = static_cast<float>(by - ay);
    float tMaxX = WALL_FAR_T;
    float tMaxY = WALL_FAR_T;
    float tDeltaX = WALL_FAR_T;
    float tDeltaY = WALL_FAR_T;
    if (cellX == lastX && cellY == lastY) return 1;
    if (stepX)
    {
        float edge = static_cast<float>((stepX > 0 ? cellX + 1 : cellX) * span);
        tMaxX = (edge - static_cast<float>(ax)) / deltaX;
        tDeltaX = static_cast<float>(span) / fabsf(deltaX);
    }
    if (stepY)
    {
        float edge = static_cast<float>((stepY > 0 ? cellY + 1 : cellY) * span);
        tMaxY = (edge - static_cast<float>(ay)) / deltaY;
        tDeltaY = static_cast<float>(span) / fabsf(deltaY);
    }
    for (int guard = 0; guard <= gridW + gridH + 2; guard++)
    {
        if (tMaxX < tMaxY)
        {
            cellX += stepX;
            tMaxX += tDeltaX;
        }
        else if (tMaxY < tMaxX)
        {
            cellY += stepY;
            tMaxY += tDeltaY;
        }
        else
        {
            cellX += stepX;
            cellY += stepY;
            tMaxX += tDeltaX;
            tMaxY += tDeltaY;
        }
        if (cellX == lastX && cellY == lastY) return 1;
        if (cellX < 0 || cellY < 0 || cellX >= gridW || cellY >= gridH)
            return 1;
        if (projectile
                ? wall_bit_test(wall_projectile_bits, cellY * gridW + cellX)
                : wall_bit_test(wall_movement_bits,
                                wall_subtile_index(cellX, cellY)))
        {
            return 0;
        }
    }
    return 1;
}

float wall_logic_units(int tiles)
{
    if (wall_tile_span > 0) return static_cast<float>(tiles * wall_tile_span);
    return static_cast<float>(game_tile_to_logic(tiles) -
                              game_tile_to_logic(0));
}

int wall_cell_is_passable(float x, float y)
{
    int subX = 0;
    int subY = 0;
    if (!wall_subtile_of(x, y, &subX, &subY)) return 0;
    return wall_bit_test(wall_movement_bits, wall_subtile_index(subX, subY))
               ? 0
               : 1;
}

int wall_cached_blocked_count()
{
    return wall_blocked_tiles;
}

int wall_engine_blocked_total()
{
    return wall_script_blocked_tiles;
}

int wall_cache_width()
{
    return wall_w;
}

int wall_cache_height()
{
    return wall_h;
}

void wall_cache_drop()
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
    wall_next_ms = 0;
}

void wall_cache_bind(uintptr_t tileMap)
{
    void* tileArray = nullptr;
    int32_t mapW = 0;
    int32_t mapH = 0;
    if (!tileMap) return;
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
        wall_next_ms = 0;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(LOGIC_TILE_MAP_WIDTH_OFF),
                  &mapW) ||
        mapW <= 0 || mapW > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(LOGIC_TILE_MAP_HEIGHT_OFF),
                  &mapH) ||
        mapH <= 0 || mapH > WALL_MAX_MAP_TILES)
    {
        return;
    }
    if (!read_ptr(tileMap + static_cast<uintptr_t>(LOGIC_TILE_MAP_TILES_OFF),
                  &tileArray) ||
        !tileArray)
    {
        return;
    }
    if (reinterpret_cast<uintptr_t>(tileArray) != wall_tiles ||
        mapW != wall_tiles_w || mapH != wall_tiles_h)
    {
        wall_tiles = reinterpret_cast<uintptr_t>(tileArray);
        wall_tiles_w = mapW;
        wall_tiles_h = mapH;
        wall_have = 0;
        wall_dirty = 1;
    }
}

int wall_cache_build()
{
    int width = wall_tiles_w;
    int height = wall_tiles_h;
    int chosen = -1;
    if (!wall_tiles || !wall_measure_tile_span()) return 0;
    if (width <= 0 || width > WALL_MAX_MAP_TILES) return 0;
    if (height <= 0 || height > WALL_MAX_MAP_TILES) return 0;
    if (!read_bytes(wall_tiles,
                    wall_pointers,
                    static_cast<size_t>(width * height) * sizeof(uintptr_t)))
    {
        return 0;
    }
    wall_have = 0;
    wall_w = width;
    wall_h = height;
    if (wall_planes > 0 && wall_planes_match_engine(wall_planes))
        chosen = wall_planes;
    for (int planes = WALL_MASK_PLANES; chosen < 0 && planes > 0; planes--)
    {
        if (planes != wall_planes && wall_planes_match_engine(planes))
            chosen = planes;
    }
    if (chosen < 0)
    {
        wall_build_planes(0);
        chosen = 0;
    }
    wall_planes = chosen;
    wall_have = 1;
    wall_dirty = 0;
    return 1;
}

int wall_cache_refresh_if_stale(uint64_t now_ms)
{
    if (!wall_dirty && now_ms < wall_next_ms) return 0;
    if (wall_fail_ms &&
        now_ms - wall_fail_ms < static_cast<uint64_t>(WALL_RETRY_MS))
        return 0;
    if (!wall_cache_build())
    {
        wall_dirty = 1;
        wall_fail_ms = now_ms;
        wall_next_ms = 0;
        return 0;
    }
    wall_next_ms = now_ms + static_cast<uint64_t>(WALL_REFRESH_MS);
    return 1;
}

int wall_cell_blocked_at(float x, float y)
{
    int subX = 0;
    int subY = 0;
    if (!wall_subtile_of(x, y, &subX, &subY)) return 0;
    return wall_bit_test(wall_movement_bits, wall_subtile_index(subX, subY));
}

int wall_cell_blocked_wide(float x, float y, float r)
{
    int lowX = 0;
    int lowY = 0;
    int highX = 0;
    int highY = 0;
    if (r <= 0.0f) return wall_cell_blocked_at(x, y);
    if (!wall_have || wall_subtile_span <= 0) return 0;
    lowX = static_cast<int>(x - r) / wall_subtile_span;
    lowY = static_cast<int>(y - r) / wall_subtile_span;
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
            if (wall_bit_test(wall_movement_bits, wall_subtile_index(sx, sy)))
                return 1;
        }
    }
    return 0;
}

int wall_line_of_sight_clear(float ax, float ay, float bx, float by)
{
    if (!wall_have || wall_tile_span <= 0) return 1;
    return wall_trace_clear(static_cast<int>(ax),
                            static_cast<int>(ay),
                            static_cast<int>(bx),
                            static_cast<int>(by),
                            1);
}

int wall_path_clear(float ax, float ay, float bx, float by)
{
    if (!wall_have || wall_subtile_span <= 0) return 1;
    return wall_trace_clear(static_cast<int>(ax),
                            static_cast<int>(ay),
                            static_cast<int>(bx),
                            static_cast<int>(by),
                            0);
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
    float reach = radius * 6.0f;
    int blocked = 0;
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
    float room = sqrtf(pin_x * pin_x + pin_y * pin_y) / reach;
    return static_cast<float>(blocked) / static_cast<float>(WALL_PROBES) +
           (1.0f - room);
}
