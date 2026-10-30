#ifndef OFFSETS_H
#define OFFSETS_H

#include <cstddef>
#include <cstdint>

inline constexpr uint64_t GAMESTATEMANAGER__INSTANCE     = 0x1123e58;
inline constexpr uint64_t GAMESTATEMANAGER__CURRENTSTATE = 0x48;
inline constexpr uint64_t GAMESTATEMANAGER__STATE        = 0x50;
inline constexpr uint64_t GAMESTATEMANAGER__CLIENT       = 0x28;

inline constexpr uint64_t LOGICBATTLEMODECLIENT__INPUTMANAGER      = 0x58;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER = 0x28;

inline constexpr uint64_t LOGICGAMEOBJECTMANAGER__GAMEOBJECTS     = 0x0;
inline constexpr uint64_t LOGICGAMEOBJECTMANAGER__GAMEOBJECTCOUNT = 0xc;

inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETDATA     = 0x10;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETGLOBALID = 0x8;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETX        = 0x30;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETY        = 0x34;

inline constexpr uint64_t LOGICCHARACTERDATA__GETCOLLISIONRADIUS = 0x9913e8;

inline constexpr uint64_t LOGICPROJECTILEDATA__GETSPEED      = 0x124;
inline constexpr uint64_t LOGICPROJECTILEDATA__GETRADIUS     = 0x130;
inline constexpr uint64_t LOGICDATA__ISPROJECTILE_VTABLESLOT = 0x48;

inline constexpr uint64_t LOGICTILEMAP__TILEARRAY  = 0x20;
inline constexpr uint64_t LOGICTILEMAP__PATHFINDER = 0x60;
inline constexpr uint64_t LOGICTILEMAP__WIDTH      = 0xc4;
inline constexpr uint64_t LOGICTILEMAP__HEIGHT     = 0xc8;

inline constexpr uint64_t LOGICPATHFINDER__TILEMAP     = 0x10;
inline constexpr uint64_t LOGICPATHFINDER__ISCLEARPATH = 0xafb06c;

inline constexpr uint64_t BATTLEMODE__GETINSTANCE                 = 0x8c5130;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETTILEMAP       = 0x991440;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETOWNCHARACTER  = 0xac3934;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETOWNPLAYERTEAM = 0xa7f47c;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO =
    0xac3a58;
inline constexpr uint64_t CLIENTINPUTMESSAGE__CONSTRUCTOR        = 0xa95798;
inline constexpr uint64_t CLIENTINPUTMANAGER__ADDINPUT           = 0x74675c;
inline constexpr uint64_t LOGICTILEMAP__ISPASSABLEPATHFINDER     = 0x942ebc;
inline constexpr uint64_t LOGICTILEMAP__TILETOLOGIC              = 0x943040;
inline constexpr uint64_t LOGICTILEMAP__ISPLAYERLINEOFSIGHTCLEAR = 0x943b34;
inline constexpr uint64_t LOGICVECTOR2__NORMALIZE                = 0xbd0794;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__GETDISTANCEBETWEEN  = 0xaf6838;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__LINESEGMENTINTERSECTSCIRCLE =
    0xaf53c4;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__GETDISTANCESQUAREDBETWEEN =
    0xaf7198;
inline constexpr uint64_t LOGICPROJECTILECLIENT__GETTRAVELTICKS = 0xa3192c;

#endif
