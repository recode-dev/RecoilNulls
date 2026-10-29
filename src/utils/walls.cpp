#include "walls.h"
#include "../core/scan.h"
#include <array>
#include <cmath>
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
static uintptr_t wall_map = 0;
static int wall_dirty = 1;
static uint64_t wall_fail_ms = 0;
static uintptr_t wall_type_key[WALL_TYPE_CACHE_MAX];
static uint8_t wall_type_bits[WALL_TYPE_CACHE_MAX];
static int wall_type_n = 0;
static int wall_type_next = 0;

static void wall_types_clear()
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
    if (!read_bytes(type + static_cast<uintptr_t>(TILETYPEDATA_BLOCKFLAGS_OFF),
                    &packed,
                    2))
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
    int holes = 0;
    int i = 0;
    wall_have = 0;
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
        if (!pointer_plausible(tile) || !read_ptr(tile, &type))
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
    if (holes) return 0;
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
    tx = game_logic_to_tile(static_cast<int>(x));
    ty = game_logic_to_tile(static_cast<int>(y));
    if (tx < 0 || tx >= wall_w || ty < 0 || ty >= wall_h)
    {
        return 0;
    }
    return (wall_grid[ty * wall_w + tx] & mask) ? 1 : 0;
}

int wall_is_blocked_wide(float x, float y, float r, int mask)
{
    if (wall_is_blocked_at(x, y, mask))
    {
        return 1;
    }
    if (r <= 0.0f)
    {
        return 0;
    }
    if (wall_is_blocked_at(x + r, y, mask))
    {
        return 1;
    }
    if (wall_is_blocked_at(x - r, y, mask))
    {
        return 1;
    }
    if (wall_is_blocked_at(x, y + r, mask))
    {
        return 1;
    }
    if (wall_is_blocked_at(x, y - r, mask))
    {
        return 1;
    }
    return 0;
}

int wall_los(float ax, float ay, float bx, float by)
{
    if (!wall_map)
    {
        return 1;
    }
    return game_los(wall_map,
                    static_cast<int>(ax),
                    static_cast<int>(ay),
                    static_cast<int>(bx),
                    static_cast<int>(by))
               ? 1
               : 0;
}

int32_t axis(float v, int tiles)
{
    float limit = (float)tiles * WALL_TILE_SIZE - 1.0f;
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
        blocked += wall_is_blocked_wide(
                       mx + dx * d, my + dy * d, radius, WALL_BLOCKS_MOVEMENT)
                       ? 1
                       : 0;
    }
    float pin_x = (float)axis(mx + dx * reach, wall_cache_w()) - mx;
    float pin_y = (float)axis(my + dy * reach, wall_cache_h()) - my;
    float room = sqrtf(pin_x * pin_x + pin_y * pin_y) / reach;
    return (float)blocked / (float)PROBES + (1.0f - room);
}
