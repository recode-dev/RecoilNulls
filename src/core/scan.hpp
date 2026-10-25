#pragma once

#include <cstdint>
#include <cstddef>

namespace scanner
{
    constexpr size_t NAME_CAP = 24;
    constexpr int32_t MAX_TEAMMATES = 8;
    constexpr int32_t MAX_ENEMIES = 24;
    constexpr int32_t MAX_PROJECTILES = 48;
    constexpr int32_t MAX_DESTROYED = 16;

    struct Teammate
    {
        int32_t playerIndex;
        int32_t x;
        int32_t y;
        int32_t radius;
        int32_t teamId;
    };

    struct Enemy
    {
        uint32_t gid;
        uintptr_t ptr;
        uintptr_t body;
        int32_t x;
        int32_t y;
        int32_t brawlerId;
        int32_t teamId;
        int32_t playerIndex;
        int32_t moveSpeed;
        int32_t radius;
        bool isSpawnable;
        char brawlerName[NAME_CAP];
    };

    struct Projectile
    {
        uint32_t gid;
        uintptr_t ptr;
        int32_t x;
        int32_t y;
        int32_t spawnX;
        int32_t spawnY;
        int32_t ownerTeam;
        int32_t ownerIndex;
        uint32_t speed;
        uint32_t radius;
        int32_t castRange;
        uint32_t spawnedAt;
        float vx;
        float vy;
        bool isIndirect;
        bool isBeam;
        char name[NAME_CAP];
        char ownerName[NAME_CAP];
    };

    struct Destroyed
    {
        char name[NAME_CAP];
        int32_t spawnX;
        int32_t spawnY;
        int32_t x;
        int32_t y;
    };

    struct ScannerData
    {
        uintptr_t ownCharacter = 0;
        uintptr_t battleModeClient = 0;
        uintptr_t battleScreen = 0;
        int32_t myTeamId = -1;
        int32_t myPlayerIndex = -1;
        int32_t myX = 0;
        int32_t myY = 0;
        int32_t myRadius = 60;
        int32_t mySpeed = 720;
        int32_t myHp = 0;
        bool myAlive = false;
        uint32_t myDeadAt = 0;
        uint32_t myRespawnAt = 0;
        uint32_t myRespawns = 0;
        uint32_t lastUpdate = 0;
        char myBrawlerName[NAME_CAP] = {};
        int32_t teammateCount = 0;
        int32_t enemyCount = 0;
        int32_t projectileCount = 0;
        int32_t destroyedCount = 0;
        Teammate teammates[MAX_TEAMMATES] = {};
        Enemy enemies[MAX_ENEMIES] = {};
        Projectile projectiles[MAX_PROJECTILES] = {};
        Destroyed destroyed[MAX_DESTROYED] = {};
    };

    struct OwnStatus
    {
        bool alive;
        int32_t hp;
        uint32_t deadMs;
        uint32_t respawns;
        int32_t respawnAgeMs;
    };

    void init(uintptr_t base);
    uintptr_t getBattleMode();
    void reset();
    void enableProjectileTracking();
    uint32_t nowMs();
    bool readBytes(uintptr_t address, void *out, size_t size);
    bool writeBytes(uintptr_t address, const void *data, size_t size);
    bool update(uintptr_t battleModeClient, uint32_t nowMs, bool autododge);
    const ScannerData &data();
    OwnStatus ownStatus(uint32_t nowMs);
    bool isFriendlyOwner(int32_t ownerIndex, int32_t team);
    void captureProjectile(uintptr_t projectile, uintptr_t data, int32_t ownerTeamOverride, bool hasOverride, uint32_t nowMs);
    void removeProjectile(uintptr_t projectile);
}
