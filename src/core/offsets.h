#ifndef OFFSETS_H
#define OFFSETS_H

#include <cstddef>
#include <cstdint>

inline constexpr uint64_t GAMESTATEMANAGER__INSTANCE     = 0x1123e58ULL;
inline constexpr uint64_t GAMESTATEMANAGER__CURRENTSTATE = 0x48ULL;
inline constexpr uint64_t GAMESTATEMANAGER__STATE        = 0x50ULL;
inline constexpr uint64_t GAMESTATEMANAGER__CLIENT       = 0x28ULL;

inline constexpr uint64_t LOGICBATTLEMODECLIENT__INPUTMANAGER      = 0x58ULL;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER = 0x28ULL;

inline constexpr uint64_t LOGICGAMEOBJECTMANAGER__GAMEOBJECTS     = 0x0ULL;
inline constexpr uint64_t LOGICGAMEOBJECTMANAGER__GAMEOBJECTCOUNT = 0xcULL;

inline constexpr uint64_t LOGICGAMEOBJECT__DATA               = 0x10ULL;
inline constexpr uint64_t LOGICGAMEOBJECT__DATASCANSPAN       = 0x200;
inline constexpr uint64_t LOGICCHARACTER__GLOBALID            = 0x8ULL;
inline constexpr uint64_t LOGICCHARACTER__X                   = 0x30ULL;
inline constexpr uint64_t LOGICCHARACTER__Y                   = 0x34ULL;
// team lives on the data object, not on the character: GameObject::getTeam()
// is `return *(int*)(*(void**)(this + 0x10) + 0x40)` in both builds.
inline constexpr uint64_t LOGICGAMEOBJECTDATA__TEAM           = 0x40ULL;
inline constexpr uint64_t LOGICCHARACTER__ATTACKANGLE         = 0xb4ULL;
inline constexpr uint64_t LOGICCHARACTER__HITPOINTS           = 0xc4ULL;
inline constexpr uint64_t LOGICCHARACTER__DATACOLLISIONRADIUS = 0x2acULL;
inline constexpr uint64_t LOGICCHARACTER__CURRENTCHARGEUPMILLISECONDS =
    0x30cULL;
inline constexpr uint64_t LOGICCHARACTER__CHARGEUPMODE = 0x310ULL;

inline constexpr uint64_t LOGICPROJECTILEDATA__SPEED = 0x124ULL;

inline constexpr uint64_t  LOGICTILE__DATA                        = 0x0ULL;
inline constexpr uint64_t  LOGICTILE__PATHFINDERMASK              = 0x10ULL;
inline constexpr uint64_t  LOGICTILE__PATHFINDERMASKSIZE          = 0x8;
inline constexpr uintptr_t LOGICTILE__PATHFINDERMASKCANDIDATES[3] = {
    LOGICTILE__PATHFINDERMASK,
    LOGICTILE__PATHFINDERMASK - LOGICTILE__PATHFINDERMASKSIZE,
    LOGICTILE__PATHFINDERMASK + LOGICTILE__PATHFINDERMASKSIZE};
inline constexpr uint64_t LOGICTILE__PATHFINDERMASKREADWINDOWSIZE =
    LOGICTILE__PATHFINDERMASK + 6 * LOGICTILE__PATHFINDERMASKSIZE;
inline constexpr uintptr_t LOGICTILE__PATHFINDERMASKREADWINDOWS[5] = {
    LOGICTILE__PATHFINDERMASK + 6 * LOGICTILE__PATHFINDERMASKSIZE,
    LOGICTILE__PATHFINDERMASK + 5 * LOGICTILE__PATHFINDERMASKSIZE,
    LOGICTILE__PATHFINDERMASK + 4 * LOGICTILE__PATHFINDERMASKSIZE,
    LOGICTILE__PATHFINDERMASK + 3 * LOGICTILE__PATHFINDERMASKSIZE,
    LOGICTILE__PATHFINDERMASK + 2 * LOGICTILE__PATHFINDERMASKSIZE};
