#ifndef OFFSETS_H
#define OFFSETS_H

#include <stdint.h>

/**
 * Recoil Project Offsets
 * Optimized for direct C++ memory access.
 */

// --- Base Pointers / Instance Offsets ---
#define OFF_BATTLEMODE_SCREEN           0x8ULL
#define OFF_BATTLEMODE_MGR              0x28ULL
#define OFF_BATTLEMODE_CLIENTINPUT      0x58ULL
#define OFF_BATTLEMODE_HASHKEY          0x60ULL
#define OFF_BATTLEMODE_HASHENABLED      0x70ULL

// --- Manager Offsets ---
#define OFF_OBJMGR_ARRAY                0x0ULL
#define OFF_OBJMGR_COUNT                0xcULL
#define OFF_OBJMGR_STRIDE               0x8ULL

// --- TileMap & Navigation ---
#define OFF_TILEMAP_WIDTH               0xc4ULL
#define OFF_TILEMAP_HEIGHT              0xc8ULL
#define OFF_TILEMAP_ARRAY               0x20ULL
#define OFF_TILETYPE_BLOCKS_MOVE        0x56ULL

// --- GameObject Basics ---
#define OFF_OBJ_TEAM                    0x40ULL
#define OFF_OBJ_HP                      0xc4ULL
#define OFF_OBJ_OWNER_IDX               0x3cULL
#define OFF_OBJ_X                       0x30ULL
#define OFF_OBJ_Y                       0x34ULL

// --- Character Data ---
#define OFF_CHAR_BODY                   0xa0ULL
#define OFF_CHARDATA_BRAWLER_ID         0x35cULL
#define OFF_HERODATA_NAME               0x18ULL

// --- Projectile System ---
#define OFF_PROJ_TEAM                   0x40ULL
#define OFF_PROJ_DATANAME               0x18ULL
#define OFF_PROJ_INDIRECT               0x118ULL
#define OFF_PROJDATA_SPEED              0x6cULL
#define OFF_PROJDATA_RADIUS             0x74ULL

// --- ClientInput / Signature Constants ---
#define OFF_CLIENTINPUT_X               0xcULL
#define OFF_CLIENTINPUT_Y               0x10ULL
#define RVA_CI_TYPE_TABLE               0xe1bd30ULL
#define RVA_CI_TYPE_FALLBACK            0xe1bc28ULL
#define RVA_CI_HASH_INNER               0xe1bbd0ULL
#define RVA_CI_HASH_OUTER               0xe1bbf0ULL

// --- Strings ---
#define OFF_SCSTRING_LEN                0x4ULL
#define OFF_SCSTRING_DATA               0x8ULL

// --- Function RVAs (Direct Calling) ---
#define RVA_BATTLEMODE_GETINSTANCE      0x8c5130ULL
#define RVA_LOGICBATTLEMODECLIENT_UPDATE 0xac1ee8ULL
#define RVA_CLIENTINPUTMGR_ADDINPUT      0x74675cULL
#define RVA_CLIENTINPUT_CTOR             0xa95798ULL
#define RVA_MALLOC_NEW                   0xd8da2cULL
#define RVA_LOGICBATTLEMODE_SETMOVEMENT  0xac3a58ULL

// --- Getters & Logic ---
#define RVA_LOGICBATTLEMODE_GETOWNCHAR   0xac3934ULL
#define RVA_LOGICBATTLEMODE_GETOWNTEAM   0xa7f47cULL
#define RVA_LOGICBATTLEMODE_GETOWNIDX    0x9cda04ULL
#define RVA_LOGICBATTLEMODE_GETTILEMAP   0x991440ULL
#define RVA_GAMEOBJ_GETX                 0xa23afcULL
#define RVA_GAMEOBJ_GETY                 0xa23b04ULL
#define RVA_GAMEOBJ_GETDATA              0x117050ULL
#define RVA_GAMEOBJ_GETGID               0x46464cULL

// --- Character / Skill Logic ---
#define RVA_CHARDATA_GETSPEED            0x9911ecULL
#define RVA_CHARDATA_GETRADIUS           0x9913e8ULL
#define RVA_CHAR_GETWEAPONSKILL          0x9fc440ULL
#define RVA_CHAR_GETSKILLAT              0xa007fcULL
#define RVA_SKILL_GETDATA                0x1e5650ULL
#define RVA_SKILLDATA_GETRANGE           0x9de9ccULL
#define RVA_SKILLDATA_GETPROJDATA        0x9e84a8ULL

// --- Projectile Logic ---
#define RVA_PROJDATA_GETSPEED            0x8cee94ULL
#define RVA_PROJDATA_GETRADIUS           0x860d88ULL
#define RVA_PROJDATA_GETTICKS            0x979d50ULL
#define RVA_PROJDATA_ISBEAM              0x9b6448ULL
#define RVA_VTABLE_PROJ_DATA             0xff3b18ULL

// --- Misc ---
#define RVA_LOGICDATA_GETNAME            0x9a4f1cULL

#endif
