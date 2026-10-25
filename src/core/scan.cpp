#include <cstdint>
#include <cstddef>
#include <cstring>
#include <time.h>

#include "../../tools/hook.hpp"
#include "../../tools/memory.hpp"
#include "offsets.hpp"
#include "functions.hpp"
#include "scan.hpp"

using namespace scanner;

namespace
{
    constexpr int32_t CASTING_RANGE_SCALE = 100;
    constexpr int32_t SKILL_SLOT_MAX = 8;
    constexpr int32_t PROJ_INDEX_MAX = 4;
    constexpr int32_t MAX_OBJECTS = 1200;
    constexpr uint32_t PROJECTILE_TIMEOUT_MS = 800;
    constexpr size_t MAX_LIVE = 64;
    constexpr size_t MAX_NAMES = 64;
    constexpr size_t MAX_CHARS = 24;
    constexpr size_t MAX_INDICES = 16;
    constexpr size_t MAX_TEAMS = 16;
    constexpr uint32_t MAX_OBJECT_INDEX = 256;
    constexpr int32_t DEFAULT_RADIUS = 60;
    constexpr int32_t DEFAULT_MOVE_SPEED = 720;
    constexpr int32_t MIN_MOVE_SPEED = 300;
    constexpr int32_t MAX_MOVE_SPEED = 3000;
    constexpr uint32_t APP_STATE_BATTLE = 5;

    struct NameEntry
    {
        uint32_t gid;
        char name[NAME_CAP];
    };

    struct CharRef
    {
        int32_t index;
        uintptr_t ptr;
    };

    struct IndexTeam
    {
        int32_t index;
        int32_t team;
    };


    struct LiveProjectile
    {
        bool used;
        uint32_t gid;
        uintptr_t ptr;
        uintptr_t data;
        int32_t x;
        int32_t y;
        int32_t spawnX;
        int32_t spawnY;
        int32_t lastX;
        int32_t lastY;
        int32_t ownerTeam;
        int32_t ownerIndex;
        uint32_t speed;
        uint32_t radius;
        int32_t castRange;
        uint32_t spawnedAt;
        uint32_t lastTs;
        uint32_t lastSeenAt;
        float vx;
        float vy;
        bool hasVelocitySample;
        bool isIndirect;
        bool isBeam;
        char name[NAME_CAP];
        char ownerName[NAME_CAP];
    };

    uintptr_t g_base = 0;
    ScannerData g_data;

    NameEntry g_names[MAX_NAMES];
    size_t g_nameCount = 0;

    CharRef g_chars[MAX_CHARS];
    size_t g_charCount = 0;



    int32_t g_friendlyIdx[MAX_INDICES];
    size_t g_friendlyCount = 0;

    IndexTeam g_teamOfIndex[MAX_CHARS];
    size_t g_teamOfIndexCount = 0;

    int32_t g_enemyTeams[MAX_TEAMS];
    size_t g_enemyTeamCount = 0;

    LiveProjectile g_live[MAX_LIVE];
    Enemy g_enemies[MAX_ENEMIES];

    uint8_t g_batch[MAX_OBJECTS * 8];

    bool g_hpSane = false;
    bool g_wasDead = false;
    bool g_hasSeenPos = false;
    int32_t g_hpZeroStreak = 0;
    int32_t g_lastSeenX = 0;
    int32_t g_lastSeenY = 0;
    uint32_t g_scanCount = 0;

    constexpr int32_t SCSTRING_INLINE_MAX = 7;
    constexpr size_t SCSTRING_HEADER = 16;

    size_t readScStringInto(uintptr_t value, char *out, size_t capacity)
    {
        if (value == 0 || out == nullptr || capacity < 2) return 0;

        uint8_t head[SCSTRING_HEADER];
        if (!tools::memory::read(value + offsets::ScString_length, head, sizeof(head))) return 0;

        const int32_t length = (int32_t)((uint32_t)head[0] |
            ((uint32_t)head[1] << 8) |
            ((uint32_t)head[2] << 16) |
            ((uint32_t)head[3] << 24));
        if (length <= 0) return 0;

        const size_t limit = capacity - 1;
        const size_t want = (size_t)length < limit ? (size_t)length : limit;

        if (length <= SCSTRING_INLINE_MAX)
        {
            memcpy(out, head + 4, want);
            out[want] = 0;
            return want;
        }

        uintptr_t data = 0;
        memcpy(&data, head + 4, sizeof(data));
        if (data == 0) return 0;

        if (!tools::memory::read(data, out, want)) return 0;
        out[want] = 0;
        return want;
    }

