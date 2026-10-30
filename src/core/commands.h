#ifndef CORE_COMMANDS_H
#define CORE_COMMANDS_H
#include <stdint.h>
enum
{
    MSG_SIZE        = 0x48,
    MSG_SLACK       = 0x58,
    TYPE_MOVE       = 0x2,
    QUEUE_GUARD_MGR = 0,
    PRED_SET        = 1,
    PRED_FLAG       = 1,
    MOVE_ON         = 1
};
uintptr_t entry_callable(uintptr_t rva);
void*     msg_alloc();
void*     manager();
int       pred_set(int x, int y);
int       move_to(int32_t x, int32_t y, float ox, float oy);
int       ci_load_constants();
uint32_t  ci_sign(void* ci, void* battle);
int       enqueue(int x, int y);
int       enqueue_type(int x, int y, int type);
int       enqueue_skill(int x, int y, int type, void* skillData);
#endif
