#include <cstdint>
#include <cstddef>
#include "offsets.cpp"

namespace
{
    uintptr_t g_base = 0;

    template <typename T>
    T resolve(uintptr_t offset)
    {
        return g_base == 0 ? nullptr : reinterpret_cast<T>(g_base + offset);
    }
}

namespace functions
{
    void init(uintptr_t base)
    {
        g_base = base;
    }

    bool isInitialized()
    {
        return g_base != 0;
    }

    uintptr_t base()
    {
        return g_base;
    }

    uintptr_t BattleMode_getInstance()
    {
        static uintptr_t (*fn)() = resolve<uintptr_t (*)()>(offsets::BattleMode_getInstance);
        return fn != nullptr ? fn() : 0;
    }

    uintptr_t LogicBattleModeClient_getOwnCharacter(uintptr_t battleMode)
    {
        static uintptr_t (*fn)(uintptr_t) = resolve<uintptr_t (*)(uintptr_t)>(offsets::LogicBattleModeClient_getOwnCharacter);
        return fn != nullptr ? fn(battleMode) : 0;
    }

    uint32_t LogicBattleModeClient_getOwnPlayerTeam(uintptr_t battleMode)
    {
        static uint32_t (*fn)(uintptr_t) = resolve<uint32_t (*)(uintptr_t)>(offsets::LogicBattleModeClient_getOwnPlayerTeam);
        return fn != nullptr ? fn(battleMode) : 0;
    }

    int32_t LogicBattleModeClient__getOwnPlayerIndex(uintptr_t battleMode)
    {
        static int32_t (*fn)(uintptr_t) = resolve<int32_t (*)(uintptr_t)>(offsets::LogicBattleModeClient__getOwnPlayerIndex);
        return fn != nullptr ? fn(battleMode) : -1;
    }

    uintptr_t LogicBattleModeClient__getTileMap(uintptr_t battleMode)
    {
        static uintptr_t (*fn)(uintptr_t) = resolve<uintptr_t (*)(uintptr_t)>(offsets::LogicBattleModeClient__getTileMap);
        return fn != nullptr ? fn(battleMode) : 0;
    }

    void LogicBattleModeClient_setClientPredictionMoveTo(uintptr_t battleMode, int32_t x, int32_t y, int32_t flags)
    {
        static void (*fn)(uintptr_t, int32_t, int32_t, int32_t) = resolve<void (*)(uintptr_t, int32_t, int32_t, int32_t)>(offsets::LogicBattleModeClient_setClientPredictionMoveTo);
        if (fn != nullptr) fn(battleMode, x, y, flags);
    }

    uintptr_t ClientInput_constructor_int(uintptr_t input, int32_t type)
    {
        static uintptr_t (*fn)(uintptr_t, int32_t) = resolve<uintptr_t (*)(uintptr_t, int32_t)>(offsets::ClientInput_constructor_int);
        return fn != nullptr ? fn(input, type) : 0;
    }

    void ClientInputManager_addInput(uintptr_t manager, uintptr_t input)
    {
        static void (*fn)(uintptr_t, uintptr_t) = resolve<void (*)(uintptr_t, uintptr_t)>(offsets::ClientInputManager_addInput);
        if (fn != nullptr) fn(manager, input);
    }

    uintptr_t operator_new(size_t size)
    {
        static uintptr_t (*fn)(size_t) = resolve<uintptr_t (*)(size_t)>(offsets::operator_new);
        return fn != nullptr ? fn(size) : 0;
    }

    int32_t LogicGameObjectClient_getX(uintptr_t object)
    {
        static int32_t (*fn)(uintptr_t) = resolve<int32_t (*)(uintptr_t)>(offsets::LogicGameObjectClient_getX);
        return fn != nullptr ? fn(object) : 0;
    }

    int32_t LogicGameObjectClient_getY(uintptr_t object)
    {
        static int32_t (*fn)(uintptr_t) = resolve<int32_t (*)(uintptr_t)>(offsets::LogicGameObjectClient_getY);
        return fn != nullptr ? fn(object) : 0;
    }

    uintptr_t LogicGameObjectClient_getData(uintptr_t object)
    {
        static uintptr_t (*fn)(uintptr_t) = resolve<uintptr_t (*)(uintptr_t)>(offsets::LogicGameObjectClient_getData);
        return fn != nullptr ? fn(object) : 0;
    }

    uint32_t LogicGameObjectClient_getGlobalID(uintptr_t object)
    {
        static uint32_t (*fn)(uintptr_t) = resolve<uint32_t (*)(uintptr_t)>(offsets::LogicGameObjectClient_getGlobalID);
        return fn != nullptr ? fn(object) : 0;
    }

    int32_t LogicCharacterData_getSpeed(uintptr_t data)
    {
        static int32_t (*fn)(uintptr_t) = resolve<int32_t (*)(uintptr_t)>(offsets::LogicCharacterData_getSpeed);
        return fn != nullptr ? fn(data) : 0;
    }

