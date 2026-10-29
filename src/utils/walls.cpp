#include "walls.h"

#include <cmath>
#include <cstring>

#include "../core/scan.h"

namespace
{
struct wall_layout
{
    uint64_t tiles_offset;
    uint64_t width_offset;
    uint64_t height_offset;
    uint64_t block_mask_offset;
    uint64_t projectile_mask_offset;
    uint64_t vision_mask_offset;
    uint64_t tile_data_offset;
    uint64_t blocks_projectiles_offset;
};

constexpr wall_layout WALL_LAYOUTS[] = {
    {0x20ULL, 0xc4ULL, 0xc8ULL, 0x10ULL, 0x18ULL, 0x20ULL, 0x00ULL, 0x57ULL},
    {0x00ULL, 0x90ULL, 0x94ULL, 0x10ULL, 0x18ULL, 0x20ULL, 0x00ULL, 0x57ULL},
    {0x20ULL, 0xc4ULL, 0xc8ULL, 0x08ULL, 0x10ULL, 0x18ULL, 0x00ULL, 0x57ULL},
    {0x00ULL, 0x90ULL, 0x94ULL, 0x08ULL, 0x10ULL, 0x18ULL, 0x00ULL, 0x57ULL},
    {0x18ULL, 0xb4ULL, 0xb8ULL, 0x10ULL, 0x18ULL, 0x20ULL, 0x00ULL, 0x57ULL},
};
constexpr int WALL_LAYOUT_COUNT =
    static_cast<int>(sizeof(WALL_LAYOUTS) / sizeof(WALL_LAYOUTS[0]));
constexpr int WALL_PROBE_CELLS = 16;
constexpr int WALL_PROBE_AGREEMENT = 90;
constexpr uint32_t WALL_ALL_SUBTILES = 0x07ffffffu;

uint32_t wall_move_mask[WALL_MAX_TOTAL_TILES];
uint8_t wall_vision_data[WALL_MAX_TOTAL_TILES];
uintptr_t wall_tiles = 0;
int wall_tiles_w = 0;
int wall_tiles_h = 0;
int wall_w = 0;
int wall_h = 0;
int wall_have = 0;
int wall_dirty = 1;
int wall_layout_found = 0;
int wall_local_trusted = 0;
int wall_engine_blocked = 0;
int wall_layer = WALL_LAYER_NORMAL;
uint64_t wall_fail_ms = 0;
uintptr_t wall_map = 0;
wall_layout wall_slots = WALL_LAYOUTS[0];

bool wall_read_word(uintptr_t address, uint64_t offset, uint32_t* out)
{
    if (!read_bytes(address + static_cast<uintptr_t>(offset), out, sizeof(*out)))
    {
        *out = 0;
        return false;
    }
    return true;
}

bool wall_read_tile(uintptr_t tiles, int index, uintptr_t* out)
{
    *out = 0;
    return read_bytes(tiles + static_cast<uintptr_t>(index) * sizeof(uintptr_t),
                      out,
                      sizeof(*out))
               ? true
               : false;
}

int wall_bit_index(int layer, int subX, int subY)
{
    return layer * 9 + subX + subY * 3;
}

int wall_subtile_passable(uint32_t mask, int layer, int subX, int subY)
{
    int bit = wall_bit_index(layer, subX, subY);
    if (bit < 0 || bit >= 32)
    {
        return -1;
    }
    return (mask & (1u << bit)) ? 0 : 1;
}

bool wall_tile_union_mask(uintptr_t tile, const wall_layout& layout, uint32_t* out)
{
    uint32_t blockMask = 0;
    uint32_t projectileMask = 0;
    uint32_t visionMask = 0;
    *out = 0;
    if (!wall_read_word(tile, layout.block_mask_offset, &blockMask))
    {
        return false;
    }
    if (!wall_read_word(tile, layout.projectile_mask_offset, &projectileMask))
    {
        return false;
    }
    if (!wall_read_word(tile, layout.vision_mask_offset, &visionMask))
    {
        return false;
    }
    *out = blockMask | projectileMask | visionMask;
    return true;
}

int wall_tile_vision_flag(uintptr_t tile, const wall_layout& layout, uint32_t unionMask)
{
    uintptr_t data = 0;
    uint8_t flag = 0;
    if (!read_ptr(tile + static_cast<uintptr_t>(layout.tile_data_offset), &data) ||
        !pointer_plausible(data))
    {
        return (unionMask & WALL_ALL_SUBTILES) ? 1 : 0;
    }
    if (!read_bytes(data + static_cast<uintptr_t>(layout.blocks_projectiles_offset),
                    &flag,
                    sizeof(flag)))
    {
        return (unionMask & WALL_ALL_SUBTILES) ? 1 : 0;
    }
    return flag ? 1 : 0;
}

bool wall_layout_dimensions(uintptr_t tileMap,
                            const wall_layout& layout,
                            int32_t* width,
                            int32_t* height,
                            uintptr_t* tiles)
{
    *width = 0;
    *height = 0;
    *tiles = 0;
    if (!read_int(tileMap + static_cast<uintptr_t>(layout.width_offset), width))
    {
        return false;
    }
    if (!read_int(tileMap + static_cast<uintptr_t>(layout.height_offset), height))
    {
        return false;
    }
    if (*width <= 0 || *width > WALL_MAX_MAP_TILES)
    {
        return false;
    }
    if (*height <= 0 || *height > WALL_MAX_MAP_TILES)
    {
        return false;
    }
    if (!read_ptr(tileMap + static_cast<uintptr_t>(layout.tiles_offset), tiles))
    {
        return false;
    }
    return *tiles != 0;
}

int wall_layout_tiles_plausible(uintptr_t tiles, int total, const wall_layout& layout)
{
    int step = total / 8 > 0 ? total / 8 : 1;
    int checked = 0;
    for (int index = 0; index < total; index += step)
    {
        uintptr_t tile = 0;
        uint32_t probe = 0;
        if (!wall_read_tile(tiles, index, &tile))
        {
            return 0;
        }
        checked++;
        if (!tile)
        {
            continue;
        }
        if (!pointer_plausible(tile))
        {
            return 0;
        }
        if (!wall_read_word(tile, layout.block_mask_offset, &probe))
        {
            return 0;
        }
    }
    return checked > 0 ? 1 : 0;
}

int wall_layout_agreement(uintptr_t tileMap,
                          const wall_layout& layout,
                          int width,
                          int height,
                          uintptr_t tiles)
{
    int stepX = width / 4 > 0 ? width / 4 : 1;
    int stepY = height / 4 > 0 ? height / 4 : 1;
    int samples = 0;
    int matches = 0;
    int cells = 0;
    for (int cellY = 0; cellY < height && cells < WALL_PROBE_CELLS; cellY += stepY)
    {
        for (int cellX = 0; cellX < width && cells < WALL_PROBE_CELLS; cellX += stepX)
        {
            uintptr_t tile = 0;
            uint32_t unionMask = WALL_ALL_SUBTILES;
            if (!wall_read_tile(tiles, cellX + width * cellY, &tile))
            {
                continue;
            }
            if (tile && pointer_plausible(tile) &&
                !wall_tile_union_mask(tile, layout, &unionMask))
            {
                continue;
            }
            cells++;
            for (int layer = 0; layer < WALL_LAYER_COUNT; layer++)
            {
                for (int corner = 0; corner < 2; corner++)
                {
                    int subX = corner == 0 ? 1 : 0;
                    int subY = corner == 0 ? 1 : 2;
                    int local = wall_subtile_passable(unionMask, layer, subX, subY);
                    if (local < 0)
                    {
                        continue;
                    }
                    int engine = game_tile_is_passable_layer(
                        tileMap, layer, cellX * 3 + subX, cellY * 3 + subY);
                    samples++;
                    if (local == engine)
                    {
                        matches++;
                    }
                }
            }
        }
    }
    if (samples < WALL_LAYER_COUNT * 2)
    {
        return 0;
    }
    return matches * 100 >= samples * WALL_PROBE_AGREEMENT ? 1 : 0;
}

void wall_store_tiles(uintptr_t tiles, int width, int height)
{
    if (tiles == wall_tiles && width == wall_tiles_w && height == wall_tiles_h)
    {
        return;
    }
    wall_tiles = tiles;
    wall_tiles_w = width;
    wall_tiles_h = height;
    wall_have = 0;
    wall_dirty = 1;
}

int wall_subtile_index(float value)
{
    float units = value / wall_subtile_units();
    if (units < 0.0f)
    {
        return -1;
    }
    return static_cast<int>(units);
}

int wall_vision_blocked_subtile(int subX, int subY)
{
    int cellX = 0;
    int cellY = 0;
    if (!wall_have)
    {
        return 0;
    }
    if (subX < 0 || subY < 0)
    {
        return 0;
    }
    cellX = subX / 3;
    cellY = subY / 3;
    if (cellX >= wall_w || cellY >= wall_h)
    {
        return 0;
    }
    return wall_vision_data[cellY * wall_w + cellX] ? 1 : 0;
}

int wall_engine_blocked_point(float x, float y, int layer)
{
    int subX = wall_subtile_index(x);
    int subY = wall_subtile_index(y);
    if (!wall_map || subX < 0 || subY < 0)
    {
        return 0;
    }
    return game_tile_is_passable_layer(wall_map, layer, subX, subY) ? 0 : 1;
}
}

