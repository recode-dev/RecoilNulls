#pragma once

#include <cstdint>
#include <cstddef>

namespace offsets
{
    inline constexpr uintptr_t BattleMode_battleScreenPtr                       = 0x8;
    inline constexpr uintptr_t BattleMode_objectManagerPtr                      = 0x58;
    inline constexpr uintptr_t BattleMode_clientInputManager                    = 0x58;
    inline constexpr uintptr_t BattleMode_hashKey                               = 0x60;
    inline constexpr uintptr_t BattleMode_getInstance                           = 0x8c5130;

    inline constexpr uintptr_t AppState                                         = 0x1123e58;
    inline constexpr uintptr_t AppState_state                                   = 0x50;
    inline constexpr uintptr_t AppState_scene                                   = 0x48;
    inline constexpr uintptr_t Scene_client                                     = 0x28;
    inline constexpr uintptr_t Client_ownIndex                                  = 0xe0;
    inline constexpr uintptr_t Client_ownTeam                                   = 0xe4;

    inline constexpr uintptr_t ObjectManager_objectsArray                       = 0x0;
    inline constexpr uintptr_t ObjectManager_count                              = 0xc;
    inline constexpr uintptr_t ObjectManager_ptrStride                          = 0x8;

    inline constexpr uintptr_t TileMap_Width                                    = 0xc4;
    inline constexpr uintptr_t TileMap_Height                                   = 0xc8;
    inline constexpr uintptr_t TileMap_TilesArray                               = 0x20;

    inline constexpr uintptr_t TileTypeData_BlocksMovement                      = 0x56;

    inline constexpr uintptr_t GameObj_team                                     = 0x40;
    inline constexpr uintptr_t GameObj_hp                                       = 0xc4;

    inline constexpr uintptr_t LogicGameObjectClient_ownerIndex                 = 0x3c;
    inline constexpr uintptr_t LogicGameObjectClient_getX                       = 0xa23afc;
    inline constexpr uintptr_t LogicGameObjectClient_getY                       = 0xa23b04;
    inline constexpr uintptr_t LogicGameObjectClient_getData                    = 0x117050;
    inline constexpr uintptr_t LogicGameObjectClient_getGlobalID                = 0x46464c;

    inline constexpr uintptr_t LogicCharacterClient_body                        = 0xa0;
    inline constexpr uintptr_t LogicCharacterClient_predictionX                 = 0xfcc;
    inline constexpr uintptr_t LogicCharacterClient_predictionY                 = 0xfd0;
    inline constexpr uintptr_t LogicCharacterClient_predictionFlags1            = 0xf9c;
    inline constexpr uintptr_t LogicCharacterClient_predictionFlags2            = 0xfa0;
    inline constexpr uintptr_t LogicCharacterClient__getWeaponSkill             = 0x9fc440;
    inline constexpr uintptr_t LogicCharacterClient__getSkillAt                 = 0xa007fc;

    inline constexpr uintptr_t LogicCharacterData_getSpeed                      = 0x9911ec;
    inline constexpr uintptr_t LogicCharacterData_getCollisionRadius            = 0x9913e8;

    inline constexpr uintptr_t CharData_brawlerId                               = 0x35c;

    inline constexpr uintptr_t HeroData_namePtr                                 = 0x18;

    inline constexpr uintptr_t LogicProjectileClient_ownerTeam                  = 0x40;
    inline constexpr uintptr_t LogicProjectileClient_dataName                   = 0x18;
    inline constexpr uintptr_t LogicProjectileClient_ctor                       = 0xa313f8;
    inline constexpr uintptr_t LogicProjectileClient_destruct                   = 0xa31a00;

    inline constexpr uintptr_t Projectile_isIndirect                            = 0x118;

    inline constexpr uintptr_t ProjectileData_speed                             = 0x6c;
    inline constexpr uintptr_t ProjectileData_radius                            = 0x74;

    inline constexpr uintptr_t LogicProjectileData_getSpeed                     = 0x8cee94;
    inline constexpr uintptr_t LogicProjectileData_getRadius                    = 0x860d88;
    inline constexpr uintptr_t LogicProjectileData_getNumEarlyTicks             = 0x979d50;
    inline constexpr uintptr_t LogicProjectileData_isBeam                       = 0x9b6448;

    inline constexpr uintptr_t ClientInput_x                                    = 0xc;
    inline constexpr uintptr_t ClientInput_y                                    = 0x10;
    inline constexpr uintptr_t ClientInput_typeConstantTable                    = 0xe1bd30;
    inline constexpr uintptr_t ClientInput_typeFallback                         = 0xe1bc28;
    inline constexpr uintptr_t ClientInput_hashInnerMask                        = 0xe1bbd0;
    inline constexpr uintptr_t ClientInput_hashOuterMask                        = 0xe1bbf0;
    inline constexpr uintptr_t ClientInput_constructor_int                      = 0xa95798;

    inline constexpr uintptr_t ClientInputManager_addInput                      = 0x74675c;

    inline constexpr uintptr_t ScString_length                                  = 0x4;

    inline constexpr uintptr_t LogicBattleModeClient_update                     = 0xac1ee8;
    inline constexpr uintptr_t LogicBattleModeClient__getTileMap                = 0x991440;
    inline constexpr uintptr_t LogicBattleModeClient_setClientPredictionMoveTo  = 0xac3a58;

    inline constexpr uintptr_t BattleScreen_activateSkill                       = 0x7a0084;
    inline constexpr uintptr_t BattleScreen_getClosestTargetForAutoshoot        = 0x7b67ec;
    inline constexpr uintptr_t BattleScreen__calculateProjectilePath            = 0x7b3ae8;

    inline constexpr uintptr_t LogicTile__setData                               = 0x9401fc;

    inline constexpr uintptr_t LogicSkillClient__getData                        = 0x1e5650;

    inline constexpr uintptr_t LogicSkillData__getCastingRange                  = 0x9de9cc;
    inline constexpr uintptr_t LogicSkillData__getProjectileData                = 0x9e84a8;

    inline constexpr uintptr_t LogicData_getName                                = 0x9a4f1c;

    inline constexpr uintptr_t operator_new                                     = 0xd8da2c;

    inline constexpr uintptr_t Vtable_projectileData                            = 0xff3b18;
}
