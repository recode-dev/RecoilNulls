#ifndef OFFSETS_H
#define OFFSETS_H

#include <cstdint>

inline constexpr uint64_t RVA_BATTLE_MODE_GET_INSTANCE = 0x8c5130ULL;
inline constexpr uint64_t RVA_LOGIC_BATTLE_MODE_CLIENT_GET_TILE_MAP =
    0x991440ULL;
inline constexpr uint64_t RVA_LOGIC_BATTLE_MODE_CLIENT_GET_OWN_CHARACTER =
    0xac3934ULL;
inline constexpr uint64_t RVA_LOGIC_BATTLE_MODE_CLIENT_GET_OWN_PLAYER_INDEX =
    0x9cda04ULL;
inline constexpr uint64_t RVA_LOGIC_BATTLE_MODE_CLIENT_GET_OWN_PLAYER_TEAM =
    0xa7f47cULL;
inline constexpr uint64_t RVA_LOGIC_DATA_GET_NAME = 0x9a4f1cULL;
inline constexpr uint64_t
    RVA_LOGIC_BATTLE_MODE_CLIENT_SET_CLIENT_PREDICTION_MOVE_TO = 0xac3a58ULL;
inline constexpr uint64_t RVA_CLIENT_INPUT_MESSAGE_CONSTRUCTOR = 0x00a95798ULL;
inline constexpr uint64_t RVA_CLIENT_INPUT_MANAGER_ADD_INPUT = 0x74675cULL;
inline constexpr uint64_t RVA_LOGIC_TILE_MAP_IS_PASSABLE_PATH_FINDER =
    0x942ebcULL;
inline constexpr uint64_t LOGIC_TILE_BLOCK_MASK_OFF = 0x10ULL;
inline constexpr uint64_t LOGIC_TILE_WATER_MASK_OFF = 0x18ULL;
inline constexpr uint64_t LOGIC_TILE_THIRD_MASK_OFF = 0x20ULL;
inline constexpr uint64_t LOGIC_TILE_FOURTH_MASK_OFF = 0x28ULL;
inline constexpr uint64_t RVA_LOGIC_TILE_MAP_IS_PLAYER_LINE_OF_SIGHT_CLEAR =
    0x943b08ULL;
inline constexpr uint64_t RVA_LOGIC_TILE_MAP_TILE_TO_LOGIC = 0x943040ULL;
inline constexpr uint64_t RVA_LOGIC_TILE_MAP_LOGIC_TO_TILE = 0x943028ULL;

inline constexpr uint64_t GAME_STATE_RVA = 0x1123e58ULL;
inline constexpr uint64_t VTABLE_LOGIC_PROJECTILE_DATA_RVA = 0x000ff3b18ULL;
inline constexpr uint64_t CLIENT_INPUT_TYPE_TABLE_RVA = 0x00e1bd30ULL;
inline constexpr uint64_t CLIENT_INPUT_TYPE_FALLBACK_RVA = 0x00e1bc28ULL;
inline constexpr uint64_t CLIENT_INPUT_HASH_INNER_MASK_RVA = 0x00e1bbd0ULL;
inline constexpr uint64_t CLIENT_INPUT_HASH_OUTER_MASK_RVA = 0x00e1bbf0ULL;
inline constexpr uint64_t DATA_CONST_RVA_LOW = 0xf74000ULL;
inline constexpr uint64_t DATA_CONST_RVA_SIZE = 0xd4000ULL;

inline constexpr uint64_t GAME_STATE_STATE_OFF = 0x50ULL;
inline constexpr uint64_t GAME_STATE_SCENE_OFF = 0x48ULL;
inline constexpr uint64_t SCENE_CLIENT_OFF = 0x28ULL;
inline constexpr uint64_t LOGIC_BATTLE_MODE_CLIENT_INPUT_MANAGER_OFF = 0x58ULL;
inline constexpr uint64_t LOGIC_BATTLE_MODE_CLIENT_GAME_OBJECT_MANAGER_OFF =
    0x28ULL;
