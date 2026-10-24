#include <cstdint>
#include <cstddef>
#include <cstring>
#include <frida-gum.h>

#include "walls.hpp"
#include "../core/offsets.hpp"
#include "../core/functions.hpp"

namespace
{
    constexpr int32_t MAX_MAP_TILES = 120;
    constexpr size_t MAX_TOTAL_TILES = (size_t)MAX_MAP_TILES * MAX_MAP_TILES;
    constexpr uint32_t RETRY_MS = 2000;
    constexpr float TRACE_STEP = 40.0f;
    constexpr float TRACE_BACKOFF = 75.0f;
    constexpr size_t CHUNK_POINTERS = 512;
    constexpr size_t TYPE_CACHE_SIZE = 32;

    uint8_t g_grid[MAX_TOTAL_TILES];
    uintptr_t g_pointerChunk[CHUNK_POINTERS];
    int32_t g_width = 0;
    int32_t g_height = 0;
    bool g_valid = false;
    bool g_needsRebuild = true;
    uint32_t g_lastFailedTs = 0;
    bool g_watchInstalled = false;

    struct TypeFlags
    {
        uintptr_t key;
        uint8_t flags;
    };

    TypeFlags g_typeCache[TYPE_CACHE_SIZE];
    size_t g_typeCount = 0;

    bool readBytes(uintptr_t address, void *out, size_t size)
    {
        if (address == 0 || out == nullptr || size == 0) return false;
        if (!gum_memory_is_readable((gconstpointer)address, (gsize)size)) return false;
        memcpy(out, (const void *)address, size);
        return true;
    }

