#ifndef OFFSETS_H
#define OFFSETS_H

#include <cstdint>

inline constexpr uint64_t RVA_BATTLEMODE_GETINSTANCE = 0x8c5130ULL;
inline constexpr uint64_t RVA_LOGICBATTLEMODECLIENT_GETTILEMAP = 0x991440ULL;
inline constexpr uint64_t RVA_LOGICBATTLEMODECLIENT_GETOWNCHARACTER =
    0xac3934ULL;
inline constexpr uint64_t RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERINDEX =
    0x9cda04ULL;
inline constexpr uint64_t RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERTEAM =
    0xa7f47cULL;
inline constexpr uint64_t RVA_LOGICDATA_GETNAME = 0x9a4f1cULL;
inline constexpr uint64_t RVA_LOGICBATTLEMODECLIENT_SETCLIENTPREDICTIONMOVETO =
    0xac3a58ULL;
inline constexpr uint64_t RVA_CLIENTINPUTMESSAGE_CONSTRUCT = 0x00a95798ULL;
inline constexpr uint64_t RVA_CLIENTINPUTMANAGER_ADDINPUT = 0x74675cULL;
inline constexpr uint64_t RVA_LOGIC_TILEMAP_ISPASSABLEPATHFINDER = 0x942ebcULL;
// LogicTile keeps three 64-bit passability bitmaps. Bit index is
// (x % 3) + (y % 3) * 3 + layer * 9; a set bit means the sub-cell is blocked.
// Verified in v69: LogicTileMap::isPassablePathFinder (0x942ebc) calls
// LogicTile::isPassablePathFinder (0x9402ec), which reads
// +0x10/+0x18/+0x20/+0x28.
inline constexpr uint64_t LOGICTILE_BLOCKMASK_OFF = 0x10ULL;
inline constexpr uint64_t LOGICTILE_WATERMASK_OFF = 0x18ULL;
inline constexpr uint64_t LOGICTILE_THIRDMASK_OFF = 0x20ULL;
inline constexpr uint64_t LOGICTILE_FOURTHMASK_OFF = 0x28ULL;
inline constexpr uint64_t RVA_LOGIC_TILEMAP_ISPLAYERLINEOFSIGHTCLEAR =
    0x943b08ULL;
inline constexpr uint64_t RVA_LOGIC_TILE_TO_LOGIC = 0x943040ULL;
inline constexpr uint64_t RVA_LOGIC_LOGIC_TO_TILE = 0x943028ULL;

inline constexpr uint64_t GAMESTATE_RVA = 0x1123e58ULL;
inline constexpr uint64_t VTABLE_PROJECTILEDATA_RVA = 0x000ff3b18ULL;
inline constexpr uint64_t CLIENTINPUT_TYPE_TABLE_RVA = 0x00e1bd30ULL;
inline constexpr uint64_t CLIENTINPUT_TYPE_FALLBACK_RVA = 0x00e1bc28ULL;
inline constexpr uint64_t CLIENTINPUT_HASH_INNER_MASK_RVA = 0x00e1bbd0ULL;
inline constexpr uint64_t CLIENTINPUT_HASH_OUTER_MASK_RVA = 0x00e1bbf0ULL;
inline constexpr uint64_t DATACONST_RVA_LO = 0xf74000ULL;
inline constexpr uint64_t DATACONST_RVA_SIZE = 0xd4000ULL;

inline constexpr uint64_t GAMESTATE_STATE_OFF = 0x50ULL;
inline constexpr uint64_t GAMESTATE_SCENE_OFF = 0x48ULL;
inline constexpr uint64_t SCENE_CLIENT_OFF = 0x28ULL;
inline constexpr uint64_t LOGICBATTLEMODECLIENT_INPUTMANAGER_OFF = 0x58ULL;
inline constexpr uint64_t LOGICBATTLEMODECLIENT_OBJECTMANAGER_OFF = 0x28ULL;
inline constexpr uint64_t BATTLEMODE_HASH_ENABLED_OFF = 0x70ULL;
inline constexpr uint64_t BATTLEMODE_HASH_KEY_OFF = 0x60ULL;
inline constexpr uint64_t OBJECTMANAGER_ARRAY_OFF = 0x0ULL;
inline constexpr uint64_t OBJECTMANAGER_COUNT_OFF = 0xcULL;

