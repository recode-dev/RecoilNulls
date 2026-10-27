#ifndef CORE_COMMANDS_H
#define CORE_COMMANDS_H
#include <cstdint>
inline constexpr int MSG_SIZE = 0x48;
inline constexpr int MSG_SLACK = 0x58;
inline constexpr int TYPE_MOVE = 0x2;
inline constexpr int PRED_FLAG = 1;
uintptr_t entry_callable(uintptr_t rva);
[[nodiscard]] void* msg_alloc();
void* manager();
int pred_set(int x, int y);
int move_to(int32_t x, int32_t y);
int ci_load_constants();
uint32_t ci_sign(void* ci, void* battle);
int enqueue(int x, int y);
int enqueue_type(int x, int y, int type);
int enqueue_skill(int x, int y, int type, void* skillData);
#endif