    bool readU16(uintptr_t address, uint16_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    bool readS32(uintptr_t address, int32_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    bool readPtr(uintptr_t address, uintptr_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    uint32_t nowMs()
    {
        return (uint32_t)(g_get_monotonic_time() / 1000);
    }

    uint8_t flagsOfType(uintptr_t tileType)
    {
        for (size_t i = 0; i < g_typeCount; i++)
        {
            if (g_typeCache[i].key == tileType) return g_typeCache[i].flags;
        }

        uint8_t flags = 0;
        uint16_t packed = 0;
        if (readU16(tileType + offsets::TileTypeData_BlocksMovement, packed))
        {
            if ((packed & 0xff) != 0) flags |= walls::BLOCKS_MOVEMENT;
            if ((packed >> 8) != 0) flags |= walls::BLOCKS_PROJECTILES;
        }

        if (g_typeCount < TYPE_CACHE_SIZE)
        {
            g_typeCache[g_typeCount].key = tileType;
            g_typeCache[g_typeCount].flags = flags;
            g_typeCount++;
        }

        return flags;
    }

    bool rebuildFrom(uintptr_t tileMap)
    {
        if (tileMap == 0) return false;

        int32_t width = 0;
        int32_t height = 0;
        if (!readS32(tileMap + offsets::TileMap_Width, width)) return false;
        if (!readS32(tileMap + offsets::TileMap_Height, height)) return false;
        if (width <= 0 || width > MAX_MAP_TILES || height <= 0 || height > MAX_MAP_TILES) return false;

        const size_t total = (size_t)width * (size_t)height;
        if (total > MAX_TOTAL_TILES) return false;

        uintptr_t tiles = 0;
        if (!readPtr(tileMap + offsets::TileMap_TilesArray, tiles)) return false;
        if (tiles == 0) return false;

        g_typeCount = 0;
        memset(g_grid, 0, total);

        for (size_t start = 0; start < total; start += CHUNK_POINTERS)
        {
            size_t count = total - start;
            if (count > CHUNK_POINTERS) count = CHUNK_POINTERS;
            if (!readBytes(tiles + start * sizeof(uintptr_t), g_pointerChunk, count * sizeof(uintptr_t))) return false;

            for (size_t k = 0; k < count; k++)
            {
                const uintptr_t tile = g_pointerChunk[k];
                if (tile == 0) continue;

                uintptr_t tileType = 0;
                if (!readPtr(tile, tileType)) continue;
                if (tileType == 0) continue;

                g_grid[start + k] = flagsOfType(tileType);
            }
        }

        g_width = width;
        g_height = height;
        g_valid = true;
        g_needsRebuild = false;
        return true;
    }

    typedef struct _TileListener TileListener;
    typedef struct _TileListenerClass TileListenerClass;

    struct _TileListener
    {
        GObject parent;
    };

    struct _TileListenerClass
    {
        GObjectClass parent_class;
    };

    static void tile_listener_iface_init(gpointer g_iface, gpointer);

    static void tile_listener_init(TileListener *)
    {
    }

    static void tile_listener_class_init(TileListenerClass *)
    {
    }

    static void tile_listener_on_enter(GumInvocationListener *, GumInvocationContext *context)
    {
        g_needsRebuild = true;
    }

    static void tile_listener_iface_init(gpointer g_iface, gpointer)
    {
        GumInvocationListenerInterface *iface = static_cast<GumInvocationListenerInterface *>(g_iface);
        iface->on_enter = tile_listener_on_enter;
    }

    G_DEFINE_TYPE_EXTENDED(TileListener, tile_listener, G_TYPE_OBJECT, 0,
        G_IMPLEMENT_INTERFACE(GUM_TYPE_INVOCATION_LISTENER, tile_listener_iface_init))
}

void walls::watchTileChanges(uintptr_t baseAddress)
{
    if (g_watchInstalled) return;
    if (baseAddress == 0) return;

    GumInterceptor *interceptor = gum_interceptor_obtain();
    TileListener *listener = reinterpret_cast<TileListener *>(g_object_new(tile_listener_get_type(), NULL));
    const GumAttachReturn attachResult = gum_interceptor_attach(interceptor,
        GSIZE_TO_POINTER(baseAddress + offsets::LogicTile__setData),
        GUM_INVOCATION_LISTENER(listener),
        NULL);

    g_watchInstalled = attachResult == GUM_ATTACH_OK;
}

void walls::notifyBattleModeChanged(uintptr_t battleMode)
{
    g_valid = false;
    g_width = 0;
    g_height = 0;
    g_needsRebuild = true;
    g_lastFailedTs = 0;
    g_typeCount = 0;
    walls::maybeRefreshWallCache(battleMode, nowMs());
}

void walls::maybeRefreshWallCache(uintptr_t battleMode, uint32_t now)
{
    if (!g_needsRebuild || battleMode == 0) return;
    if (g_lastFailedTs != 0 && now - g_lastFailedTs < RETRY_MS) return;

    if (functions::LogicBattleModeClient_getTileMap == nullptr) return;
    const uintptr_t tileMap = functions::LogicBattleModeClient_getTileMap(battleMode);
    if (rebuildFrom(tileMap)) return;

    g_lastFailedTs = now;
}

int32_t walls::getWallCacheW()
{
    return g_width;
}

int32_t walls::getWallCacheH()
{
    return g_height;
}

bool walls::losCheck(float wx0, float wy0, float wx1, float wy1, uint8_t checkBit)
{
    if (!g_valid) return true;

    const int32_t width = g_width;
    const int32_t height = g_height;

    int32_t cx = (int32_t)(wx0 / TILE_SIZE);
    int32_t cy = (int32_t)(wy0 / TILE_SIZE);
    const int32_t tx = (int32_t)(wx1 / TILE_SIZE);
    const int32_t ty = (int32_t)(wy1 / TILE_SIZE);

    if (cx == tx && cy == ty) return true;

    const int32_t dx = tx > cx ? tx - cx : cx - tx;
    const int32_t dy = -((ty > cy) ? ty - cy : cy - ty);
    const int32_t sx = cx < tx ? 1 : -1;
    const int32_t sy = cy < ty ? 1 : -1;

    int32_t err = dx + dy;
    const int32_t maxSteps = dx - dy + 2;

    for (int32_t n = 0; n < maxSteps; n++)
    {
        const int32_t e2 = 2 * err;
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

        if (cx == tx && cy == ty) return true;
        if (cx < 0 || cx >= width || cy < 0 || cy >= height) continue;
        if ((g_grid[cy * width + cx] & checkBit) != 0) return false;
    }

    return true;
}

float walls::traceWallHit(float wx, float wy, float dirX, float dirY, float maxDist, uint8_t checkBit)
{
    if (!g_valid || maxDist <= 0.0f) return maxDist;

    const int32_t width = g_width;
    const int32_t height = g_height;
    if (width <= 0 || height <= 0) return maxDist;

    float dist = 0.0f;
    while (dist < maxDist)
    {
        dist += TRACE_STEP;
        if (dist > maxDist) dist = maxDist;

        const int32_t tx = (int32_t)((wx + dirX * dist) / TILE_SIZE);
        const int32_t ty = (int32_t)((wy + dirY * dist) / TILE_SIZE);

        if (tx < 0 || tx >= width || ty < 0 || ty >= height)
        {
            const float backed = dist - TRACE_BACKOFF;
            return backed > 0.0f ? backed : 0.0f;
        }

        if ((g_grid[ty * width + tx] & checkBit) != 0)
        {
            const float backed = dist - TRACE_BACKOFF;
            return backed > 0.0f ? backed : 0.0f;
        }
    }

    return maxDist;
}

bool walls::isBlockedAt(float wx, float wy, uint8_t checkBit)
{
    if (!g_valid) return false;

    const int32_t width = g_width;
    const int32_t height = g_height;
    if (width <= 0 || height <= 0) return false;

    const int32_t tx = (int32_t)(wx / TILE_SIZE);
    const int32_t ty = (int32_t)(wy / TILE_SIZE);
    if (tx < 0 || tx >= width || ty < 0 || ty >= height) return false;

    return (g_grid[ty * width + tx] & checkBit) != 0;
}

bool walls::isBlockedWide(float wx, float wy, float radius, uint8_t checkBit)
{
    if (walls::isBlockedAt(wx, wy, checkBit)) return true;
    if (radius <= 0.0f) return false;

    return walls::isBlockedAt(wx + radius, wy, checkBit) ||
        walls::isBlockedAt(wx - radius, wy, checkBit) ||
        walls::isBlockedAt(wx, wy + radius, checkBit) ||
        walls::isBlockedAt(wx, wy - radius, checkBit);
}
