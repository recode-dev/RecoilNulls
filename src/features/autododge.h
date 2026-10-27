#ifndef FEATURES_AUTODODGE_AUTODODGE_H
#define FEATURES_AUTODODGE_AUTODODGE_H
#include <stdint.h>
extern __thread int in_move;
extern int active;
extern int moving;
void autododge(void);
#endif
