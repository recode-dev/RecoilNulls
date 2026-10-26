#ifndef RECOIL_FEATURES_AUTODODGE_AUTODODGE_H
#define RECOIL_FEATURES_AUTODODGE_AUTODODGE_H
#include <stdint.h>
extern __thread int rcl_in_move;
extern int32_t rcl_pl_mine[12];
extern int rcl_active;
extern int rcl_moving;
extern int rcl_cand_now[3];
extern int rcl_cand_seen;
extern int32_t rcl_prev_x;
extern int32_t rcl_prev_y;
void rcl_autododge(void);
void rcl_autododge_reset(void);
#endif