inline constexpr uint64_t LOGICCHARACTER_GLOBALID_OFF = 0x8ULL;
inline constexpr uint64_t LOGICCHARACTER_X_OFF = 0x30ULL;
inline constexpr uint64_t LOGICCHARACTER_Y_OFF = 0x34ULL;
inline constexpr uint64_t LOGICCHARACTER_TEAM_OFF = 0x40ULL;
inline constexpr uint64_t LOGICCHARACTER_ATTACK_ANGLE_OFF = 0xb4ULL;
inline constexpr uint64_t LOGICCHARACTER_HP_OFF = 0xc4ULL;
inline constexpr uint64_t LOGICGAMEOBJECT_DATA_OFF = 0x10ULL;
inline constexpr uint64_t LOGICCHARACTERDATA_COLLISION_OFF = 0x2acULL;

inline constexpr uint64_t PROJSERVER_STARTTICK_OFF = 0x28ULL;
inline constexpr uint64_t PROJSERVER_ENDTICK_OFF = 0x2cULL;
inline constexpr uint64_t PROJSERVER_STARTX_OFF = 0xd0ULL;
inline constexpr uint64_t PROJSERVER_STARTY_OFF = 0xd4ULL;
inline constexpr uint64_t PROJSERVER_ENDX_OFF = 0xd8ULL;
inline constexpr uint64_t PROJSERVER_ENDY_OFF = 0xdcULL;

inline constexpr uint64_t PROJDATA_CFG_OFF = 0x10ULL;
inline constexpr size_t PROJDATA_BLOCK_SIZE = 0x1e8ULL;
inline constexpr size_t PROJDATA_CFG_BLOCK_SIZE = 0xc0ULL;
inline constexpr uint64_t PROJDATA_RADIUS_OFF = 0xa4ULL;
inline constexpr uint64_t PROJDATA_HOMING_OFF = 0x97ULL;
inline constexpr uint64_t PROJDATA_TTYPE_OFF = 0x170ULL;
inline constexpr uint64_t PROJECTILEDATA_RADIUS_OFF = 0x74ULL;
inline constexpr uint64_t PROJECTILEDATA_PASSES_ENV_OFF = 0x148ULL;
inline constexpr uint64_t PROJECTILEDATA_ISBOUNCING_OFF = 0xe9ULL;
inline constexpr uint64_t PROJECTILEDATA_PIERCES_ENV_OFF = 0xf5ULL;
inline constexpr uint64_t PROJECTILEDATA_PIERCES_ENV_BUTTER_OFF = 0xf6ULL;

inline constexpr uint64_t TILEMAP_WIDTH_OFF = 0xc4ULL;
inline constexpr uint64_t TILEMAP_HEIGHT_OFF = 0xc8ULL;
inline constexpr uint64_t TILEMAP_TILES_OFF = 0x20ULL;

inline constexpr uint64_t SCSTRING_LENGTH_OFF = 0x4ULL;
inline constexpr uint64_t SCSTRING_DATA_OFF = 0x8ULL;

inline constexpr uint64_t CLIENTINPUTMANAGER_QUEUE_OFF = 0x20ULL;
inline constexpr uint64_t CLIENTINPUTMESSAGE_TYPE_OFF = 0x08ULL;
inline constexpr uint64_t CLIENTINPUTMESSAGE_X_OFF = 0x0cULL;
inline constexpr uint64_t CLIENTINPUTMESSAGE_Y_OFF = 0x10ULL;
inline constexpr uint64_t CLIENTINPUT_SKILLDATA_OFF = 0x40ULL;
inline constexpr uint64_t CLIENTINPUT_TOKEN_OFF = 0x34ULL;
inline constexpr uint64_t CLIENTINPUT_TOKEN_INPUT_OFF = 0x4ULL;
inline constexpr uint64_t CLIENTINPUT_TYPE_COUNT = 0x17ULL;

inline constexpr uint64_t HEAP_MAX_REGION_SIZE = 0x100000000ULL;

#endif
