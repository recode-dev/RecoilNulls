#ifndef FEATURES_AUTODODGE_AUTODODGE_H
#define FEATURES_AUTODODGE_AUTODODGE_H
#include <cstdint>
inline constexpr int AD_REPORT_MAX = 3;
inline constexpr int AD_ENEMY_MAX = 4;
extern int in_move;
extern int active;
extern int ad_human;
extern int ad_pass_own;
extern float ad_dir_x, ad_dir_y;
extern int ad_hold_ms, ad_urgent;
extern int32_t ad_target_x;
extern int32_t ad_target_y;
extern int ad_best_dir;
extern int ad_char_n;
extern int ad_proj_n;
extern int ad_enemy_n;
extern int ad_threat_n;
extern int ad_threat_pid[AD_REPORT_MAX];
extern float ad_threat_x[AD_REPORT_MAX];
extern float ad_threat_y[AD_REPORT_MAX];
extern float ad_threat_vx[AD_REPORT_MAX];
extern float ad_threat_vy[AD_REPORT_MAX];
extern float ad_threat_r[AD_REPORT_MAX];
extern float ad_threat_d[AD_REPORT_MAX];
extern float ad_threat_ttc[AD_REPORT_MAX];
extern char ad_threat_name[AD_REPORT_MAX][40];
extern int ad_hit_dmg, ad_hit_act, ad_hit_n, ad_hit_x, ad_hit_y;
extern int ad_hit_picked;
extern float ad_hit_ca[AD_REPORT_MAX];
extern float ad_hit_ttc[AD_REPORT_MAX];
extern float ad_hit_r[AD_REPORT_MAX];
extern float ad_threat_ca[AD_REPORT_MAX];
extern int ad_ind_n;
extern uint64_t ad_qv[16];
extern uint16_t ad_qc[16];
extern uint32_t ad_proj_sample, ad_proj_move;
extern int ad_enemy_pid[AD_ENEMY_MAX];
extern float ad_enemy_x[AD_ENEMY_MAX];
extern float ad_enemy_y[AD_ENEMY_MAX];
extern int ad_enemy_hp[AD_ENEMY_MAX];
extern float ad_enemy_d[AD_ENEMY_MAX];
extern int ad_enemy_aim[AD_ENEMY_MAX];
void autododge_reset();
void autododge();
namespace ad
{
extern float own_hitbox;
constexpr int THREAT_MAX = 32;
constexpr int PROJ_SLOTS = 48;
// server logic tick = 20 Hz / 50 ms.
// v36 proof: TimeUtil::getTicksAsSecondsString(0x28db78) converts ticks with
// (ticks % 20 == 0 ? ticks / 20 : ticks * 0.05); LogicCharacterServer::
// MAX_HOLD_TICKS * 0.05 used as seconds. 1 tick = 0.05 s.
constexpr uint64_t TICK_MS = 50;
static_assert(TICK_MS == 50, "logic tick is 20 Hz (50 ms) - see v36 0x28db78");
constexpr uint64_t PROJ_TTL_MS = TICK_MS * 20;
constexpr uint8_t B_IND = 2, B_PIERCE = 4, B_BOUNCE = 8;
struct Threat
{
    float x, y, vx, vy, rad, tt;
    int32_t pid;
    char name[40];
};
struct Proj
{
    int32_t pid;
    uint64_t seen;
    float x, y, ux, uy, speed, rad;
    uint8_t flags;
    bool vel;
    bool meas;
    bool homing;
    int16_t ttype;
    char name[40];
};
extern Threat threats[THREAT_MAX];
extern int threat_n;
extern Proj projs[PROJ_SLOTS];
extern uintptr_t char_vt;
extern float self_speed;
extern int pend_dmg;
void evict_stale(uint64_t now);
void collect_world(uint64_t now, int32_t own_gid, float mx, float my);
float clear_one(float mx, float my, float dx, float dy, const Threat& t);
void hit_report(float mx, float my, float dx, float dy);
void fill_report(float mx, float my, float dx, float dy);
}
#endif
