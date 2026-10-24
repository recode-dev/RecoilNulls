#include <cstdint>
#include <cstddef>
#include <frida-gum.h>

#include "offsets.hpp"
#include "functions.hpp"

namespace
{
    uintptr_t g_base = 0;

    bool readBytes(uintptr_t address, void *out, size_t size)
    {
        if (address == 0 || out == nullptr || size == 0) return false;
        return gum_memory_read((GumAddress)address, out, (gsize)size);
    }

    template <typename T>
    bool bind(T &slot, uintptr_t offset)
    {
        if (g_base == 0 || offset == 0) return false;
        const uintptr_t target = g_base + offset;
        uint32_t probe = 0;
        if (!readBytes(target, &probe, sizeof(probe))) return false;
        slot = reinterpret_cast<T>(target);
        return true;
    }
}

bool functions::init(uintptr_t base_address)
{
    g_base = base_address;
    bool ok = base_address != 0;

    ok = bind(functions::BattleMode_getInstance, offsets::BattleMode_getInstance) && ok;
    ok = bind(functions::LogicBattleModeClient_getOwnCharacter, offsets::LogicBattleModeClient_getOwnCharacter) && ok;
    ok = bind(functions::LogicBattleModeClient_getOwnPlayerTeam, offsets::LogicBattleModeClient_getOwnPlayerTeam) && ok;
    ok = bind(functions::LogicBattleModeClient_getOwnPlayerIndex, offsets::LogicBattleModeClient__getOwnPlayerIndex) && ok;
    ok = bind(functions::LogicBattleModeClient_getTileMap, offsets::LogicBattleModeClient__getTileMap) && ok;
    ok = bind(functions::LogicBattleModeClient_setClientPredictionMoveTo, offsets::LogicBattleModeClient_setClientPredictionMoveTo) && ok;

    ok = bind(functions::ClientInput_constructor_int, offsets::ClientInput_constructor_int) && ok;
    ok = bind(functions::ClientInputManager_addInput, offsets::ClientInputManager_addInput) && ok;
    ok = bind(functions::operator_new, offsets::operator_new) && ok;

    ok = bind(functions::LogicGameObjectClient_getX, offsets::LogicGameObjectClient_getX) && ok;
    ok = bind(functions::LogicGameObjectClient_getY, offsets::LogicGameObjectClient_getY) && ok;
    ok = bind(functions::LogicGameObjectClient_getData, offsets::LogicGameObjectClient_getData) && ok;
    ok = bind(functions::LogicGameObjectClient_getGlobalID, offsets::LogicGameObjectClient_getGlobalID) && ok;

    ok = bind(functions::LogicCharacterData_getSpeed, offsets::LogicCharacterData_getSpeed) && ok;
    ok = bind(functions::LogicCharacterData_getCollisionRadius, offsets::LogicCharacterData_getCollisionRadius) && ok;
    ok = bind(functions::LogicCharacterClient_getWeaponSkill, offsets::LogicCharacterClient__getWeaponSkill) && ok;
    ok = bind(functions::LogicCharacterClient_getSkillAt, offsets::LogicCharacterClient__getSkillAt) && ok;

    ok = bind(functions::LogicSkillClient_getData, offsets::LogicSkillClient__getData) && ok;
    ok = bind(functions::LogicSkillData_getCastingRange, offsets::LogicSkillData__getCastingRange) && ok;
    ok = bind(functions::LogicSkillData_getProjectileData, offsets::LogicSkillData__getProjectileData) && ok;

    ok = bind(functions::LogicProjectileData_getSpeed, offsets::LogicProjectileData_getSpeed) && ok;
    ok = bind(functions::LogicProjectileData_getRadius, offsets::LogicProjectileData_getRadius) && ok;
    ok = bind(functions::LogicProjectileData_getRendering, offsets::LogicProjectileData_getRendering) && ok;
    ok = bind(functions::LogicProjectileData_getSpawnAreaEffect, offsets::LogicProjectileData__getSpawnAreaEffect) && ok;
    ok = bind(functions::LogicProjectileData_getNumEarlyTicks, offsets::LogicProjectileData_getNumEarlyTicks) && ok;
    ok = bind(functions::LogicProjectileData_isBeam, offsets::LogicProjectileData_isBeam) && ok;

    ok = bind(functions::AreaEffectData_getRadius, offsets::AreaEffectData__getRadius) && ok;
    ok = bind(functions::AreaEffectData_getActiveTimeMs, offsets::AreaEffectData__getActiveTimeMs) && ok;

    ok = bind(functions::LogicData_getName, offsets::LogicData_getName) && ok;

    return ok;
}

bool functions::isInitialized()
{
    return g_base != 0 && functions::BattleMode_getInstance != nullptr;
}

uintptr_t functions::base()
{
    return g_base;
}