float wall_tile_units()
{
    static float cached = 0.0f;
    if (cached <= 0.0f)
    {
        int base = game_tile_to_logic(0);
        int unit = game_tile_to_logic(1);
        if (unit <= base)
        {
            return WALL_TILE_UNITS;
        }
        cached = static_cast<float>(unit - base);
    }
    return cached;
}

float wall_subtile_units()
{
    float units = wall_tile_units() / 3.0f;
    return units > 0.0f ? units : WALL_SUBTILE_UNITS;
}

float wall_logic_units(int tiles)
{
    return wall_tile_units() * static_cast<float>(tiles);
}

int wall_layer_for_radius(float radius)
{
    return radius > WALL_BULKY_RADIUS ? WALL_LAYER_BULKY : WALL_LAYER_NORMAL;
}

int wall_cell_is_passable(float x, float y)
{
    return wall_cell_is_passable_layer(x, y, wall_layer);
}

int wall_cell_is_passable_layer(float x, float y, int layer)
{
    int subX = 0;
    int subY = 0;
    if (!wall_map || layer < 0)
    {
        return 0;
    }
    subX = wall_subtile_index(x);
    subY = wall_subtile_index(y);
    if (subX < 0 || subY < 0)
    {
        return 0;
    }
    return game_tile_is_passable_layer(wall_map, layer, subX, subY);
}