    uint32_t LogicCharacterData_getCollisionRadius(uintptr_t data)
    {
        static uint32_t (*fn)(uintptr_t) = resolve<uint32_t (*)(uintptr_t)>(offsets::LogicCharacterData_getCollisionRadius);
        return fn != nullptr ? fn(data) : 0;
    }

    uintptr_t LogicCharacterClient__getWeaponSkill(uintptr_t character)
    {
        static uintptr_t (*fn)(uintptr_t) = resolve<uintptr_t (*)(uintptr_t)>(offsets::LogicCharacterClient__getWeaponSkill);
        return fn != nullptr ? fn(character) : 0;
    }

    uintptr_t LogicCharacterClient__getSkillAt(uintptr_t character, int32_t slot)
    {
        static uintptr_t (*fn)(uintptr_t, int32_t) = resolve<uintptr_t (*)(uintptr_t, int32_t)>(offsets::LogicCharacterClient__getSkillAt);
        return fn != nullptr ? fn(character, slot) : 0;
    }

    uintptr_t LogicSkillClient__getData(uintptr_t skill, int32_t hyper)
    {
        static uintptr_t (*fn)(uintptr_t, int32_t) = resolve<uintptr_t (*)(uintptr_t, int32_t)>(offsets::LogicSkillClient__getData);
        return fn != nullptr ? fn(skill, hyper) : 0;
    }

    int32_t LogicSkillData__getCastingRange(uintptr_t skillData)
    {
        static int32_t (*fn)(uintptr_t) = resolve<int32_t (*)(uintptr_t)>(offsets::LogicSkillData__getCastingRange);
        return fn != nullptr ? fn(skillData) : 0;
    }

    uintptr_t LogicSkillData__getProjectileData(uintptr_t skillData, int32_t index)
    {
        static uintptr_t (*fn)(uintptr_t, int32_t) = resolve<uintptr_t (*)(uintptr_t, int32_t)>(offsets::LogicSkillData__getProjectileData);
        return fn != nullptr ? fn(skillData, index) : 0;
    }

    uint32_t LogicProjectileData_getSpeed(uintptr_t data)
    {
        static uint32_t (*fn)(uintptr_t) = resolve<uint32_t (*)(uintptr_t)>(offsets::LogicProjectileData_getSpeed);
        return fn != nullptr ? fn(data) : 0;
    }

    uint32_t LogicProjectileData_getRadius(uintptr_t data)
    {
        static uint32_t (*fn)(uintptr_t) = resolve<uint32_t (*)(uintptr_t)>(offsets::LogicProjectileData_getRadius);
        return fn != nullptr ? fn(data) : 0;
    }

    int32_t LogicProjectileData_getRendering(uintptr_t data)
    {
        static int32_t (*fn)(uintptr_t) = resolve<int32_t (*)(uintptr_t)>(offsets::LogicProjectileData_getRendering);
        return fn != nullptr ? fn(data) : 0;
    }

    uintptr_t LogicProjectileData__getSpawnAreaEffect(uintptr_t data)
    {
        static uintptr_t (*fn)(uintptr_t) = resolve<uintptr_t (*)(uintptr_t)>(offsets::LogicProjectileData__getSpawnAreaEffect);
        return fn != nullptr ? fn(data) : 0;
    }

    uint32_t LogicProjectileData_getNumEarlyTicks(uintptr_t data)
    {
        static uint32_t (*fn)(uintptr_t) = resolve<uint32_t (*)(uintptr_t)>(offsets::LogicProjectileData_getNumEarlyTicks);
        return fn != nullptr ? fn(data) : 0;
    }

    uint32_t LogicProjectileData_isBeam(uintptr_t data)
    {
        static uint32_t (*fn)(uintptr_t) = resolve<uint32_t (*)(uintptr_t)>(offsets::LogicProjectileData_isBeam);
        return fn != nullptr ? fn(data) : 0;
    }

    int32_t AreaEffectData__getRadius(uintptr_t data)
    {
        static int32_t (*fn)(uintptr_t) = resolve<int32_t (*)(uintptr_t)>(offsets::AreaEffectData__getRadius);
        return fn != nullptr ? fn(data) : 0;
    }

    int32_t AreaEffectData__getActiveTimeMs(uintptr_t data)
    {
        static int32_t (*fn)(uintptr_t) = resolve<int32_t (*)(uintptr_t)>(offsets::AreaEffectData__getActiveTimeMs);
        return fn != nullptr ? fn(data) : 0;
    }

    uintptr_t LogicData_getName(uintptr_t data)
    {
        static uintptr_t (*fn)(uintptr_t) = resolve<uintptr_t (*)(uintptr_t)>(offsets::LogicData_getName);
        return fn != nullptr ? fn(data) : 0;
    }
}