inline constexpr uint64_t LOGICTILEDATA__BLOCKSMOVEMENT      = 0x56ULL;
inline constexpr uint64_t LOGICTILEDATA__BLOCKFLAGSSIZE      = 2;
inline constexpr unsigned LOGICTILEDATA__PROJECTILEFLAGSHIFT = 8;

inline constexpr uint64_t LOGICTILEMAP__TILEARRAY = 0x20ULL;
inline constexpr uint64_t LOGICTILEMAP__WIDTH     = 0xc4ULL;
inline constexpr uint64_t LOGICTILEMAP__HEIGHT    = 0xc8ULL;

inline constexpr uint64_t SCSTRING__BYTELENGTH = 0x4ULL;
inline constexpr uint64_t SCSTRING__CHARACTERS = 0x8ULL;

inline constexpr uint64_t CLIENTINPUTMANAGER__INPUTQUEUE = 0x20ULL;
inline constexpr uint64_t CLIENTINPUTMESSAGE__TYPE       = 0x8ULL;
inline constexpr uint64_t CLIENTINPUTMESSAGE__X          = 0xcULL;
inline constexpr uint64_t CLIENTINPUTMESSAGE__Y          = 0x10ULL;
inline constexpr uint64_t CLIENTINPUTMESSAGE__SIZE       = 0x48;
inline constexpr uint64_t CLIENTINPUTMESSAGE__PROBESIZE  = 0x80;
inline constexpr uint64_t CLIENTINPUT__TYPECOUNT         = 0x17;

inline constexpr uint64_t BATTLEMODE__GETINSTANCE                 = 0x8c5130ULL;
inline constexpr uint64_t LOGICDATA__GETNAME                      = 0x9a4f1cULL;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETTILEMAP       = 0x991440ULL;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETOWNCHARACTER  = 0xac3934ULL;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETOWNPLAYERTEAM = 0xa7f47cULL;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO =
    0xac3a58ULL;
inline constexpr uint64_t CLIENTINPUTMESSAGE__CONSTRUCTOR       = 0xa95798ULL;
inline constexpr uint64_t CLIENTINPUTMANAGER__ADDINPUT          = 0x74675cULL;
inline constexpr uint64_t LOGICTILEMAP__ISPASSABLEPATHFINDER    = 0x942ebcULL;
inline constexpr uint64_t LOGICTILEMAP__TILETOLOGIC             = 0x943040ULL;
inline constexpr uint64_t LOGICTILEMAP__LOGICTOTILE             = 0x943028ULL;
inline constexpr uint64_t LOGICPROJECTILEDATA__VTABLE           = 0xff3b18ULL;
inline constexpr uint64_t LOGICMATH__SQRT                       = 0xbd000cULL;
inline constexpr uint64_t LOGICMATH__SIN                        = 0xbcfec0ULL;
inline constexpr uint64_t LOGICMATH__GETDISTANCESQUARETOLINE    = 0xbd0424ULL;
inline constexpr uint64_t LOGICPROJECTILEDATA__GETRADIUS        = 0xa2d7f8ULL;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETX           = 0xa23afcULL;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETY           = 0xa23b04ULL;
inline constexpr uint64_t LOGICVECTOR2__GETANGLE                = 0xbd0824ULL;
inline constexpr uint64_t LOGICVECTOR2__NORMALIZE               = 0xbd0794ULL;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__GETDISTANCEBETWEEN = 0xaf6838ULL;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__GETDISTANCESQUAREDBETWEEN =
    0xaf7198ULL;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__LINESEGMENTINTERSECTSCIRCLE =
    0xaf53c4ULL;
inline constexpr uint64_t LOGICPROJECTILECLIENT__GETTRAVELTICKS = 0xa3192cULL;

#endif