    bool readBytes(uintptr_t address, void *out, size_t size)
    {
        return tools::memory::read(address, out, size);
    }

    bool readU32(uintptr_t address, uint32_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    bool readPtr(uintptr_t address, uintptr_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    constexpr uintptr_t IMAGE_SPAN = 0x4000000;

    bool inImage(uintptr_t pointer)
    {
        return pointer >= g_base && pointer < g_base + IMAGE_SPAN;
    }

    bool mapped(uintptr_t object)
    {
        uintptr_t vtable = 0;
        if (object == 0) return false;
        if (!readPtr(object, vtable)) return false;

        return inImage(vtable);
    }

    bool readS32(uintptr_t address, int32_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    bool readU8(uintptr_t address, uint8_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    const char *ownerNameByIndex(int32_t ownerIndex)
    {
        if (ownerIndex < 0) return "";
        if (ownerIndex == g_data.myPlayerIndex && g_data.myBrawlerName[0] != 0) return g_data.myBrawlerName;

        for (int32_t i = 0; i < g_data.enemyCount; i++)
        {
            const Enemy &enemy = g_data.enemies[i];
            if (enemy.playerIndex == ownerIndex && enemy.brawlerName[0] != 0) return enemy.brawlerName;
        }

        return "";
    }

    uint32_t readGid(uintptr_t object)
    {
        uint32_t gid = 0;
        readU32(object + offsets::GameObj_gid, gid);

        return gid;
    }

    void copyName(const char *src, char *out)
    {
        size_t i = 0;
        while (i + 1 < NAME_CAP && src[i] != 0)
        {
            char c = src[i];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            out[i] = c;
            i++;
        }
        out[i] = 0;
    }


    bool readProjectileName(uintptr_t data, char *out)
    {
        out[0] = 0;
        if (!mapped(data)) return false;

        uintptr_t namePtr = 0;
        if (!readPtr(data + offsets::LogicProjectileClient_dataName, namePtr)) return false;
        if (namePtr == 0) return false;
        return readScStringInto(namePtr, out, NAME_CAP) > 0 && out[0] != 0;
    }

    int32_t readOwnerIndex(uintptr_t obj)
    {
        int32_t value = 0;
        if (!readS32(obj + offsets::LogicGameObjectClient_ownerIndex, value)) return -1;
        return value;
    }

    int32_t hpOf(uintptr_t obj)
    {
        if (obj == 0) return -1;
        uint32_t raw = 0;
        if (!readU32(obj + offsets::GameObj_hp, raw)) return -1;
        const int32_t value = (int32_t)raw;
        if (value > 200000) return -1;
        return value;
    }

    int32_t readObjectTeam(uintptr_t obj)
    {
        uint32_t raw = 0;
        if (!readU32(obj + offsets::GameObj_team, raw)) return -1;
        return (int32_t)raw;
    }

    uintptr_t bodyOf(uintptr_t obj)
    {
        if (obj == 0 || offsets::LogicCharacterClient_body == 0) return obj;

        uintptr_t body = 0;
        if (!readPtr(obj + offsets::LogicCharacterClient_body, body)) return obj;
        if (body == 0 || body == obj) return obj;

        uintptr_t objVtable = 0;
        uintptr_t bodyVtable = 0;
        if (!readPtr(obj, objVtable)) return obj;
        if (!readPtr(body, bodyVtable)) return obj;
        if (objVtable == 0 || bodyVtable == 0 || bodyVtable != objVtable) return obj;
        return body;
    }

    bool hasFriendlyIndex(int32_t index)
    {
        for (size_t i = 0; i < g_friendlyCount; i++)
        {
            if (g_friendlyIdx[i] == index) return true;
        }
        return false;
    }

    void addFriendlyIndex(int32_t index)
    {
        if (index < 0 || hasFriendlyIndex(index)) return;
        if (g_friendlyCount >= MAX_INDICES) return;
        g_friendlyIdx[g_friendlyCount++] = index;
    }

    bool hasEnemyTeam(int32_t team)
    {
        for (size_t i = 0; i < g_enemyTeamCount; i++)
        {
            if (g_enemyTeams[i] == team) return true;
        }
        return false;
    }

    void addEnemyTeam(int32_t team)
    {
        if (team < 0 || hasEnemyTeam(team)) return;
        if (g_enemyTeamCount >= MAX_TEAMS) return;
        g_enemyTeams[g_enemyTeamCount++] = team;
    }

    void setIndexTeam(int32_t index, int32_t team)
    {
        if (index < 0) return;
        for (size_t i = 0; i < g_teamOfIndexCount; i++)
        {
            if (g_teamOfIndex[i].index == index)
            {
                g_teamOfIndex[i].team = team;
                return;
            }
        }
        if (g_teamOfIndexCount >= MAX_CHARS) return;
        g_teamOfIndex[g_teamOfIndexCount].index = index;
        g_teamOfIndex[g_teamOfIndexCount].team = team;
        g_teamOfIndexCount++;
    }

    int32_t teamOfIndex(int32_t index)
    {
        for (size_t i = 0; i < g_teamOfIndexCount; i++)
        {
            if (g_teamOfIndex[i].index == index) return g_teamOfIndex[i].team;
        }
        return -1;
    }






    const char *cachedName(uint32_t gid, bool &found)
    {
        for (size_t i = 0; i < g_nameCount; i++)
        {
            if (g_names[i].gid == gid)
            {
                found = true;
                return g_names[i].name;
            }
        }
        found = false;
        return "";
    }

    void storeName(uint32_t gid, const char *name)
    {
        for (size_t i = 0; i < g_nameCount; i++)
        {
            if (g_names[i].gid == gid)
            {
                strncpy(g_names[i].name, name, NAME_CAP - 1);
                g_names[i].name[NAME_CAP - 1] = 0;
                return;
            }
        }
        if (g_nameCount >= MAX_NAMES) return;
        g_names[g_nameCount].gid = gid;
        strncpy(g_names[g_nameCount].name, name, NAME_CAP - 1);
        g_names[g_nameCount].name[NAME_CAP - 1] = 0;
        g_nameCount++;
    }

    void rememberCharacter(int32_t index, uintptr_t character);



    void rememberCharacter(int32_t index, uintptr_t character)
    {
        if (index < 0 || character == 0) return;

        bool stored = false;
        for (size_t i = 0; i < g_charCount; i++)
        {
            if (g_chars[i].index == index)
            {
                g_chars[i].ptr = character;
                stored = true;
                break;
            }
        }
        if (!stored)
        {
            if (g_charCount < MAX_CHARS)
            {
                g_chars[g_charCount].index = index;
                g_chars[g_charCount].ptr = character;
                g_charCount++;
            }
        }
    }


    LiveProjectile *findLive(uint32_t gid)
    {
        for (size_t i = 0; i < MAX_LIVE; i++)
        {
            if (g_live[i].used && g_live[i].gid == gid) return &g_live[i];
        }
        return nullptr;
    }

    LiveProjectile *freeLive()
    {
        for (size_t i = 0; i < MAX_LIVE; i++)
        {
            if (!g_live[i].used) return &g_live[i];
        }

        size_t oldest = 0;
        for (size_t i = 1; i < MAX_LIVE; i++)
        {
            if (g_live[i].lastSeenAt < g_live[oldest].lastSeenAt) oldest = i;
        }
        return &g_live[oldest];
    }

    void clearLive()
    {
        for (size_t i = 0; i < MAX_LIVE; i++) g_live[i].used = false;
    }

    void markDead()
    {
        g_wasDead = true;
        g_data.myAlive = false;
        g_data.myHp = 0;
    }

    void markAlive(uint32_t now)
    {
        if (g_wasDead)
        {
            g_wasDead = false;
            g_data.myRespawns++;
            g_data.myRespawnAt = now;
        }
        g_data.myAlive = true;
    }

    void updateLiveProjectiles(uint32_t now)
    {
        g_data.projectileCount = 0;

        for (size_t i = 0; i < MAX_LIVE; i++)
        {
            LiveProjectile &live = g_live[i];
            if (!live.used) continue;

            if (live.ptr == 0 || !mapped(live.ptr) || now - live.lastSeenAt > PROJECTILE_TIMEOUT_MS)
            {
                live.used = false;
                continue;
            }

            int32_t x = 0;
            int32_t y = 0;
            readS32(live.ptr + offsets::GameObj_x, x);
            readS32(live.ptr + offsets::GameObj_y, y);

            const uint32_t elapsed = now - live.lastTs;
            const int32_t dx = x - live.lastX;
            const int32_t dy = y - live.lastY;
            if (elapsed > 2 && elapsed < 300 && dx * dx + dy * dy >= 16)
            {
                const float dt = (float)elapsed / 1000.0f;
                const float observedVx = (float)dx / dt;
                const float observedVy = (float)dy / dt;
                const float alpha = live.hasVelocitySample ? 0.4f : 1.0f;
                live.vx = live.vx * (1.0f - alpha) + observedVx * alpha;
                live.vy = live.vy * (1.0f - alpha) + observedVy * alpha;
                live.hasVelocitySample = true;
            }

            live.x = x;
            live.y = y;
            live.lastX = x;
            live.lastY = y;
            live.lastTs = now;

            const int32_t liveTeam = readObjectTeam(live.ptr);
            if (liveTeam >= 0) live.ownerTeam = liveTeam;

            const int32_t liveOwner = readOwnerIndex(live.ptr);
            if (liveOwner >= 0) live.ownerIndex = liveOwner;

            const char *ownerName = ownerNameByIndex(live.ownerIndex);
            if (ownerName[0] != 0) strncpy(live.ownerName, ownerName, NAME_CAP - 1);
            live.ownerName[NAME_CAP - 1] = 0;


            if (scanner::isFriendlyOwner(live.ownerIndex, live.ownerTeam)) continue;
            if (g_data.projectileCount >= MAX_PROJECTILES) continue;

            Projectile &out = g_data.projectiles[g_data.projectileCount++];
            out.gid = live.gid;
            out.ptr = live.ptr;
            out.x = x;
            out.y = y;
            out.spawnX = live.spawnX;
            out.spawnY = live.spawnY;
            out.ownerTeam = live.ownerTeam;
            out.ownerIndex = live.ownerIndex;
            out.speed = live.speed;
            out.radius = live.radius;
            out.castRange = live.castRange > 0 ? live.castRange : 0;
            out.spawnedAt = live.spawnedAt;
            out.vx = live.vx;
            out.vy = live.vy;
            out.isIndirect = live.isIndirect;
            out.isBeam = live.isBeam;
            strncpy(out.name, live.name, NAME_CAP - 1);
            out.name[NAME_CAP - 1] = 0;
            strncpy(out.ownerName, live.ownerName, NAME_CAP - 1);
            out.ownerName[NAME_CAP - 1] = 0;
        }
    }
}

bool scanner::isObject(uintptr_t pointer)
{
    return mapped(pointer);
}

void scanner::init(uintptr_t base)
{
    g_base = base;
    scanner::reset();
}

uintptr_t scanner::getBattleMode()
{
    if (g_base == 0) return 0;

    uintptr_t appState = 0;
    if (!readBytes(g_base + offsets::AppState, &appState, sizeof(appState))) return 0;
    if (!tools::memory::plausible(appState)) return 0;

    uint32_t state = 0;
    if (!readBytes(appState + offsets::AppState_state, &state, sizeof(state))) return 0;
    if (state != APP_STATE_BATTLE) return 0;

    uintptr_t scene = 0;
    if (!readBytes(appState + offsets::AppState_scene, &scene, sizeof(scene))) return 0;
    if (!tools::memory::plausible(scene)) return 0;

    uintptr_t client = 0;
    if (!readBytes(scene + offsets::Scene_client, &client, sizeof(client))) return 0;
    if (!tools::memory::plausible(client)) return 0;
    if (!mapped(client)) return 0;

    return client;
}

void scanner::reset()
{
    g_nameCount = 0;
    g_charCount = 0;
    g_friendlyCount = 0;
    g_teamOfIndexCount = 0;
    g_enemyTeamCount = 0;
    g_hpSane = false;
    g_wasDead = false;
    g_hasSeenPos = false;
    g_hpZeroStreak = 0;
    g_lastSeenX = 0;
    g_lastSeenY = 0;
    g_scanCount = 0;
    clearLive();

    g_data.ownCharacter = 0;
    g_data.battleModeClient = 0;
    g_data.battleScreen = 0;
    g_data.myTeamId = -1;
    g_data.myPlayerIndex = -1;
    g_data.myHp = 0;
    g_data.myAlive = false;
    g_data.myRespawnAt = 0;
    g_data.myRespawns = 0;
    g_data.myBrawlerName[0] = 0;
    g_data.enemyCount = 0;
    g_data.projectileCount = 0;
    g_data.lastUpdate = 0;
}

const scanner::ScannerData &scanner::data()
{
    return g_data;
}

bool scanner::isFriendlyOwner(int32_t ownerIndex, int32_t team)
{
    if (team >= 0 && g_data.myTeamId >= 0 && team != g_data.myTeamId && hasEnemyTeam(team)) return false;
    if (ownerIndex >= 0 && hasFriendlyIndex(ownerIndex)) return true;
    if (team >= 0 && g_data.myTeamId >= 0 && team == g_data.myTeamId) return true;
    if (ownerIndex >= 0)
    {
        const int32_t byIndex = teamOfIndex(ownerIndex);
        if (byIndex >= 0 && g_data.myTeamId >= 0 && byIndex == g_data.myTeamId) return true;
    }
    return false;
}

void scanner::captureProjectile(uintptr_t projectile, uintptr_t data, int32_t ownerTeamOverride, bool hasOverride, uint32_t nowMs)
{
    if (!mapped(projectile)) return;
    if (!mapped(data)) return;

    const uint32_t gid = readGid(projectile);
    LiveProjectile *existing = findLive(gid);
    if (existing != nullptr)
    {
        existing->ptr = projectile;
        existing->data = data;
        existing->lastSeenAt = nowMs;
        if (hasOverride) existing->ownerTeam = ownerTeamOverride;
        return;
    }

    LiveProjectile *record = freeLive();
    memset(record, 0, sizeof(*record));
    record->used = true;
    record->gid = gid;
    record->ptr = projectile;
    record->data = data;

    int32_t x = 0;
    int32_t y = 0;
    readS32(projectile + offsets::GameObj_x, x);
    readS32(projectile + offsets::GameObj_y, y);

    uint32_t speed = 0;
    uint32_t radius = 0;
    readU32(data + offsets::ProjectileData_speed, speed);
    readU32(data + offsets::ProjectileData_radius, radius);
    record->speed = speed > 0 ? speed : 1;
    record->radius = radius;

    uint32_t isIndirect = 0;
    readU32(projectile + offsets::Projectile_isIndirect, isIndirect);
    record->isIndirect = isIndirect != 0;


    int32_t ownerTeam = 0;
    if (hasOverride)
    {
        ownerTeam = ownerTeamOverride;
    }
    else
    {
        uint32_t raw = 0;
        if (readU32(projectile + offsets::LogicProjectileClient_ownerTeam, raw)) ownerTeam = (int32_t)raw;
    }
    record->ownerTeam = ownerTeam;
    record->ownerIndex = readOwnerIndex(projectile);

    readProjectileName(data, record->name);
    record->castRange = 0;

    const uint32_t earlyTicks = 0;

    record->x = x;
    record->y = y;
    record->spawnX = x;
    record->spawnY = y;
    record->lastX = x;
    record->lastY = y;
    record->spawnedAt = nowMs - earlyTicks * 16;
    record->lastTs = nowMs;
    record->lastSeenAt = nowMs;

    const char *ownerName = ownerNameByIndex(record->ownerIndex);
    if (ownerName[0] != 0) strncpy(record->ownerName, ownerName, NAME_CAP - 1);
    record->ownerName[NAME_CAP - 1] = 0;
}

void scanner::removeProjectile(uintptr_t projectile)
{
    if (!mapped(projectile)) return;

    const uint32_t gid = readGid(projectile);
    LiveProjectile *record = findLive(gid);
    if (record == nullptr) return;

    record->used = false;
}

bool scanner::update(uintptr_t battleModeClient, uint32_t nowMs, bool autododge)
{
    if (g_base == 0) return false;
    if (!mapped(battleModeClient)) return false;

    uint32_t ownIndex = 0;
    if (!readBytes(battleModeClient + offsets::Client_ownIndex, &ownIndex, sizeof(ownIndex))) return false;
    if (ownIndex >= MAX_OBJECT_INDEX) return false;

    uint32_t ownTeam = 0;
    if (!readBytes(battleModeClient + offsets::Client_ownTeam, &ownTeam, sizeof(ownTeam))) return false;
    if (ownTeam >= (uint32_t)MAX_TEAMS) return false;

    uintptr_t manager = 0;
    if (!readBytes(battleModeClient + offsets::BattleMode_objectManagerPtr, &manager, sizeof(manager))) return false;
    if (!tools::memory::plausible(manager)) return false;

    uintptr_t objects = 0;
    if (!readBytes(manager + offsets::ObjectManager_objectsArray, &objects, sizeof(objects))) return false;
    if (!tools::memory::plausible(objects)) return false;

    uintptr_t own = 0;
    if (!readBytes(objects + (size_t)ownIndex * sizeof(uintptr_t), &own, sizeof(own))) return false;

    if (!tools::memory::plausible(own))
    {
        markDead();
        g_data.ownCharacter = 0;
        g_data.battleModeClient = battleModeClient;
        g_data.myHp = 0;
            g_data.enemyCount = 0;


        if (autododge)
        {
            updateLiveProjectiles(nowMs);
        }
        else
        {
            clearLive();
            g_data.projectileCount = 0;
        }
        g_data.lastUpdate = nowMs;
        return true;
    }

    g_data.ownCharacter = own;
    g_data.battleModeClient = battleModeClient;

    uintptr_t battleScreen = 0;
    if (readPtr(battleModeClient + offsets::BattleMode_battleScreenPtr, battleScreen) && battleScreen != 0)
    {
        uintptr_t vtable = 0;
        g_data.battleScreen = readPtr(battleScreen, vtable) && vtable != 0 ? battleScreen : 0;
    }
    else
    {
        g_data.battleScreen = 0;
    }

    g_data.myTeamId = (int32_t)ownTeam;
    g_data.myPlayerIndex = (int32_t)ownIndex;

    g_friendlyCount = 0;
    g_teamOfIndexCount = 0;
    g_enemyTeamCount = 0;
    if (g_data.myPlayerIndex >= 0)
    {
        addFriendlyIndex(g_data.myPlayerIndex);
        setIndexTeam(g_data.myPlayerIndex, g_data.myTeamId);
    }

    g_charCount = 0;
    rememberCharacter(g_data.myPlayerIndex, own);

    const uintptr_t self = bodyOf(own);
    const uintptr_t position = mapped(self) ? self : own;

    readS32(position + offsets::GameObj_x, g_data.myX);
    readS32(position + offsets::GameObj_y, g_data.myY);

    g_data.mySpeed = DEFAULT_MOVE_SPEED;
    g_data.myRadius = DEFAULT_RADIUS;

    int32_t hp = hpOf(own);
    if (self != own)
    {
        const int32_t bodyHp = hpOf(self);
        if (bodyHp > 0 || hp <= 0) hp = bodyHp;
    }

    if (hp > 0) g_hpSane = true;

    bool wentDead = false;
    if (g_hpSane && hp == 0)
    {
        const int32_t jx = g_hasSeenPos ? g_data.myX - g_lastSeenX : 0;
        const int32_t jy = g_hasSeenPos ? g_data.myY - g_lastSeenY : 0;
        if (jx * jx + jy * jy > 400) g_hpZeroStreak = 0;
        else g_hpZeroStreak++;
        wentDead = g_hpZeroStreak >= 2;
    }
    else
    {
        g_hpZeroStreak = 0;
    }

    g_data.myHp = hp > 0 ? hp : 0;
    g_lastSeenX = g_data.myX;
    g_lastSeenY = g_data.myY;
    g_hasSeenPos = true;

    if (wentDead) markDead();
    else markAlive(nowMs);

    int32_t enemyCount = 0;

    uintptr_t objectManager = 0;
    if (readPtr(battleModeClient + offsets::BattleMode_objectManagerPtr, objectManager) && objectManager != 0)
    {
        uintptr_t objects = 0;
        uint32_t count = 0;
        readPtr(objectManager + offsets::ObjectManager_objectsArray, objects);
        readU32(objectManager + offsets::ObjectManager_count, count);

        if (objects != 0 && count > 0 && count <= (uint32_t)MAX_OBJECTS)
        {
            const size_t stride = offsets::ObjectManager_ptrStride;
            const size_t bytes = (size_t)count * stride;
            const bool batched = bytes <= sizeof(g_batch) && readBytes(objects, g_batch, bytes);

            for (uint32_t i = 0; i < count; i++)
            {
                uintptr_t object = 0;
                if (batched)
                {
                    memcpy(&object, g_batch + (size_t)i * stride, sizeof(object));
                }
                else if (!readPtr(objects + (size_t)i * stride, object))
                {
                    continue;
                }

                if (!mapped(object)) continue;

                const int32_t team = readObjectTeam(object);
                const int32_t owner = readOwnerIndex(object);

                const bool friendly = (owner >= 0 && hasFriendlyIndex(owner)) ||
                    (team >= 0 && team == g_data.myTeamId);

                if (friendly)
                {
                    if (owner >= 0)
                    {
                        addFriendlyIndex(owner);
                        setIndexTeam(owner, team);
                    }
                    continue;
                }

                uint32_t objectHp = 0;
                if (!readU32(object + offsets::GameObj_hp, objectHp) || objectHp == 0) continue;

                uint8_t deadFlag = 0;
                if (readU8(object + offsets::GameObj_deadFlag, deadFlag) && (deadFlag & 1) != 0) continue;

                if (enemyCount >= MAX_ENEMIES) continue;

                const uintptr_t body = bodyOf(object);
                const uintptr_t enemyPosition = mapped(body) ? body : object;

                Enemy &enemy = g_enemies[enemyCount++];
                enemy.gid = readGid(object);
                enemy.ptr = object;
                enemy.body = body;
                enemy.x = 0;
                enemy.y = 0;
                readS32(enemyPosition + offsets::GameObj_x, enemy.x);
                readS32(enemyPosition + offsets::GameObj_y, enemy.y);
                enemy.brawlerId = 0;
                enemy.teamId = team;
                enemy.playerIndex = owner;
                enemy.moveSpeed = 0;
                enemy.radius = DEFAULT_RADIUS;
                enemy.isSpawnable = false;
                enemy.brawlerName[0] = 0;

                if (owner >= 0) setIndexTeam(owner, team);
                if (team >= 0) addEnemyTeam(team);
            }
        }
    }

    g_data.enemyCount = enemyCount;
    if (enemyCount > 0) memcpy(g_data.enemies, g_enemies, (size_t)enemyCount * sizeof(Enemy));

    g_scanCount++;
    if ((g_scanCount & 63) == 0)
    {
        size_t keep = 0;
        for (size_t i = 0; i < g_nameCount; i++)
        {
            bool active = false;
            for (int32_t e = 0; e < g_data.enemyCount; e++)
            {
                if (g_data.enemies[e].gid == g_names[i].gid)
                {
                    active = true;
                    break;
                }
            }
            if (active)
            {
                if (keep != i) g_names[keep] = g_names[i];
                keep++;
            }
        }
        g_nameCount = keep;
    }


    if (autododge)
    {
        updateLiveProjectiles(nowMs);
    }
    else
    {
        clearLive();
        g_data.projectileCount = 0;
    }

    g_data.lastUpdate = nowMs;
    return true;
}

namespace
{

    using SlotFn = uint64_t (*)(void *, uint64_t, uint64_t, uint64_t,
                                uint64_t, uint64_t, uint64_t, uint64_t);

    SlotFn g_origCtor = nullptr;
    SlotFn g_origDestruct = nullptr;
    bool g_hooksInstalled = false;

    uint64_t projectile_ctor(void *self, uint64_t data, uint64_t a2, uint64_t a3,
                             uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7)
    {
        uint64_t result = 0;

        if (g_origCtor != nullptr) result = g_origCtor(self, data, a2, a3, a4, a5, a6, a7);

        scanner::captureProjectile((uintptr_t)self, (uintptr_t)data, 0, false, scanner::nowMs());

        return result;
    }

    uint64_t projectile_destruct(void *self, uint64_t a1, uint64_t a2, uint64_t a3,
                                 uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7)
    {
        scanner::removeProjectile((uintptr_t)self);

        if (g_origDestruct != nullptr) return g_origDestruct(self, a1, a2, a3, a4, a5, a6, a7);

        return 0;
    }
}

void scanner::enableProjectileTracking()
{
    if (g_base == 0 || g_hooksInstalled) return;
    if (offsets::LogicProjectileClient_ctor == 0) return;
    if (offsets::LogicProjectileClient_destruct == 0) return;

    g_hooksInstalled = true;

    g_origCtor = reinterpret_cast<SlotFn>(
        tools::hook::install(g_base, offsets::LogicProjectileClient_ctor, reinterpret_cast<void *>(&projectile_ctor)));

    g_origDestruct = reinterpret_cast<SlotFn>(
        tools::hook::install(g_base, offsets::LogicProjectileClient_destruct, reinterpret_cast<void *>(&projectile_destruct)));
}

uint32_t scanner::nowMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint32_t)((uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000));
}
