#ifndef FEATURES_AUTODODGE_AUTODODGE_H
#define FEATURES_AUTODODGE_AUTODODGE_H
#include <cstdint>
inline constexpr int AD_REPORT_MAX = 3;
inline constexpr int AD_ENEMY_MAX = 4;
inline constexpr int AD_PROBE_MAX = 24;
extern int in_move;
extern int active;
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
extern float ad_pad, ad_pad_excess;
extern float ad_hit_ca[AD_REPORT_MAX];
extern float ad_hit_ttc[AD_REPORT_MAX];
extern float ad_hit_r[AD_REPORT_MAX];
extern float ad_threat_ca[AD_REPORT_MAX];
extern int ad_beam_n;
extern int ad_ind_n;
extern int ad_enemy_pid[AD_ENEMY_MAX];
extern float ad_enemy_x[AD_ENEMY_MAX];
extern float ad_enemy_y[AD_ENEMY_MAX];
extern int ad_enemy_hp[AD_ENEMY_MAX];
extern float ad_enemy_d[AD_ENEMY_MAX];
extern int ad_enemy_aim[AD_ENEMY_MAX];
void autododge_reset();
void autododge();
#endif
