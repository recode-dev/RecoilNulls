#pragma once

#include <cstdint>
#include <cstddef>

namespace walls
{
    constexpr float TILE_SIZE = 300.0f;
    constexpr uint8_t BLOCKS_MOVEMENT = 128;
    constexpr uint8_t BLOCKS_PROJECTILES = 64;

    void watchTileChanges(uintptr_t baseAddress);
    void notifyBattleModeChanged(uintptr_t battleMode);
    void maybeRefreshWallCache(uintptr_t battleMode, uint32_t nowMs);

    int32_t getWallCacheW();
    int32_t getWallCacheH();
    bool losCheck(float wx0, float wy0, float wx1, float wy1, uint8_t checkBit);
    float traceWallHit(float wx, float wy, float dirX, float dirY, float maxDist, uint8_t checkBit);
    bool isBlockedAt(float wx, float wy, uint8_t checkBit);
    bool isBlockedWide(float wx, float wy, float radius, uint8_t checkBit);
}
