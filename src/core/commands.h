#ifndef CORE_COMMANDS_H
#define CORE_COMMANDS_H

#include <cstdint>

inline constexpr int CLIENT_INPUT_MESSAGE_TYPE_MOVE = 0x2;
inline constexpr int CLIENT_INPUT_MESSAGE_PREDICTION_FLAG = 1;

inline constexpr int CLIENT_INPUT_PIPELINE_UNKNOWN = 0;
inline constexpr int CLIENT_INPUT_PIPELINE_READY = 1;
inline constexpr int CLIENT_INPUT_PIPELINE_CALLABLES_MISSING = -1;
inline constexpr int CLIENT_INPUT_PIPELINE_LAYOUT_MISMATCH = -2;

uintptr_t callable_at_rva(uintptr_t rva);
[[nodiscard]] void* allocate_client_input_message();
void* get_client_input_manager();
int set_client_prediction_move_to(int x, int y);
int load_client_input_constants();
int client_input_pipeline_state();
uint32_t sign_client_input_message(void* ci, void* battle);
int enqueue_client_input_message(int type, int x, int y);
int enqueue_move_input(int x, int y);
int enqueue_move_input_with_type(int x, int y, int type);

#endif
