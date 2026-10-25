#pragma once

#include <cstdint>
#include <cstddef>

namespace functions
{
    bool init(uintptr_t base_address);
    bool isInitialized();
    uintptr_t base();

    inline uintptr_t (*BattleMode_getInstance)() = nullptr;
    inline uintptr_t (*LogicBattleModeClient_getOwnCharacter)(uintptr_t) = nullptr;
    inline uint32_t (*LogicBattleModeClient_getOwnPlayerTeam)(uintptr_t) = nullptr;
    inline int32_t (*LogicBattleModeClient_getOwnPlayerIndex)(uintptr_t) = nullptr;
    inline uintptr_t (*LogicBattleModeClient_getTileMap)(uintptr_t) = nullptr;
    inline void (*LogicBattleModeClient_setClientPredictionMoveTo)(uintptr_t, int32_t, int32_t, int32_t) = nullptr;

    inline uintptr_t (*ClientInput_constructor_int)(uintptr_t, int32_t) = nullptr;
    inline void (*ClientInputManager_addInput)(uintptr_t, uintptr_t) = nullptr;
    inline uintptr_t (*operator_new)(size_t) = nullptr;

    inline int32_t (*LogicGameObjectClient_getX)(uintptr_t) = nullptr;
    inline int32_t (*LogicGameObjectClient_getY)(uintptr_t) = nullptr;
    inline uintptr_t (*LogicGameObjectClient_getData)(uintptr_t) = nullptr;
    inline uint32_t (*LogicGameObjectClient_getGlobalID)(uintptr_t) = nullptr;

    inline int32_t (*LogicCharacterData_getSpeed)(uintptr_t) = nullptr;
    inline uint32_t (*LogicCharacterData_getCollisionRadius)(uintptr_t) = nullptr;
    inline uintptr_t (*LogicCharacterClient_getWeaponSkill)(uintptr_t) = nullptr;
    inline uintptr_t (*LogicCharacterClient_getSkillAt)(uintptr_t, int32_t) = nullptr;

    inline uintptr_t (*LogicSkillClient_getData)(uintptr_t, int32_t) = nullptr;
    inline int32_t (*LogicSkillData_getCastingRange)(uintptr_t) = nullptr;
    inline uintptr_t (*LogicSkillData_getProjectileData)(uintptr_t, int32_t) = nullptr;

    inline uint32_t (*LogicProjectileData_getSpeed)(uintptr_t) = nullptr;
    inline uint32_t (*LogicProjectileData_getRadius)(uintptr_t) = nullptr;
    inline uint32_t (*LogicProjectileData_getNumEarlyTicks)(uintptr_t) = nullptr;
    inline uint32_t (*LogicProjectileData_isBeam)(uintptr_t) = nullptr;

    inline uintptr_t (*LogicData_getName)(uintptr_t) = nullptr;

    inline void (*BattleScreen_calculateProjectilePath)(void *, void *, void *, void *, int32_t, float, float, float, float, float, float) = nullptr;
}
