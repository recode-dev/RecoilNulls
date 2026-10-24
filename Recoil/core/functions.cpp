#include <cstdint>
#include <cstddef>

#include "offsets.cpp"

namespace fn
{
    using GetInstance = void *(*)();
    using Int32OfPtr = int32_t (*)(void *);
    using PtrOfPtr = void *(*)(void *);
    using VoidOfPtr3Int = void (*)(void *, int, int, int);
    using PtrOfPtrInt = void *(*)(void *, int);
    using VoidOfPtrPtr = void (*)(void *, void *);
    using U32OfPtr = uint32_t (*)(void *);
    using PtrOfULong = void *(*)(unsigned long);

    struct Functions
    {
        GetInstance BattleMode_getInstance = nullptr;
        Int32OfPtr LogicGameObjectClient_getX = nullptr;
        Int32OfPtr LogicGameObjectClient_getY = nullptr;
        PtrOfPtr LogicBattleModeClient_getOwnCharacter = nullptr;
        VoidOfPtr3Int LogicBattleModeClient_setClientPredictionMoveTo = nullptr;
        PtrOfPtrInt ClientInput_constructor_int = nullptr;
        VoidOfPtrPtr ClientInputManager_addInput = nullptr;
        U32OfPtr LogicGameObjectClient_getGlobalID = nullptr;
        U32OfPtr LogicBattleModeClient_getOwnPlayerTeam = nullptr;
        Int32OfPtr LogicBattleModeClient_getOwnPlayerIndex = nullptr;
        PtrOfPtr LogicGameObjectClient_getData = nullptr;
        U32OfPtr LogicProjectileData_getSpeed = nullptr;
        U32OfPtr LogicProjectileData_getRadius = nullptr;
        PtrOfPtr LogicProjectileData_getSpawnAreaEffect = nullptr;
        U32OfPtr LogicProjectileData_getNumEarlyTicks = nullptr;
        U32OfPtr LogicProjectileData_isBeam = nullptr;
        Int32OfPtr AreaEffectData_getRadius = nullptr;
        Int32OfPtr AreaEffectData_getActiveTimeMs = nullptr;
        Int32OfPtr LogicProjectileData_getRendering = nullptr;
        Int32OfPtr LogicCharacterData_getSpeed = nullptr;
        U32OfPtr LogicCharacterData_getCollisionRadius = nullptr;
        PtrOfPtr LogicBattleModeClient_getTileMap = nullptr;
        PtrOfPtr LogicProjectileClient_getData = nullptr;
        PtrOfPtr LogicCharacterClient_getWeaponSkill = nullptr;
        Int32OfPtr LogicSkillData_getCastingRange = nullptr;
        PtrOfPtrInt LogicSkillData_getProjectileData = nullptr;
        PtrOfPtrInt LogicSkillClient_getData = nullptr;
        PtrOfPtr LogicData_getName = nullptr;
        PtrOfPtrInt LogicCharacterClient_getSkillAt = nullptr;
        PtrOfULong operator_new = nullptr;
    };

    inline uintptr_t base = 0;
    inline Functions fns;
    inline bool initialized = false;

    template <typename T>
    inline T nf(uintptr_t moduleBase, uintptr_t offset)
    {
        if (offset == 0) return nullptr;
        return reinterpret_cast<T>(moduleBase + offset);
    }

    inline void initFunctions(uintptr_t baseIn)
    {
        if (initialized) return;
        base = baseIn;

        fns.BattleMode_getInstance = nf<GetInstance>(base, offsets::BattleMode_getInstance);
        fns.LogicGameObjectClient_getX = nf<Int32OfPtr>(base, offsets::LogicGameObjectClient_getX);
        fns.LogicGameObjectClient_getY = nf<Int32OfPtr>(base, offsets::LogicGameObjectClient_getY);
        fns.LogicBattleModeClient_getOwnCharacter = nf<PtrOfPtr>(base, offsets::LogicBattleModeClient_getOwnCharacter);
        fns.LogicBattleModeClient_setClientPredictionMoveTo = nf<VoidOfPtr3Int>(base, offsets::LogicBattleModeClient_setClientPredictionMoveTo);
        fns.ClientInput_constructor_int = nf<PtrOfPtrInt>(base, offsets::ClientInput_constructor_int);
        fns.ClientInputManager_addInput = nf<VoidOfPtrPtr>(base, offsets::ClientInputManager_addInput);
        fns.LogicGameObjectClient_getGlobalID = nf<U32OfPtr>(base, offsets::LogicGameObjectClient_getGlobalID);
        fns.LogicBattleModeClient_getOwnPlayerTeam = nf<U32OfPtr>(base, offsets::LogicBattleModeClient_getOwnPlayerTeam);
        fns.LogicBattleModeClient_getOwnPlayerIndex = nf<Int32OfPtr>(base, offsets::LogicBattleModeClient__getOwnPlayerIndex);
        fns.LogicGameObjectClient_getData = nf<PtrOfPtr>(base, offsets::LogicGameObjectClient_getData);
        fns.LogicProjectileData_getSpeed = nf<U32OfPtr>(base, offsets::LogicProjectileData_getSpeed);
        fns.LogicProjectileData_getRadius = nf<U32OfPtr>(base, offsets::LogicProjectileData_getRadius);
        fns.LogicProjectileData_getSpawnAreaEffect = nf<PtrOfPtr>(base, offsets::LogicProjectileData__getSpawnAreaEffect);
        fns.LogicProjectileData_getNumEarlyTicks = nf<U32OfPtr>(base, offsets::LogicProjectileData_getNumEarlyTicks);
        fns.LogicProjectileData_isBeam = nf<U32OfPtr>(base, offsets::LogicProjectileData_isBeam);
        fns.AreaEffectData_getRadius = nf<Int32OfPtr>(base, offsets::AreaEffectData__getRadius);
        fns.AreaEffectData_getActiveTimeMs = nf<Int32OfPtr>(base, offsets::AreaEffectData__getActiveTimeMs);
        fns.LogicProjectileData_getRendering = nf<Int32OfPtr>(base, offsets::LogicProjectileData_getRendering);
        fns.LogicCharacterData_getSpeed = nf<Int32OfPtr>(base, offsets::LogicCharacterData_getSpeed);
        fns.LogicCharacterData_getCollisionRadius = nf<U32OfPtr>(base, offsets::LogicCharacterData_getCollisionRadius);
        fns.LogicBattleModeClient_getTileMap = nf<PtrOfPtr>(base, offsets::LogicBattleModeClient__getTileMap);
        fns.LogicProjectileClient_getData = nf<PtrOfPtr>(base, 0);
        fns.LogicCharacterClient_getWeaponSkill = nf<PtrOfPtr>(base, offsets::LogicCharacterClient__getWeaponSkill);
        fns.LogicSkillData_getCastingRange = nf<Int32OfPtr>(base, offsets::LogicSkillData__getCastingRange);
        fns.LogicSkillData_getProjectileData = nf<PtrOfPtrInt>(base, offsets::LogicSkillData__getProjectileData);
        fns.LogicSkillClient_getData = nf<PtrOfPtrInt>(base, offsets::LogicSkillClient__getData);
        fns.LogicData_getName = nf<PtrOfPtr>(base, offsets::LogicData_getName);
        fns.LogicCharacterClient_getSkillAt = nf<PtrOfPtrInt>(base, offsets::LogicCharacterClient__getSkillAt);
        fns.operator_new = nf<PtrOfULong>(base, offsets::operator_new);

        initialized = true;
    }

    inline Functions &getFunctions()
    {
        return fns;
    }

    inline uintptr_t getBase()
    {
        return base;
    }
}