#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <mach/mach.h>
#include "offsets.cpp"

namespace
{
    uintptr_t g_base = 0;

    bool readBytes(uintptr_t address, void *out, size_t size)
    {
        if (address == 0 || out == nullptr || size == 0) return false;
        vm_size_t copied = 0;
        kern_return_t result = vm_read_overwrite(mach_task_self(),
                                                (vm_address_t)address,
                                                (vm_size_t)size,
                                                (vm_address_t)out,
                                                &copied);
        return result == KERN_SUCCESS && copied == (vm_size_t)size;
    }

    template <typename T>
    T resolve(uintptr_t offset)
    {
        if (g_base == 0 || offset == 0) return nullptr;
        const uintptr_t target = g_base + offset;
        uint32_t probe = 0;
        if (!readBytes(target, &probe, sizeof(probe))) return nullptr;
        return reinterpret_cast<T>(target);
    }

    template <uintptr_t Offset, typename R, typename... Args>
    R invoke(Args... args)
    {
        static R (*fn)(Args...) = nullptr;
        if (fn == nullptr) fn = resolve<R (*)(Args...)>(Offset);
        if (fn == nullptr)
        {
            if constexpr (std::is_void_v<R>) return;
            else return R{};
        }
        return fn(args...);
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
        return invoke<offsets::BattleMode_getInstance, uintptr_t>();
    }

    int32_t LogicGameObjectClient_getX(uintptr_t object)
    {
        return object != 0 ? invoke<offsets::LogicGameObjectClient_getX, int32_t>(object) : 0;
    }

    int32_t LogicGameObjectClient_getY(uintptr_t object)
    {
        return object != 0 ? invoke<offsets::LogicGameObjectClient_getY, int32_t>(object) : 0;
    }

    uintptr_t LogicBattleModeClient_getOwnCharacter(uintptr_t battleMode)
    {
        return battleMode != 0 ? invoke<offsets::LogicBattleModeClient_getOwnCharacter, uintptr_t>(battleMode) : 0;
    }

    void LogicBattleModeClient_setClientPredictionMoveTo(uintptr_t battleMode, int32_t x, int32_t y, int32_t flags)
    {
        if (battleMode != 0) invoke<offsets::LogicBattleModeClient_setClientPredictionMoveTo, void>(battleMode, x, y, flags);
    }

    uintptr_t ClientInput_constructor_int(uintptr_t input, int32_t type)
    {
        return input != 0 ? invoke<offsets::ClientInput_constructor_int, uintptr_t>(input, type) : 0;
    }

    void ClientInputManager_addInput(uintptr_t manager, uintptr_t input)
    {
        if (manager != 0 && input != 0) invoke<offsets::ClientInputManager_addInput, void>(manager, input);
    }

    uint32_t LogicGameObjectClient_getGlobalID(uintptr_t object)
    {
        return object != 0 ? invoke<offsets::LogicGameObjectClient_getGlobalID, uint32_t>(object) : 0;
    }

    uint32_t LogicBattleModeClient_getOwnPlayerTeam(uintptr_t battleMode)
    {
        return battleMode != 0 ? invoke<offsets::LogicBattleModeClient_getOwnPlayerTeam, uint32_t>(battleMode) : 0;
    }

    int32_t LogicBattleModeClient_getOwnPlayerIndex(uintptr_t battleMode)
    {
        return battleMode != 0 ? invoke<offsets::LogicBattleModeClient__getOwnPlayerIndex, int32_t>(battleMode) : -1;
    }

    uintptr_t LogicGameObjectClient_getData(uintptr_t object)
    {
        return object != 0 ? invoke<offsets::LogicGameObjectClient_getData, uintptr_t>(object) : 0;
    }

    uint32_t LogicProjectileData_getSpeed(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::LogicProjectileData_getSpeed, uint32_t>(data) : 0;
    }

    uint32_t LogicProjectileData_getRadius(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::LogicProjectileData_getRadius, uint32_t>(data) : 0;
    }

    uintptr_t LogicProjectileData_getSpawnAreaEffect(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::LogicProjectileData__getSpawnAreaEffect, uintptr_t>(data) : 0;
    }

    uint32_t LogicProjectileData_getNumEarlyTicks(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::LogicProjectileData_getNumEarlyTicks, uint32_t>(data) : 0;
    }

    uint32_t LogicProjectileData_isBeam(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::LogicProjectileData_isBeam, uint32_t>(data) : 0;
    }

    int32_t AreaEffectData_getRadius(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::AreaEffectData__getRadius, int32_t>(data) : 0;
    }

    int32_t AreaEffectData_getActiveTimeMs(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::AreaEffectData__getActiveTimeMs, int32_t>(data) : 0;
    }

    int32_t LogicProjectileData_getRendering(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::LogicProjectileData_getRendering, int32_t>(data) : 0;
    }

    int32_t LogicCharacterData_getSpeed(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::LogicCharacterData_getSpeed, int32_t>(data) : 0;
    }

    uint32_t LogicCharacterData_getCollisionRadius(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::LogicCharacterData_getCollisionRadius, uint32_t>(data) : 0;
    }

    uintptr_t LogicBattleModeClient_getTileMap(uintptr_t battleMode)
    {
        return battleMode != 0 ? invoke<offsets::LogicBattleModeClient__getTileMap, uintptr_t>(battleMode) : 0;
    }

    uintptr_t LogicCharacterClient_getWeaponSkill(uintptr_t character)
    {
        return character != 0 ? invoke<offsets::LogicCharacterClient__getWeaponSkill, uintptr_t>(character) : 0;
    }

    int32_t LogicSkillData_getCastingRange(uintptr_t skillData)
    {
        return skillData != 0 ? invoke<offsets::LogicSkillData__getCastingRange, int32_t>(skillData) : 0;
    }

    uintptr_t LogicSkillData_getProjectileData(uintptr_t skillData, int32_t index)
    {
        return skillData != 0 ? invoke<offsets::LogicSkillData__getProjectileData, uintptr_t>(skillData, index) : 0;
    }

    uintptr_t LogicSkillClient_getData(uintptr_t skill, int32_t hyper)
    {
        return skill != 0 ? invoke<offsets::LogicSkillClient__getData, uintptr_t>(skill, hyper) : 0;
    }

    uintptr_t LogicData_getName(uintptr_t data)
    {
        return data != 0 ? invoke<offsets::LogicData_getName, uintptr_t>(data) : 0;
    }

    uintptr_t LogicCharacterClient_getSkillAt(uintptr_t character, int32_t slot)
    {
        return character != 0 ? invoke<offsets::LogicCharacterClient__getSkillAt, uintptr_t>(character, slot) : 0;
    }

    uintptr_t operator_new(size_t size)
    {
        return size != 0 ? invoke<offsets::operator_new, uintptr_t>(size) : 0;
    }
}