inline constexpr uint64_t BATTLE_MODE_HASH_ENABLED_OFF = 0x70ULL;
inline constexpr uint64_t BATTLE_MODE_HASH_KEY_OFF = 0x60ULL;
inline constexpr uint64_t LOGIC_GAME_OBJECT_MANAGER_SERVER_ARRAY_OFF = 0x0ULL;
inline constexpr uint64_t LOGIC_GAME_OBJECT_MANAGER_SERVER_COUNT_OFF = 0xcULL;

inline constexpr uint64_t LOGIC_CHARACTER_GLOBAL_ID_OFF = 0x8ULL;
inline constexpr uint64_t LOGIC_CHARACTER_X_OFF = 0x30ULL;
inline constexpr uint64_t LOGIC_CHARACTER_Y_OFF = 0x34ULL;
inline constexpr uint64_t LOGIC_CHARACTER_TEAM_OFF = 0x40ULL;
inline constexpr uint64_t LOGIC_CHARACTER_ATTACK_ANGLE_OFF = 0xb4ULL;
inline constexpr uint64_t LOGIC_CHARACTER_HP_OFF = 0xc4ULL;
inline constexpr uint64_t LOGIC_GAME_OBJECT_DATA_OFF = 0x10ULL;
inline constexpr uint64_t LOGIC_CHARACTER_DATA_COLLISION_OFF = 0x2acULL;

inline constexpr uint64_t LOGIC_PROJECTILE_SERVER_X_OFF = 0x38ULL;
inline constexpr uint64_t LOGIC_PROJECTILE_SERVER_Y_OFF = 0x3cULL;
inline constexpr uint64_t LOGIC_PROJECTILE_SERVER_OWNER_OFF = 0x100ULL;
inline constexpr uint64_t LOGIC_PROJECTILE_SERVER_STARTTICK_OFF = 0x28ULL;
inline constexpr uint64_t LOGIC_PROJECTILE_SERVER_ENDTICK_OFF = 0x2cULL;
inline constexpr uint64_t LOGIC_PROJECTILE_SERVER_STARTX_OFF = 0xd0ULL;
inline constexpr uint64_t LOGIC_PROJECTILE_SERVER_STARTY_OFF = 0xd4ULL;
inline constexpr uint64_t LOGIC_PROJECTILE_SERVER_ENDX_OFF = 0xd8ULL;
inline constexpr uint64_t LOGIC_PROJECTILE_SERVER_ENDY_OFF = 0xdcULL;

inline constexpr size_t LOGIC_PROJECTILE_DATA_BLOCK_SIZE = 0x1e8ULL;

inline constexpr uint64_t LOGIC_TILE_MAP_WIDTH_OFF = 0xc4ULL;
inline constexpr uint64_t LOGIC_TILE_MAP_HEIGHT_OFF = 0xc8ULL;
inline constexpr uint64_t LOGIC_TILE_MAP_TILES_OFF = 0x20ULL;

inline constexpr uint64_t SC_STRING_LENGTH_OFF = 0x4ULL;
inline constexpr uint64_t SC_STRING_DATA_OFF = 0x8ULL;

inline constexpr uint64_t CLIENT_INPUT_MANAGER_QUEUE_OFF = 0x20ULL;
inline constexpr uint64_t CLIENT_INPUT_MESSAGE_TYPE_OFF = 0x08ULL;
inline constexpr uint64_t CLIENT_INPUT_MESSAGE_X_OFF = 0x0cULL;
inline constexpr uint64_t CLIENT_INPUT_MESSAGE_Y_OFF = 0x10ULL;
inline constexpr uint64_t CLIENT_INPUT_SKILL_DATA_OFF = 0x40ULL;
inline constexpr uint64_t CLIENT_INPUT_TOKEN_OFF = 0x34ULL;
inline constexpr uint64_t CLIENT_INPUT_TOKEN_INPUT_OFF = 0x4ULL;
inline constexpr uint64_t CLIENT_INPUT_TYPE_COUNT = 0x17ULL;

#endif
