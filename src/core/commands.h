#ifndef CORE_COMMANDS_H
#define CORE_COMMANDS_H
#include <stdint.h>
#define MSG_SIZE 0x48
#define MSG_SLACK 0x58
#define TYPE_MOVE 0x2
#define QUEUE_GUARD_MGR 0
#define PRED_SET 1
#define PRED_FLAG 1
#define MOVE_ON 1
uintptr_t entry_callable(uintptr_t rva);
void *msg_alloc(void);
void *g_manager(void);
int pred_set(int x, int y);
int move_to(int32_t x, int32_t y, float ox, float oy);
int ci_load_constants(void);
uint32_t ci_sign(void *ci, void *battle);
int enqueue(int x, int y);
int enqueue_type(int x, int y, int type);
int enqueue_skill(int x, int y, int type, void *skillData);
#endif
