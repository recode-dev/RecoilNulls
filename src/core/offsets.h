#ifndef OFFSETS_H
#define OFFSETS_H
#include <cstddef>
#include <cstdint>
inline constexpr uint64_t BATTLEMODE__GETINSTANCE                           = 0x8c5130;
inline constexpr uint64_t LOGICBATTLEMODE__CLIENT                           = 0x28;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER          = 0x28;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__INPUTMANAGER               = 0x58;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETTILEMAP                 = 0x991440;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETOWNCHARACTER            = 0xac3934;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO  = 0xac3a58;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__OWNTEAM                    = 0xe4;
inline constexpr uint64_t LOGICGAMEOBJECTMANAGER__GAMEOBJECTS               = 0x0;
inline constexpr uint64_t LOGICGAMEOBJECTMANAGER__GAMEOBJECTCOUNT           = 0xc;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETDATA                    = 0x10;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETGLOBALID                = 0x8;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETX                       = 0x30;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETY                       = 0x34;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__HP                         = 0xc4;
inline constexpr uint64_t LOGICDATA__TEAM                                   = 0x40;
inline constexpr uint64_t LOGICCHARACTERDATA__GETCOLLISIONRADIUS            = 0x9913e8;
inline constexpr uint64_t LOGICPROJECTILEDATA__VTABLE                       = 0xff3b18;
inline constexpr uint64_t LOGICPROJECTILEDATA__GETSPEED                     = 0x124;
inline constexpr uint64_t LOGICPROJECTILEDATA__GETRADIUS                    = 0x130;
inline constexpr uint64_t LOGICPROJECTILEDATA__PASSESENVIRONMENT            = 0x23c;
inline constexpr uint64_t LOGICPROJECTILEDATA__PIERCESENVIRONMENT           = 0x1e5;
inline constexpr uint64_t LOGICPROJECTILEDATA__PIERCESENVIRONMENTLIKEBUTTER = 0x1e6;
inline constexpr uint64_t LOGICPROJECTILECLIENT__TARGETX                    = 0x200;
inline constexpr uint64_t LOGICPROJECTILECLIENT__TARGETY                    = 0x204;
inline constexpr uint64_t LOGICTILEMAP__WIDTH                               = 0xc4;
inline constexpr uint64_t LOGICTILEMAP__HEIGHT                              = 0xc8;
inline constexpr uint64_t LOGICTILEMAP__TILETOLOGIC                         = 0x943040;
inline constexpr uint64_t LOGICTILEMAP__PATHFINDER                          = 0x60;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__LINESEGMENTINTERSECTSCIRCLE    = 0xaf53c4;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__GETDISTANCEBETWEEN             = 0xaf6838;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__GETCLOSESTPATHFINDERCOLLISION  = 0xaf5d10;
inline constexpr uint64_t LOGICVECTOR2__GETLENGTH                           = 0xbd0740;
inline constexpr uint64_t LOGICVECTOR2__NORMALIZE                           = 0xbd0794;
inline constexpr uint64_t LOGICMATH__GETANGLE                               = 0xbd021c;
inline constexpr uint64_t LOGICMATH__NORMALIZEANGLE180                      = 0xbcfe80;
inline constexpr uint64_t LOGICMATH__GETROTATEDX                            = 0x7b3a40;
inline constexpr uint64_t LOGICMATH__GETROTATEDY                            = 0x7b3a94;
inline constexpr uint64_t CLIENTINPUT__CONSTRUCTOR                          = 0xa95798;
inline constexpr uint64_t CLIENTINPUT__X                                    = 0xc;
inline constexpr uint64_t CLIENTINPUT__Y                                    = 0x10;
inline constexpr uint64_t CLIENTINPUT__SIZE                                 = 0x48;
inline constexpr uint64_t CLIENTINPUTMANAGER__ADDINPUT                      = 0x74675c;
inline constexpr uint64_t CLIENTINPUTMANAGER__QUEUE                         = 0x20;
#endif