int wall_cached_blocked_count()
{
    int count = 0;
    int total = wall_w * wall_h;
    for (int index = 0; index < total; index++)
    {
        if (wall_move_mask[index])
        {
            count++;
        }
    }
    return count;
}

int wall_cached_vision_blocked_count()
{
    int count = 0;
    int total = wall_w * wall_h;
    for (int index = 0; index < total; index++)
    {
        if (wall_vision_data[index])
        {
            count++;
        }
    }
    return count;
}

int wall_engine_blocked_total()
{
    return wall_engine_blocked;
}

int wall_cache_width()
{
    return wall_w;
}

int wall_cache_height()
{
    return wall_h;
}

int wall_cache_local_trusted()
{
    return wall_local_trusted;
}

int wall_cache_layer()
{
    return wall_layer;
}

void wall_cache_set_layer(int layer)
{
    if (layer < 0)
    {
        layer = 0;
    }
    if (layer >= WALL_LAYER_COUNT)
    {
        layer = WALL_LAYER_COUNT - 1;
    }
    wall_layer = layer;
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
    wall_layout_found = 0;
    wall_local_trusted = 0;
    wall_engine_blocked = 0;
    wall_fail_ms = 0;
}

void wall_cache_bind(uintptr_t tileMap)
{
    int32_t width = 0;
    int32_t height = 0;
    uintptr_t tiles = 0;
    if (!tileMap)
    {
        return;
    }
    if (tileMap != wall_map)
    {
        wall_map = tileMap;
        wall_layout_found = 0;
        wall_local_trusted = 0;
        wall_store_tiles(0, 0, 0);
    }
    if (wall_layout_found &&
        wall_layout_dimensions(tileMap, wall_slots, &width, &height, &tiles))
    {
        wall_store_tiles(tiles, width, height);
        return;
    }
    for (int index = 0; index < WALL_LAYOUT_COUNT; index++)
    {
        if (!wall_layout_dimensions(
                tileMap, WALL_LAYOUTS[index], &width, &height, &tiles))
        {
            continue;
        }
        if (!wall_layout_tiles_plausible(tiles, width * height, WALL_LAYOUTS[index]))
        {
            continue;
        }
        wall_slots = WALL_LAYOUTS[index];
        wall_layout_found = 1;
        wall_store_tiles(tiles, width, height);
        return;
    }
}

int wall_cache_build()
{
    int width = wall_tiles_w;
    int height = wall_tiles_h;
    int total = 0;
    if (!wall_tiles || !wall_map)
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
    wall_engine_blocked = 0;
    for (int index = 0; index < total; index++)
    {
        uintptr_t tile = 0;
        uint32_t unionMask = WALL_ALL_SUBTILES;
        bool readable = false;
        wall_move_mask[index] = WALL_ALL_SUBTILES;
        wall_vision_data[index] = 1;
        if (!wall_read_tile(wall_tiles, index, &tile))
        {
            return 0;
        }
        if (tile && pointer_plausible(tile))
        {
            readable = wall_tile_union_mask(tile, wall_slots, &unionMask);
        }
        if (readable)
        {
            wall_move_mask[index] = unionMask;
            wall_vision_data[index] =
                static_cast<uint8_t>(wall_tile_vision_flag(tile, wall_slots, unionMask));
        }
        if (!game_tile_is_passable_layer(wall_map,
                                         wall_layer,
                                         (index % width) * 3 + 1,
                                         (index / width) * 3 + 1))
        {
            wall_engine_blocked++;
        }
    }
    wall_w = width;
    wall_h = height;
    wall_have = 1;
    wall_dirty = 0;
    wall_local_trusted = wall_layout_agreement(wall_map, wall_slots, width, height, wall_tiles);
    return 1;
}

