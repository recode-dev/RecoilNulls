#ifndef OFFSETS_H
#define OFFSETS_H
#include <cstddef>
#include <cstdint>
inline constexpr uint64_t BATTLEMODE__GETINSTANCE                          = 0x8c5130;
inline constexpr uint64_t LOGICBATTLEMODE__CLIENT                          = 0x28;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER         = 0x28;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__INPUTMANAGER              = 0x58;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETTILEMAP                = 0x991440;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__GETOWNCHARACTER           = 0xac3934;
inline constexpr uint64_t LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO = 0xac3a58;
inline constexpr uint64_t LOGICGAMEOBJECTMANAGER__GAMEOBJECTS              = 0x0;
inline constexpr uint64_t LOGICGAMEOBJECTMANAGER__GAMEOBJECTCOUNT          = 0xc;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETDATA                   = 0x10;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETGLOBALID               = 0x8;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETX                      = 0x30;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__GETY                      = 0x34;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__HP                        = 0xc4;
inline constexpr uint64_t LOGICGAMEOBJECTCLIENT__TEAM                      = 0x40;
inline constexpr uint64_t LOGICCHARACTERDATA__GETCOLLISIONRADIUS           = 0x9913e8;
inline constexpr uint64_t LOGICPROJECTILEDATA__GETSPEED                    = 0x124;
inline constexpr uint64_t LOGICPROJECTILEDATA__GETRADIUS                   = 0x130;
inline constexpr uint64_t LOGICDATA__ISPROJECTILE                          = 0x48;
inline constexpr uint64_t LOGICTILEMAP__PATHFINDER                         = 0x60;
inline constexpr uint64_t LOGICTILEMAP__WIDTH                              = 0xc4;
inline constexpr uint64_t LOGICTILEMAP__HEIGHT                             = 0xc8;
inline constexpr uint64_t LOGICPATHFINDER__TILEMAP                         = 0x10;
inline constexpr uint64_t LOGICPATHFINDER__ISCLEARPATH                     = 0xafb06c;
inline constexpr uint64_t CLIENTINPUTMESSAGE__CONSTRUCTOR                  = 0xa95798;
inline constexpr uint64_t CLIENTINPUTMESSAGE__TYPE                         = 0x8;
inline constexpr uint64_t CLIENTINPUTMESSAGE__X                            = 0xc;
inline constexpr uint64_t CLIENTINPUTMESSAGE__Y                            = 0x10;
inline constexpr uint64_t CLIENTINPUTMESSAGE__SIZE                         = 0x48;
inline constexpr uint64_t CLIENTINPUTMANAGER__ADDINPUT                     = 0x74675c;
inline constexpr uint64_t CLIENTINPUTMANAGER__QUEUE                        = 0x20;
inline constexpr uint64_t LOGICTILEMAP__ISPASSABLEPATHFINDER               = 0x942ebc;
inline constexpr uint64_t LOGICTILEMAP__TILETOLOGIC                        = 0x943040;
inline constexpr uint64_t LOGICTILEMAP__ISPLAYERLINEOFSIGHTCLEAR           = 0x943b34;
inline constexpr uint64_t LOGICGAMEPLAYUTIL__LINESEGMENTINTERSECTSCIRCLE   = 0xaf53c4;
inline constexpr uint64_t LOGICPROJECTILECLIENT__GETTRAVELTICKS            = 0xa3192c;
inline constexpr uint64_t LOGICCHARACTERDATA__GETWEAPONSKILL               = 0x9df18c;
inline constexpr uint64_t LOGICCHARACTERDATA__GETULTISKILL                 = 0x9df194;
inline constexpr uint64_t LOGICSKILLDATA__GETCASTINGRANGETILES             = 0x9de9cc;
// LogicSkillData::getProjectile() = single LogicProjectileData* at +0x58.
// Proven: v36 ref 0x490584 `return *(this+0x58)` (decompiled, symbolized);
// v69 twin 0x9b4e14 `ldr x0,[x0,#0x58]; ret` byte-identical, score 1.000.
// NOT an array: v69 0x9debb8 is another class' indexed accessor; there is no
// LogicSkillData::getProjectile(int) and no count field. Old 0x58-array +
// 0x64-count reading was dead logic.
inline constexpr uint64_t LOGICSKILLDATA__PROJECTILE = 0x58;
#endif
