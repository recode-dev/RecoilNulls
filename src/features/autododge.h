#ifndef FEATURES_AUTODODGE_AUTODODGE_H
#define FEATURES_AUTODODGE_AUTODODGE_H
#include <stdint.h>
extern __thread int in_move;
extern int32_t pl_mine[12];
extern int g_active;
extern int g_moving;
extern int cand_now[3];
extern int cand_seen;
extern int32_t prev_x;
extern int32_t prev_y;
void g_autododge(void);
void autododge_reset(void);
#endif
