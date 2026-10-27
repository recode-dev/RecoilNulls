#ifndef FEATURES_AUTODODGE_AUTODODGE_H
#define FEATURES_AUTODODGE_AUTODODGE_H
#include <stdint.h>
extern __thread int in_move;
extern int active;
extern int moving;
extern int32_t ad_target_x;
extern int32_t ad_target_y;
void autododge_reset(void);
void autododge(void);
#endif
