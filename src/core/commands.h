#ifndef CORE_COMMANDS_H
#define CORE_COMMANDS_H

#include <cstddef>
#include <cstdint>

inline constexpr size_t CLIENT_INPUT_MESSAGE_ALLOCATION_SIZE = 0x48;
inline constexpr uintptr_t CLIENT_INPUT_MESSAGE_SEQUENCE_OFF = 0x00;
inline constexpr uintptr_t CLIENT_INPUT_MESSAGE_SIGNED_OFF = 0x04;
inline constexpr size_t CLIENT_INPUT_MESSAGE_SIGNED_SIZE = 0x10;
inline constexpr uintptr_t CLIENT_INPUT_MESSAGE_CHECKSUM_OFF = 0x34;
inline constexpr int CLIENT_INPUT_MESSAGE_TYPE_MOVE = 0x2;
inline constexpr int CLIENT_INPUT_MESSAGE_TYPE_LIMIT = 0x17;
inline constexpr int CLIENT_INPUT_MESSAGE_PREDICTION_FLAG = 1;

uintptr_t callable_at_rva(uintptr_t rva);
[[nodiscard]] void* allocate_client_input_message();
void* get_client_input_manager();
int set_client_prediction_move_to(int x, int y);
int load_client_input_constants();
uint32_t sign_client_input_message(void* ci, void* battle);
int enqueue_client_input_message(int type, int x, int y);
int enqueue_move_input(int x, int y);
int enqueue_move_input_with_type(int x, int y, int type);

#endif
