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

inline constexpr uint64_t PROJECTILEDATA_SPEED_OFF = 0x124ULL;
inline constexpr uint64_t PROJECTILEDATA_RADIUS_OFF = 0x74ULL;
inline constexpr uint64_t PROJECTILEDATA_SHOT_BY_HERO_OFF = 0x1d8ULL;
inline constexpr uint64_t PROJECTILEDATA_ISBEAM_OFF = 0x1d9ULL;
inline constexpr uint64_t PROJECTILEDATA_ISBOUNCING_OFF = 0x1daULL;
inline constexpr uint64_t PROJECTILEDATA_PIERCES_CHARS_OFF = 0x1e4ULL;
inline constexpr uint64_t PROJECTILEDATA_PIERCES_ENV_OFF = 0x1e5ULL;
inline constexpr uint64_t PROJECTILEDATA_PIERCES_ENV_BUTTER_OFF = 0x1e6ULL;
inline constexpr uint64_t PROJECTILEDATA_INDIRECT_OFF = 0x108ULL;
inline constexpr uint64_t RVA_LOGICDATA_GETNAME = 0x9a4f1cULL;
inline constexpr uint64_t SCSTRING_LENGTH_OFF = 0x4ULL;
inline constexpr uint64_t SCSTRING_DATA_OFF = 0x8ULL;
inline constexpr uint64_t MODE_MANAGER_OFF = 0x28ULL;
inline constexpr uint64_t MGR_ARRAY_OFF = 0x0ULL;
inline constexpr uint64_t MGR_COUNT_OFF = 0xcULL;
inline constexpr uint64_t DC_RVA_LO = 0xf74000ULL;
inline constexpr uint64_t OBJ_GLOBALID_OFF = 0x8ULL;
inline constexpr uint64_t MOVE_RVA = 0xac3a58ULL;
inline constexpr uint64_t ALLOC_RVA = 0x00d8da2cULL;
inline constexpr uint64_t MSGCTOR_RVA = 0x00a95798ULL;
inline constexpr uint64_t CI_MGR_QUEUE_OFF = 0x20ULL;
inline constexpr uint64_t ADDINPUT_RVA = 0x74675cULL;
inline constexpr uint64_t MGR_OFF = 0x58ULL;
inline constexpr uint64_t TYPE_OFF = 0x08ULL;
inline constexpr uint64_t X_OFF = 0x0cULL;
inline constexpr uint64_t Y_OFF = 0x10ULL;
inline constexpr uint64_t ALLOC_GOT_RVA = 0x00f78180ULL;
inline constexpr uint64_t CLIENT_OFF = 0x28ULL;
inline constexpr uint64_t OBJ_TEAM_OFF = 0x40ULL;
inline constexpr uint64_t OBJ_DEADFLAG_OFF = 0xd0ULL;
inline constexpr uint64_t OBJ_X_OFF = 0x30ULL;
inline constexpr uint64_t OBJ_Y_OFF = 0x34ULL;
inline constexpr uint64_t OBJ_HP_OFF = 0xc4ULL;
inline constexpr uint64_t STATE_RVA = 0x1123e58ULL;
inline constexpr uint64_t STATE_ENUM_OFF = 0x50ULL;
inline constexpr uint64_t SCENE_OFF = 0x48ULL;
inline constexpr uint64_t ELEM_DEF_OFF = 0x10ULL;
inline constexpr uint64_t TILEMAP_W_OFF = 0xc4ULL;
inline constexpr uint64_t TILEMAP_H_OFF = 0xc8ULL;
inline constexpr uint64_t TILEMAP_ARRAY_OFF = 0x20ULL;
inline constexpr uint64_t TILE_TYPE_MOVE_OFF = 0x56ULL;
inline constexpr uint64_t CI_TYPE_TABLE_RVA = 0x00e1bd30ULL;
inline constexpr uint64_t CI_TABLE_TYPES = 0x17ULL;
inline constexpr uint64_t CI_TYPE_FALLBACK_RVA = 0x00e1bc28ULL;
inline constexpr uint64_t CI_HASH_INNER_MASK_RVA = 0x00e1bbd0ULL;
inline constexpr uint64_t CI_HASH_OUTER_MASK_RVA = 0x00e1bbf0ULL;
inline constexpr uint64_t BM_HASH_ENABLED_OFF = 0x70ULL;
inline constexpr uint64_t BM_HASH_KEY_OFF = 0x60ULL;
inline constexpr uint64_t CI_TOKEN_OFF = 0x34ULL;
inline constexpr uint64_t CI_TOKEN_READ_OFF = 0x4ULL;
inline constexpr uint64_t VTABLE_PROJ_DATA_RVA = 0x000ff3b18ULL;
inline constexpr uint64_t DC_RVA_SIZE = 0xd4000ULL;
inline constexpr uint64_t HEAP_REGION_MAX_SIZE = 0x100000000ULL;
inline constexpr uint64_t OFF_CLIENTINPUT_SKILLDATA = 0x40ULL;
#endif