int wall_cache_refresh_if_stale(uint64_t now_ms)
{
    if (!wall_dirty)
    {
        return 0;
    }
    if (now_ms - wall_fail_ms < WALL_RETRY_MS)
    {
        return 0;
    }
    if (wall_cache_build())
    {
        return 1;
    }
    wall_fail_ms = now_ms;
    return 0;
}

int wall_cell_blocked_layer(float x, float y, int layer)
{
    int subX = 0;
    int subY = 0;
    int cellX = 0;
    int cellY = 0;
    int passable = 0;
    if (layer < 0)
    {
        layer = 0;
    }
    if (!wall_have || !wall_local_trusted || layer >= WALL_LAYER_COUNT)
    {
        return wall_engine_blocked_point(x, y, layer);
    }
    subX = wall_subtile_index(x);
    subY = wall_subtile_index(y);
    if (subX < 0 || subY < 0)
    {
        return 0;
    }
    cellX = subX / 3;
    cellY = subY / 3;
    if (cellX >= wall_w || cellY >= wall_h)
    {
        return 0;
    }
    passable = wall_subtile_passable(
        wall_move_mask[cellY * wall_w + cellX], layer, subX % 3, subY % 3);
    if (passable < 0)
    {
        return wall_engine_blocked_point(x, y, layer);
    }
    return passable ? 0 : 1;
}

int wall_cell_blocked_at(float x, float y)
{
    return wall_cell_blocked_layer(x, y, wall_layer);
}

int wall_cell_blocked_wide(float x, float y, float r)
{
    int layer = wall_layer_for_radius(r);
    if (wall_cell_blocked_layer(x, y, layer))
    {
        return 1;
    }
    if (r <= 0.0f)
    {
        return 0;
    }
    if (wall_cell_blocked_layer(x + r, y, layer))
    {
        return 1;
    }
    if (wall_cell_blocked_layer(x - r, y, layer))
    {
        return 1;
    }
    if (wall_cell_blocked_layer(x, y + r, layer))
    {
        return 1;
    }
    if (wall_cell_blocked_layer(x, y - r, layer))
    {
        return 1;
    }
    return 0;
}

int wall_vision_blocked_at(float x, float y)
{
    int subX = wall_subtile_index(x);
    int subY = wall_subtile_index(y);
    return wall_vision_blocked_subtile(subX, subY);
}

int wall_line_of_sight_clear(float ax, float ay, float bx, float by)
{
    float dx = bx - ax;
    float dy = by - ay;
    float length = sqrtf(dx * dx + dy * dy);
    int steps = 0;
    if (!wall_have)
    {
        return 1;
    }
    steps = static_cast<int>(length / WALL_LOS_STEP) + 1;
    if (steps < 1)
    {
        steps = 1;
    }
    if (steps > 4096)
    {
        steps = 4096;
    }
    for (int index = 1; index < steps; index++)
    {
        float t = static_cast<float>(index) / static_cast<float>(steps);
        if (wall_vision_blocked_at(ax + dx * t, ay + dy * t))
        {
            return 0;
        }
    }
    return 1;
}

int32_t axis(float v, int tiles)
{
    float limit = wall_logic_units(tiles) - 1.0f;
    if (v < 0.0f)
    {
        return 0;
    }
    if (v > limit)
    {
        return static_cast<int32_t>(limit);
    }
    return static_cast<int32_t>(roundf(v));
}

float wall_pull(float mx, float my, float dx, float dy, float radius)
{
    constexpr int PROBES = 3;
    float reach = radius * 6.0f;
    int blocked = 0;
    float pin_x = 0.0f;
    float pin_y = 0.0f;
    float room = 0.0f;
    for (int index = 1; index <= PROBES; index++)
    {
        float distance = reach * 0.5f * static_cast<float>(index) / static_cast<float>(PROBES);
        blocked += wall_cell_blocked_wide(mx + dx * distance, my + dy * distance, radius) ? 1 : 0;
    }
    pin_x = static_cast<float>(axis(mx + dx * reach, wall_cache_width())) - mx;
    pin_y = static_cast<float>(axis(my + dy * reach, wall_cache_height())) - my;
    room = sqrtf(pin_x * pin_x + pin_y * pin_y) / reach;
    return static_cast<float>(blocked) / static_cast<float>(PROBES) + (1.0f - room);
}
