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
    constexpr size_t MAX_RANGES = 32;
    constexpr size_t MAX_HARVESTED = 32;
    constexpr size_t MAX_INDICES = 16;
    constexpr size_t MAX_TEAMS = 16;
    constexpr uint32_t MAX_OBJECT_INDEX = 256;
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

    struct RangeEntry
    {
        char name[NAME_CAP];
        int32_t units;
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

    uintptr_t g_harvested[MAX_HARVESTED];
    size_t g_harvestedCount = 0;

    RangeEntry g_ranges[MAX_RANGES];
    size_t g_rangeCount = 0;

    int32_t g_friendlyIdx[MAX_INDICES];
    size_t g_friendlyCount = 0;

    IndexTeam g_teamOfIndex[MAX_CHARS];
    size_t g_teamOfIndexCount = 0;

    int32_t g_enemyTeams[MAX_TEAMS];
    size_t g_enemyTeamCount = 0;

    LiveProjectile g_live[MAX_LIVE];
    Teammate g_teammates[MAX_TEAMMATES];
    Enemy g_enemies[MAX_ENEMIES];

    Destroyed g_destroyedBuf[MAX_DESTROYED];
    int32_t g_destroyedCount = 0;

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

    bool readS32(uintptr_t address, int32_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    bool readU8(uintptr_t address, uint8_t &out)
    {
        return readBytes(address, &out, sizeof(out));
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

    bool readDataName(uintptr_t data, char *out)
    {
        out[0] = 0;
        if (data == 0) return false;

        uintptr_t namePtr = 0;
        if (!readPtr(data + offsets::HeroData_namePtr, namePtr)) return false;
        if (namePtr == 0) return false;

        char raw[64];
        if (readScStringInto(namePtr, raw, sizeof(raw)) == 0) return false;

        const char *src = raw;
        if (strncmp(raw, "hero_icon_", 10) == 0) src = raw + 10;
        copyName(src, out);
        return out[0] != 0;
    }

    bool readProjectileName(uintptr_t data, char *out)
    {
        out[0] = 0;
        if (data == 0) return false;

        if (functions::LogicData_getName != nullptr)
        {
            const uintptr_t named = functions::LogicData_getName(data);
            if (named != 0 && readScStringInto(named, out, NAME_CAP) > 0 && out[0] != 0) return true;
        }

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

    uintptr_t charByIndex(int32_t index)
    {
        for (size_t i = 0; i < g_charCount; i++)
        {
            if (g_chars[i].index == index) return g_chars[i].ptr;
        }
        return 0;
    }

    bool wasHarvested(uintptr_t character)
    {
        for (size_t i = 0; i < g_harvestedCount; i++)
        {
            if (g_harvested[i] == character) return true;
        }
        return false;
    }

    void markHarvested(uintptr_t character)
    {
        if (g_harvestedCount >= MAX_HARVESTED) return;
        g_harvested[g_harvestedCount++] = character;
    }

    int32_t rangeFor(const char *name)
    {
        if (name == nullptr || name[0] == 0) return 0;
        for (size_t i = 0; i < g_rangeCount; i++)
        {
            if (strncmp(g_ranges[i].name, name, NAME_CAP) == 0) return g_ranges[i].units;
        }
        return 0;
    }

    void noteCastRange(const char *name, int32_t units)
    {
        if (name == nullptr || name[0] == 0 || units <= 0 || units >= 70000) return;

        for (size_t i = 0; i < g_rangeCount; i++)
        {
            if (strncmp(g_ranges[i].name, name, NAME_CAP) == 0)
            {
                if (units > g_ranges[i].units) g_ranges[i].units = units;
                return;
            }
        }
        if (g_rangeCount >= MAX_RANGES) return;
        strncpy(g_ranges[g_rangeCount].name, name, NAME_CAP - 1);
        g_ranges[g_rangeCount].name[NAME_CAP - 1] = 0;
        g_ranges[g_rangeCount].units = units;
        g_rangeCount++;
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
    int32_t castRangeFor(const char *name, int32_t ownerIndex);

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

    void harvestSkillData(uintptr_t skillData)
    {
        if (skillData == 0) return;
        if (functions::LogicSkillData_getCastingRange == nullptr) return;
        if (functions::LogicSkillData_getProjectileData == nullptr) return;

        const int32_t tiles = functions::LogicSkillData_getCastingRange(skillData);
        if (tiles <= 0) return;

        const int32_t units = tiles * CASTING_RANGE_SCALE;
        for (int32_t i = 0; i < PROJ_INDEX_MAX; i++)
        {
            const uintptr_t projectile = functions::LogicSkillData_getProjectileData(skillData, i);
            if (projectile == 0) continue;
            char name[NAME_CAP];
            if (readProjectileName(projectile, name)) noteCastRange(name, units);
        }
    }

    bool harvestCharacter(uintptr_t character)
    {
        if (character == 0) return false;
        bool found = false;

        if (functions::LogicCharacterClient_getWeaponSkill != nullptr)
        {
            const uintptr_t weapon = functions::LogicCharacterClient_getWeaponSkill(character);
            if (weapon != 0)
            {
                harvestSkillData(weapon);
                found = true;
            }
        }

        if (functions::LogicCharacterClient_getSkillAt == nullptr) return found;
        if (functions::LogicSkillClient_getData == nullptr) return found;

        for (int32_t slot = 0; slot < SKILL_SLOT_MAX; slot++)
        {
            const uintptr_t skill = functions::LogicCharacterClient_getSkillAt(character, slot);
            if (skill == 0) continue;

            for (int32_t hyper = 0; hyper < 2; hyper++)
            {
                const uintptr_t data = functions::LogicSkillClient_getData(skill, hyper);
                if (data == 0) continue;
                harvestSkillData(data);
                found = true;
            }
        }
        return found;
    }

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

        if (wasHarvested(character)) return;
        if (harvestCharacter(character)) markHarvested(character);
    }

    int32_t castRangeFor(const char *name, int32_t ownerIndex)
    {
        const int32_t cached = rangeFor(name);
        if (cached > 0) return cached;
        if (ownerIndex < 0) return 0;

        const uintptr_t owner = charByIndex(ownerIndex);
        if (owner == 0) return 0;
        if (!harvestCharacter(owner)) return 0;
        return rangeFor(name);
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

    void pushDestroyed(LiveProjectile &record)
    {
        if (g_destroyedCount >= MAX_DESTROYED) return;
        Destroyed &slot = g_destroyedBuf[g_destroyedCount++];
        strncpy(slot.name, record.name, NAME_CAP - 1);
        slot.name[NAME_CAP - 1] = 0;
        slot.spawnX = record.spawnX;
        slot.spawnY = record.spawnY;
        slot.x = record.lastX;
        slot.y = record.lastY;
    }

    void markDead(uint32_t now)
    {
        if (!g_wasDead)
        {
            g_wasDead = true;
            g_data.myDeadAt = now;
        }
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
            g_data.myDeadAt = 0;
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

            if (live.ptr == 0 || now - live.lastSeenAt > PROJECTILE_TIMEOUT_MS)
            {
                live.used = false;
                continue;
            }

            int32_t x = 0;
            int32_t y = 0;
            if (functions::LogicGameObjectClient_getX == nullptr) continue;
            x = functions::LogicGameObjectClient_getX(live.ptr);
            y = functions::LogicGameObjectClient_getY(live.ptr);

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

            if (live.castRange <= 0) live.castRange = castRangeFor(live.name, live.ownerIndex);

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

    return client;
}

void scanner::reset()
{
    g_nameCount = 0;
    g_charCount = 0;
    g_harvestedCount = 0;
    g_rangeCount = 0;
    g_friendlyCount = 0;
    g_teamOfIndexCount = 0;
    g_enemyTeamCount = 0;
    g_destroyedCount = 0;
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
    g_data.myDeadAt = 0;
    g_data.myRespawnAt = 0;
    g_data.myRespawns = 0;
    g_data.myBrawlerName[0] = 0;
    g_data.teammateCount = 0;
    g_data.enemyCount = 0;
    g_data.projectileCount = 0;
    g_data.destroyedCount = 0;
    g_data.lastUpdate = 0;
}

const scanner::ScannerData &scanner::data()
{
    return g_data;
}

scanner::OwnStatus scanner::ownStatus(uint32_t nowMs)
{
    OwnStatus status;
    status.alive = g_data.myAlive;
    status.hp = g_data.myHp;
    status.deadMs = g_data.myDeadAt > 0 && nowMs > g_data.myDeadAt ? nowMs - g_data.myDeadAt : 0;
    status.respawns = g_data.myRespawns;
    status.respawnAgeMs = g_data.myRespawnAt > 0
        ? (int32_t)(nowMs > g_data.myRespawnAt ? nowMs - g_data.myRespawnAt : 0)
        : -1;
    return status;
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
    if (projectile == 0) return;
    if (data == 0 && functions::LogicGameObjectClient_getData != nullptr)
    {
        data = functions::LogicGameObjectClient_getData(projectile);
    }
    if (data == 0) return;
    if (functions::LogicGameObjectClient_getGlobalID == nullptr) return;

    const uint32_t gid = functions::LogicGameObjectClient_getGlobalID(projectile);
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
    if (functions::LogicGameObjectClient_getX != nullptr) x = functions::LogicGameObjectClient_getX(projectile);
    if (functions::LogicGameObjectClient_getY != nullptr) y = functions::LogicGameObjectClient_getY(projectile);

    uint32_t speed = 0;
    uint32_t radius = 0;
    readU32(data + offsets::ProjectileData_speed, speed);
    readU32(data + offsets::ProjectileData_radius, radius);
    record->speed = speed > 0 ? speed : 1;
    record->radius = radius;

    uint32_t isIndirect = 0;
    readU32(projectile + offsets::Projectile_isIndirect, isIndirect);
    record->isIndirect = isIndirect != 0;

    if (functions::LogicProjectileData_isBeam != nullptr)
    {
        record->isBeam = functions::LogicProjectileData_isBeam(data) != 0;
    }

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
    record->castRange = castRangeFor(record->name, record->ownerIndex);

    uint32_t earlyTicks = 0;
    if (functions::LogicProjectileData_getNumEarlyTicks != nullptr)
    {
        earlyTicks = functions::LogicProjectileData_getNumEarlyTicks(data);
    }

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
    if (projectile == 0) return;
    if (functions::LogicGameObjectClient_getGlobalID == nullptr) return;

    const uint32_t gid = functions::LogicGameObjectClient_getGlobalID(projectile);
    LiveProjectile *record = findLive(gid);
    if (record == nullptr) return;

    if (!scanner::isFriendlyOwner(record->ownerIndex, record->ownerTeam)) pushDestroyed(*record);
    record->used = false;
}

bool scanner::update(uintptr_t battleModeClient, uint32_t nowMs, bool autododge)
{
    if (g_base == 0 || !tools::memory::plausible(battleModeClient)) return false;

    uintptr_t vtable = 0;
    if (!readBytes(battleModeClient, &vtable, sizeof(vtable))) return false;
    if (!tools::memory::plausible(vtable)) return false;

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
        markDead(nowMs);
        g_data.ownCharacter = 0;
        g_data.battleModeClient = battleModeClient;
        g_data.myHp = 0;
        g_data.teammateCount = 0;
        g_data.enemyCount = 0;

        g_data.destroyedCount = g_destroyedCount;
        if (g_destroyedCount > 0) memcpy(g_data.destroyed, g_destroyedBuf, (size_t)g_destroyedCount * sizeof(Destroyed));
        g_destroyedCount = 0;

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
    if (functions::LogicGameObjectClient_getX != nullptr) g_data.myX = functions::LogicGameObjectClient_getX(self);
    if (functions::LogicGameObjectClient_getY != nullptr) g_data.myY = functions::LogicGameObjectClient_getY(self);

    g_data.mySpeed = 720;
    const uintptr_t ownData = functions::LogicGameObjectClient_getData != nullptr
        ? functions::LogicGameObjectClient_getData(own)
        : 0;
    if (ownData != 0)
    {
        if (functions::LogicCharacterData_getCollisionRadius != nullptr)
        {
            const uint32_t radius = functions::LogicCharacterData_getCollisionRadius(ownData);
            g_data.myRadius = radius != 0 ? (int32_t)radius : 60;
        }
        if (functions::LogicCharacterData_getSpeed != nullptr)
        {
            const int32_t speed = (int32_t)functions::LogicCharacterData_getSpeed(ownData);
            if (speed >= 300 && speed <= 3000) g_data.mySpeed = speed;
        }
        char name[NAME_CAP];
        if (readDataName(ownData, name) && name[0] != 0)
        {
            strncpy(g_data.myBrawlerName, name, NAME_CAP - 1);
            g_data.myBrawlerName[NAME_CAP - 1] = 0;
        }
    }

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

    if (wentDead) markDead(nowMs);
    else markAlive(nowMs);

    int32_t teammateCount = 0;
    int32_t enemyCount = 0;

    const uintptr_t projectileVtable = g_base + offsets::Vtable_projectileData;

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
                if (object == 0) continue;

                const uintptr_t data = functions::LogicGameObjectClient_getData != nullptr
                    ? functions::LogicGameObjectClient_getData(object)
                    : 0;
                if (data == 0) continue;

                const int32_t team = readObjectTeam(object);

                uintptr_t dataVtable = 0;
                if (readPtr(data, dataVtable) && dataVtable == projectileVtable)
                {
                    if (autododge) scanner::captureProjectile(object, data, team, true, nowMs);
                    continue;
                }

                const int32_t index = readOwnerIndex(object);
                const bool friendly = (index >= 0 && hasFriendlyIndex(index)) ||
                    (team >= 0 && team == g_data.myTeamId);

                if (friendly)
                {
                    if (index >= 0)
                    {
                        addFriendlyIndex(index);
                        setIndexTeam(index, team);
                    }
                    if (index >= 0 && index != g_data.myPlayerIndex && teammateCount < MAX_TEAMMATES)
                    {
                        const uintptr_t teammateBody = bodyOf(object);
                        int32_t radius = 0;
                        if (functions::LogicCharacterData_getCollisionRadius != nullptr)
                        {
                            radius = (int32_t)functions::LogicCharacterData_getCollisionRadius(data);
                        }
                        if (radius > 0 && radius <= 400)
                        {
                            Teammate &slot = g_teammates[teammateCount++];
                            slot.playerIndex = index;
                            slot.x = functions::LogicGameObjectClient_getX != nullptr
                                ? functions::LogicGameObjectClient_getX(teammateBody) : 0;
                            slot.y = functions::LogicGameObjectClient_getY != nullptr
                                ? functions::LogicGameObjectClient_getY(teammateBody) : 0;
                            slot.radius = radius;
                            slot.teamId = team;
                        }
                    }
                    continue;
                }

                uint32_t objectHp = 0;
                if (!readU32(object + offsets::GameObj_hp, objectHp) || objectHp == 0) continue;

                if (enemyCount >= MAX_ENEMIES) continue;

                const uint32_t gid = functions::LogicGameObjectClient_getGlobalID != nullptr
                    ? functions::LogicGameObjectClient_getGlobalID(object)
                    : 0;

                char brawlerName[NAME_CAP];
                bool cached = false;
                const char *cachedPtr = cachedName(gid, cached);
                if (cached)
                {
                    strncpy(brawlerName, cachedPtr, NAME_CAP - 1);
                    brawlerName[NAME_CAP - 1] = 0;
                }
                else
                {
                    readDataName(data, brawlerName);
                    storeName(gid, brawlerName);
                }

                int32_t radius = 0;
                if (functions::LogicCharacterData_getCollisionRadius != nullptr)
                {
                    radius = (int32_t)functions::LogicCharacterData_getCollisionRadius(data);
                }
                if (brawlerName[0] == 0 && radius <= 0) continue;

                int32_t moveSpeed = 0;
                if (functions::LogicCharacterData_getSpeed != nullptr)
                {
                    moveSpeed = (int32_t)functions::LogicCharacterData_getSpeed(data);
                }

                const int32_t playerIndex = readOwnerIndex(object);
                if (playerIndex >= 0) setIndexTeam(playerIndex, team);
                if (team >= 0) addEnemyTeam(team);
                if (brawlerName[0] != 0) rememberCharacter(playerIndex, object);

                int32_t brawlerId = 0;
                if (brawlerName[0] != 0)
                {
                    uint8_t raw = 0;
                    if (readU8(data + offsets::CharData_brawlerId, raw)) brawlerId = raw;
                }

                const uintptr_t body = bodyOf(object);

                Enemy &enemy = g_enemies[enemyCount++];
                enemy.gid = gid;
                enemy.ptr = object;
                enemy.body = body;
                enemy.x = functions::LogicGameObjectClient_getX != nullptr
                    ? functions::LogicGameObjectClient_getX(body) : 0;
                enemy.y = functions::LogicGameObjectClient_getY != nullptr
                    ? functions::LogicGameObjectClient_getY(body) : 0;
                enemy.brawlerId = brawlerId;
                enemy.teamId = team;
                enemy.playerIndex = playerIndex;
                enemy.moveSpeed = moveSpeed;
                enemy.radius = radius;
                enemy.isSpawnable = brawlerName[0] == 0;
                strncpy(enemy.brawlerName, brawlerName, NAME_CAP - 1);
                enemy.brawlerName[NAME_CAP - 1] = 0;
            }
        }
    }

    g_data.teammateCount = teammateCount;
    if (teammateCount > 0) memcpy(g_data.teammates, g_teammates, (size_t)teammateCount * sizeof(Teammate));

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

    g_data.destroyedCount = g_destroyedCount;
    if (g_destroyedCount > 0) memcpy(g_data.destroyed, g_destroyedBuf, (size_t)g_destroyedCount * sizeof(Destroyed));
    g_destroyedCount = 0;

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
