#ifndef OFFSETS_H
#define OFFSETS_H

#include <cstddef>
#include <cstdint>

inline constexpr uint64_t GameStateManagerRva                = 0x1123e58ULL;
inline constexpr uint64_t GameStateManagerCurrentStateOffset = 0x48ULL;
inline constexpr uint64_t GameStateManagerStateOffset        = 0x50ULL;
inline constexpr uint64_t GameStateClientOffset              = 0x28ULL;

inline constexpr uint64_t LogicBattleModeClientInputManagerOffset = 0x58ULL;
inline constexpr uint64_t LogicBattleModeClientGameObjectManagerOffset =
    0x28ULL;
inline constexpr uint64_t LogicBattleModeClientHashKeyOffset     = 0x60ULL;
inline constexpr uint64_t LogicBattleModeClientHashEnabledOffset = 0x70ULL;

inline constexpr uint64_t LogicGameObjectManagerServerGameObjectsOffset =
    0x0ULL;
inline constexpr uint64_t LogicGameObjectManagerServerGameObjectCountOffset =
    0xcULL;
inline constexpr uint64_t LogicGameObjectDataOffset               = 0x10ULL;
inline constexpr uint64_t LogicGameObjectDataScanSpan             = 0x200ULL;
inline constexpr uint64_t LogicCharacterGlobalIdOffset            = 0x8ULL;
inline constexpr uint64_t LogicCharacterXOffset                   = 0x30ULL;
inline constexpr uint64_t LogicCharacterYOffset                   = 0x34ULL;
inline constexpr uint64_t LogicCharacterTeamOffset                = 0x40ULL;
inline constexpr uint64_t LogicCharacterAttackAngleOffset         = 0xb4ULL;
inline constexpr uint64_t LogicCharacterHitpointsOffset           = 0xc4ULL;
inline constexpr uint64_t LogicCharacterDataCollisionRadiusOffset = 0x2acULL;
inline constexpr uint64_t LogicCharacterCurrentChargeUpMillisecondsOffset =
    0x30cULL;
inline constexpr uint64_t LogicCharacterChargeUpModeOffset        = 0x310ULL;
inline constexpr uint64_t LogicCharacterChargeUpMillisecondsOffset = 0x314ULL;

inline constexpr uint64_t LogicProjectileServerXOffset               = 0x38ULL;
inline constexpr uint64_t LogicProjectileServerYOffset               = 0x3cULL;
inline constexpr uint64_t LogicProjectileServerTickStartOffset       = 0x28ULL;
inline constexpr uint64_t LogicProjectileServerTickEndOffset         = 0x2cULL;
inline constexpr uint64_t LogicProjectileServerStartXOffset          = 0xd0ULL;
inline constexpr uint64_t LogicProjectileServerStartYOffset          = 0xd4ULL;
inline constexpr uint64_t LogicProjectileServerTargetXOffset         = 0xd8ULL;
inline constexpr uint64_t LogicProjectileServerTargetYOffset         = 0xdcULL;
inline constexpr uint64_t LogicProjectileServerOwnerGameObjectOffset = 0x100ULL;
inline constexpr uint64_t LogicProjectileDataSpeedOffset         = 0x124ULL;
inline constexpr size_t   LogicProjectileDataSize                = 0x1e8ULL;

inline constexpr uint64_t LogicTileDataOffset           = 0x0ULL;
inline constexpr uint64_t LogicTilePathFinderMaskOffset = 0x10ULL;
inline constexpr uint64_t LogicTilePathFinderMaskSize   = 0x8ULL;
inline constexpr uint64_t LogicTileWaterPathFinderMaskOffset =
    LogicTilePathFinderMaskOffset + LogicTilePathFinderMaskSize;
inline constexpr uint64_t LogicTileHardBlockMaskOffset =
    LogicTileWaterPathFinderMaskOffset + LogicTilePathFinderMaskSize;
inline constexpr uintptr_t LogicTilePathFinderMaskOffsetCandidates[3] = {
    LogicTilePathFinderMaskOffset,
    LogicTilePathFinderMaskOffset - LogicTilePathFinderMaskSize,
    LogicTilePathFinderMaskOffset + LogicTilePathFinderMaskSize};
inline constexpr uint64_t LogicTilePathFinderMaskReadWindowSize =
    LogicTilePathFinderMaskOffset + 6 * LogicTilePathFinderMaskSize;
