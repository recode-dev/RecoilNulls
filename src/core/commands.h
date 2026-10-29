#ifndef CORE_COMMANDS_H
#define CORE_COMMANDS_H

#include <cstddef>
#include <cstdint>

#include "offsets.h"

inline constexpr size_t CLIENT_INPUT_MESSAGE_ALLOCATION_SIZE = 0x48;
inline constexpr size_t CLIENT_INPUT_MESSAGE_PROBE_SIZE = 0x80;
inline constexpr uintptr_t CLIENT_INPUT_MESSAGE_SEQUENCE_OFF = 0x00;
inline constexpr uintptr_t CLIENT_INPUT_MESSAGE_SIGNED_OFF = 0x04;
inline constexpr size_t CLIENT_INPUT_MESSAGE_SIGNED_SIZE = 0x10;
inline constexpr uintptr_t CLIENT_INPUT_MESSAGE_CHECKSUM_OFF = 0x34;
inline constexpr int CLIENT_INPUT_MESSAGE_TYPE_MOVE = 0x2;
inline constexpr int CLIENT_INPUT_MESSAGE_TYPE_LIMIT = 0x17;
inline constexpr int CLIENT_INPUT_MESSAGE_PREDICTION_FLAG = 1;

inline constexpr uintptr_t CLIENT_INPUT_TABLE_TO_FALLBACK_DISTANCE =
    CLIENT_INPUT_TYPE_TABLE_RVA - CLIENT_INPUT_TYPE_FALLBACK_RVA;
inline constexpr uintptr_t CLIENT_INPUT_TABLE_TO_INNER_MASK_DISTANCE =
    CLIENT_INPUT_TYPE_TABLE_RVA - CLIENT_INPUT_HASH_INNER_MASK_RVA;
inline constexpr uintptr_t CLIENT_INPUT_TABLE_TO_OUTER_MASK_DISTANCE =
    CLIENT_INPUT_TYPE_TABLE_RVA - CLIENT_INPUT_HASH_OUTER_MASK_RVA;

inline constexpr int CLIENT_INPUT_PIPELINE_UNKNOWN = 0;
inline constexpr int CLIENT_INPUT_PIPELINE_READY = 1;
inline constexpr int CLIENT_INPUT_PIPELINE_CALLABLES_MISSING = -1;
inline constexpr int CLIENT_INPUT_PIPELINE_LAYOUT_MISMATCH = -2;

static_assert(CLIENT_INPUT_MESSAGE_CHECKSUM_OFF + sizeof(uint32_t) <=
                  CLIENT_INPUT_MESSAGE_ALLOCATION_SIZE,
              "checksum leaves the allocated message");
static_assert(CLIENT_INPUT_MESSAGE_SIGNED_OFF +
                      CLIENT_INPUT_MESSAGE_SIGNED_SIZE <=
                  CLIENT_INPUT_MESSAGE_ALLOCATION_SIZE,
              "signed span leaves the allocated message");
static_assert(CLIENT_INPUT_MESSAGE_PROBE_SIZE >=
                  CLIENT_INPUT_MESSAGE_ALLOCATION_SIZE,
              "probe buffer is smaller than the message");

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
