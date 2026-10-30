#ifndef CORE_COMMANDS_H
#define CORE_COMMANDS_H
#include <cstdint>
inline constexpr int ClientInputMessageTypeMove       = 0x2;
inline constexpr int ClientInputMessagePredictionFlag = 1;
uintptr_t            callable_at_rva(uintptr_t rva);
[[nodiscard]] void*  allocate_client_input_message();
int                  client_prediction_move_to(int x, int y);
int                  enqueue_client_input_message(int type, int x, int y);
int                  enqueue_move_input(int x, int y);
int                  enqueue_move_input_with_type(int x, int y, int type);
#endif
