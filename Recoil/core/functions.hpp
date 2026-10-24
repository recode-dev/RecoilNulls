#pragma once

#include <cstdint>
#include <cstddef>

namespace functions
{
    void init(uintptr_t base);
    bool isInitialized();
    uintptr_t base();

    uintptr_t BattleMode_getInstance();

    int32_t LogicGameObjectClient_getX(uintptr_t object);
    int32_t LogicGameObjectClient_getY(uintptr_t object);
    uintptr_t LogicGameObjectClient_getData(uintptr_t object);
    uint32_t LogicGameObjectClient_getGlobalID(uintptr_t object);

    uintptr_t LogicBattleModeClient_getOwnCharacter(uintptr_t battleMode);
    uint32_t LogicBattleModeClient_getOwnPlayerTeam(uintptr_t battleMode);
    int32_t LogicBattleModeClient_getOwnPlayerIndex(uintptr_t battleMode);
    uintptr_t LogicBattleModeClient_getTileMap(uintptr_t battleMode);
    void LogicBattleModeClient_setClientPredictionMoveTo(uintptr_t battleMode, int32_t x, int32_t y, int32_t flags);

    uintptr_t ClientInput_constructor_int(uintptr_t input, int32_t type);
    void ClientInputManager_addInput(uintptr_t manager, uintptr_t input);

    int32_t LogicCharacterData_getSpeed(uintptr_t data);
    uint32_t LogicCharacterData_getCollisionRadius(uintptr_t data);

    uintptr_t LogicCharacterClient_getWeaponSkill(uintptr_t character);
    uintptr_t LogicCharacterClient_getSkillAt(uintptr_t character, int32_t slot);

    uintptr_t LogicSkillClient_getData(uintptr_t skill, int32_t hyper);
    int32_t LogicSkillData_getCastingRange(uintptr_t skillData);
    uintptr_t LogicSkillData_getProjectileData(uintptr_t skillData, int32_t index);

    uint32_t LogicProjectileData_getSpeed(uintptr_t data);
    uint32_t LogicProjectileData_getRadius(uintptr_t data);
    int32_t LogicProjectileData_getRendering(uintptr_t data);
    uintptr_t LogicProjectileData_getSpawnAreaEffect(uintptr_t data);
    uint32_t LogicProjectileData_getNumEarlyTicks(uintptr_t data);
    uint32_t LogicProjectileData_isBeam(uintptr_t data);

    int32_t AreaEffectData_getRadius(uintptr_t data);
    int32_t AreaEffectData_getActiveTimeMs(uintptr_t data);

    uintptr_t LogicData_getName(uintptr_t data);

    uintptr_t operator_new(size_t size);
}