inline constexpr uintptr_t LogicTilePathFinderMaskReadWindows[5] = {
    LogicTilePathFinderMaskOffset + 6 * LogicTilePathFinderMaskSize,
    LogicTilePathFinderMaskOffset + 5 * LogicTilePathFinderMaskSize,
    LogicTilePathFinderMaskOffset + 4 * LogicTilePathFinderMaskSize,
    LogicTilePathFinderMaskOffset + 3 * LogicTilePathFinderMaskSize,
    LogicTilePathFinderMaskOffset + 2 * LogicTilePathFinderMaskSize};
inline constexpr uint64_t LogicTileDataBlocksMovementOffset = 0x56ULL;
inline constexpr uint64_t LogicTileDataBlockFlagsSize       = 2ULL;
inline constexpr unsigned LogicTileDataProjectileFlagShift  = 8;

inline constexpr uint64_t LogicTileMapTileArrayOffset = 0x20ULL;
inline constexpr uint64_t LogicTileMapWidthOffset     = 0xc4ULL;
inline constexpr uint64_t LogicTileMapHeightOffset    = 0xc8ULL;

inline constexpr uint64_t StringByteLengthOffset = 0x4ULL;
inline constexpr uint64_t StringCharactersOffset = 0x8ULL;

inline constexpr uint64_t ClientInputManagerInputQueueOffset = 0x20ULL;
inline constexpr uint64_t ClientInputMessageSignedDataOffset = 0x4ULL;
inline constexpr uint64_t ClientInputMessageSignedDataSize   = 0x10ULL;
inline constexpr uint64_t ClientInputMessageTypeOffset       = 0x8ULL;
inline constexpr uint64_t ClientInputMessageXOffset          = 0xcULL;
inline constexpr uint64_t ClientInputMessageYOffset          = 0x10ULL;
inline constexpr uint64_t ClientInputMessageChecksumOffset   = 0x34ULL;
inline constexpr uint64_t ClientInputMessageSkillDataOffset  = 0x40ULL;
inline constexpr uint64_t ClientInputMessageSize             = 0x48ULL;
inline constexpr uint64_t ClientInputMessageProbeSize        = 0x80ULL;
inline constexpr uint64_t ClientInputTypeCount               = 0x17ULL;

inline constexpr uint64_t LogicDataGetNameRva                     = 0x9a4f1cULL;
inline constexpr uint64_t BattleModeGetInstanceRva                = 0x8c5130ULL;
inline constexpr uint64_t LogicBattleModeClientGetTileMapRva      = 0x991440ULL;
inline constexpr uint64_t LogicBattleModeClientGetOwnCharacterRva = 0xac3934ULL;
inline constexpr uint64_t LogicBattleModeClientGetOwnPlayerIndexRva =
    0x9cda04ULL;
inline constexpr uint64_t LogicBattleModeClientGetOwnPlayerTeamRva =
    0xa7f47cULL;
inline constexpr uint64_t LogicBattleModeClientSetClientPredictionMoveToRva =
    0xac3a58ULL;
inline constexpr uint64_t ClientInputMessageConstructorRva        = 0xa95798ULL;
inline constexpr uint64_t ClientInputManagerAddInputRva           = 0x74675cULL;
inline constexpr uint64_t LogicTileMapIsPassablePathFinderRva     = 0x942ebcULL;
inline constexpr uint64_t LogicTileMapIsPlayerLineOfSightClearRva = 0x943b08ULL;
inline constexpr uint64_t LogicTileMapTileToLogicRva              = 0x943040ULL;
inline constexpr uint64_t LogicTileMapLogicToTileRva              = 0x943028ULL;
inline constexpr uint64_t LogicProjectileDataVtableRva            = 0xff3b18ULL;
inline constexpr uint64_t LogicProjectileClientGetTravelTicksRva  = 0xa3192cULL;

inline constexpr uint64_t ClientInputTypeTableRva     = 0xe1bd30ULL;
inline constexpr uint64_t ClientInputTypeFallbackRva  = 0xe1bc28ULL;
inline constexpr uint64_t ClientInputHashInnerMaskRva = 0xe1bbd0ULL;
inline constexpr uint64_t ClientInputHashOuterMaskRva = 0xe1bbf0ULL;
inline constexpr uint64_t ClientInputTypeTableToFallbackDistance =
    ClientInputTypeTableRva - ClientInputTypeFallbackRva;
inline constexpr uint64_t ClientInputTypeTableToInnerMaskDistance =
    ClientInputTypeTableRva - ClientInputHashInnerMaskRva;
inline constexpr uint64_t ClientInputTypeTableToOuterMaskDistance =
    ClientInputTypeTableRva - ClientInputHashOuterMaskRva;

inline constexpr uint64_t DataConstRvaLow  = 0xf74000ULL;
inline constexpr uint64_t DataConstRvaSize = 0xd4000ULL;

#endif
