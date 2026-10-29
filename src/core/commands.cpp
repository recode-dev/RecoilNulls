#include "commands.h"

#include <cstdlib>
#include <cstring>

#include "../utils/crypto.h"
#include "offsets.h"
#include "scan.h"

static constexpr uintptr_t ClientInputTypeTableScanSpan = 0x20000ULL;
static constexpr size_t ClientInputTypeTableScanChunk = 0x4000;
static const uint8_t client_input_type_table_anchor[8] = {
    0xd2, 0xc7, 0xb1, 0xa3, 0xb8, 0xa6, 0xf5, 0xe4};

static uint32_t client_input_type_table[ClientInputTypeCount];
static uint32_t client_input_type_fallback = 0;
static uint8_t client_input_hash_inner_mask[0x10];
static uint8_t client_input_hash_outer_mask[0x10];
static int client_input_constants_ready = 0;
static int client_input_layout_confirmed = 0;
static int client_input_pipeline_state_value = ClientInputPipelineUnknown;

static int client_input_message_is_valid(void* memory, size_t size);
static void* client_input_battle_instance();
static void* client_input_manager_of_battle(void* battle);
static int client_input_manager_queue_is_ready(void* manager);
static int client_input_layout_probe(uintptr_t constructor);

uintptr_t callable_at_rva(uintptr_t rva)
{
    if (!base || !rva)
    {
        return 0;
    }
    if (!callable(rva))
    {
        return 0;
    }
    return base + rva;
}

static int client_input_function_is_callable(uintptr_t address)
{
    if (!address)
    {
        return 0;
    }
    if (!addr_readable(address, sizeof(uint32_t)))
    {
        return 0;
    }
    return addr_executable(address) ? 1 : 0;
}

static uintptr_t client_input_resolve(uintptr_t* slot, uintptr_t rva)
{
    if (!*slot)
    {
        uintptr_t resolved = callable_at_rva(rva);
        if (client_input_function_is_callable(resolved))
        {
            *slot = resolved;
        }
    }
    return *slot;
}

static int client_input_type_table_is_plausible(const uint32_t* table)
{
    int count = static_cast<int>(ClientInputTypeCount);
    int nonzero = 0;
    int unique = 0;
    for (int i = 0; i < count; i++)
    {
        int repeated = 0;
        if (!table[i])
        {
            continue;
        }
        nonzero++;
        for (int j = 0; j < i; j++)
        {
            if (table[j] == table[i])
            {
                repeated = 1;
                break;
            }
        }
        if (!repeated)
        {
            unique++;
        }
    }
    if (nonzero * 4 < count * 3)
    {
        return 0;
    }
    return unique + 1 >= nonzero;
}

static int client_input_constants_at(uintptr_t tableRva)
{
    uint32_t table[ClientInputTypeCount];
    uint32_t fallback = 0;
    uint8_t inner[0x10];
    uint8_t outer[0x10];
    if (!base || !tableRva)
    {
        return 0;
    }
    if (tableRva <= ClientInputTypeTableToInnerMaskDistance)
    {
        return 0;
    }
    if (!read_bytes(base + tableRva, table, sizeof(table)))
    {
        return 0;
    }
    if (!client_input_type_table_is_plausible(table))
    {
        return 0;
    }
    if (!read_bytes(base + tableRva - ClientInputTypeTableToFallbackDistance,
                    &fallback,
                    sizeof(fallback)))
    {
        return 0;
    }
    if (!read_bytes(base + tableRva - ClientInputTypeTableToInnerMaskDistance,
                    inner,
                    sizeof(inner)))
    {
        return 0;
    }
    if (!read_bytes(base + tableRva - ClientInputTypeTableToOuterMaskDistance,
                    outer,
                    sizeof(outer)))
    {
        return 0;
    }
    memcpy(client_input_type_table, table, sizeof(table));
    client_input_type_fallback = fallback;
    memcpy(client_input_hash_inner_mask, inner, sizeof(inner));
    memcpy(client_input_hash_outer_mask, outer, sizeof(outer));
    return 1;
}

static int client_input_constants_relocate(uintptr_t* outRva)
{
    uint8_t buffer[ClientInputTypeTableScanChunk +
                   sizeof(client_input_type_table_anchor)];
    uintptr_t start =
        ClientInputTypeTableRva - ClientInputTypeTableScanSpan;
    uintptr_t end = ClientInputTypeTableRva + ClientInputTypeTableScanSpan +
                    sizeof(client_input_type_table_anchor);
    uintptr_t found = 0;
    int hits = 0;
    if (!base || !outRva)
    {
        return 0;
    }
    for (uintptr_t at = start; at < end; at += ClientInputTypeTableScanChunk)
    {
        size_t span = ClientInputTypeTableScanChunk +
                      sizeof(client_input_type_table_anchor);
        size_t limit = ClientInputTypeTableScanChunk;
        if (at + span > end)
        {
            span = static_cast<size_t>(end - at);
        }
        if (span < sizeof(client_input_type_table_anchor))
        {
            break;
        }
        if (limit + sizeof(client_input_type_table_anchor) > span)
        {
            limit = span - sizeof(client_input_type_table_anchor) + 1;
        }
        if (!read_bytes(base + at, buffer, span))
        {
            continue;
        }
        for (size_t i = 0; i < limit; i++)
        {
            if (memcmp(buffer + i,
                       client_input_type_table_anchor,
                       sizeof(client_input_type_table_anchor)) != 0)
            {
                continue;
            }
            hits++;
            if (hits > 1)
            {
                return 0;
            }
            found = at + i;
        }
    }
    if (hits != 1)
    {
        return 0;
    }
    *outRva = found;
    return 1;
}

int load_client_input_constants()
{
    uintptr_t relocated = 0;
    if (client_input_constants_ready)
    {
        return 1;
    }
    if (!base)
    {
        return 0;
    }
    if (client_input_constants_at(ClientInputTypeTableRva))
    {
        client_input_constants_ready = 1;
        return 1;
    }
    if (!client_input_constants_relocate(&relocated))
    {
        return 0;
    }
    if (relocated == ClientInputTypeTableRva)
    {
        return 0;
    }
    if (!client_input_constants_at(relocated))
    {
        return 0;
    }
    client_input_constants_ready = 1;
    return 1;
}

int client_input_pipeline_state()
{
    return client_input_pipeline_state_value;
}

uint32_t sign_client_input_message(void* ci, void* battle)
{
    uint8_t key[0x10];
    uint8_t enabled = 0;
    uint32_t checksum = 0;
    uintptr_t message = reinterpret_cast<uintptr_t>(ci);
    uintptr_t mode = reinterpret_cast<uintptr_t>(battle);
    if (!ci || !battle)
    {
        return 0;
    }
    if (!read_bytes(mode + LogicBattleModeClientHashEnabledOffset, &enabled, 1))
    {
        return 0;
    }
    if (enabled != 1)
    {
        return 0;
    }
    if (!load_client_input_constants())
    {
        return 0;
    }
    if (!read_bytes(
            mode + LogicBattleModeClientHashKeyOffset, key, sizeof(key)))
    {
        return 0;
    }
    checksum = ClientInputMessageComputeChecksum(
        key,
        reinterpret_cast<const uint8_t*>(message +
                                          ClientInputMessageSignedDataOffset),
        client_input_type_table,
        client_input_type_fallback,
        client_input_hash_inner_mask,
        client_input_hash_outer_mask);
    if (!write_bytes(message + ClientInputMessageChecksumOffset,
                     &checksum,
                     sizeof(checksum)))
    {
        return 0;
    }
    return checksum;
}

static int client_input_message_is_valid(void* memory, size_t size)
{
    uintptr_t address = reinterpret_cast<uintptr_t>(memory);
    if (!memory || !size)
    {
        return 0;
    }
    if ((address & 7ULL) != 0)
    {
        return 0;
    }
    if (!addr_readable(address, size))
    {
        return 0;
    }
    if (!addr_writable(address, size))
    {
        return 0;
    }
    return 1;
}

void* allocate_client_input_message()
{
    void* memory = malloc(ClientInputMessageSize);
    if (!client_input_message_is_valid(memory, ClientInputMessageSize))
    {
        free(memory);
        return nullptr;
    }
    memset(memory, 0, ClientInputMessageSize);
    return memory;
}

static int client_input_layout_probe(uintptr_t constructor)
{
    void* probe = malloc(ClientInputMessageProbeSize);
    int32_t type = 0;
    int32_t coordinate_x = 0;
    int32_t coordinate_y = 0;
    int matches = 0;
    if (!probe)
    {
        return 0;
    }
    memset(probe, 0, ClientInputMessageProbeSize);
    reinterpret_cast<void (*)(void*, int)>(constructor)(
        probe, ClientInputMessageTypeMove);
    memcpy(&type,
           static_cast<const uint8_t*>(probe) + ClientInputMessageTypeOffset,
           sizeof(type));
    memcpy(&coordinate_x,
           static_cast<const uint8_t*>(probe) + ClientInputMessageXOffset,
           sizeof(coordinate_x));
    memcpy(&coordinate_y,
           static_cast<const uint8_t*>(probe) + ClientInputMessageYOffset,
           sizeof(coordinate_y));
    matches = type == ClientInputMessageTypeMove && coordinate_x == 0 &&
              coordinate_y == 0;
    free(probe);
    return matches;
}

static void* client_input_battle_instance()
{
    static uintptr_t slot = 0;
    uintptr_t function =
        client_input_resolve(&slot, BattleModeGetInstanceRva);
    if (!function)
    {
        return nullptr;
    }
    return reinterpret_cast<void* (*)()>(function)();
}

static void* client_input_manager_of_battle(void* battle)
{
    void* manager = nullptr;
    if (!battle)
    {
        return nullptr;
    }
    if (!read_ptr(reinterpret_cast<uintptr_t>(battle) +
                      LogicBattleModeClientInputManagerOffset,
                  &manager))
    {
        return nullptr;
    }
    if (!manager || !pointer_plausible(reinterpret_cast<uintptr_t>(manager)))
    {
        return nullptr;
    }
    return manager;
}

void* get_client_input_manager()
{
    if (!scene_object || !scene_live())
    {
        return nullptr;
    }
    return client_input_manager_of_battle(client_input_battle_instance());
}

static int client_input_manager_queue_is_ready(void* manager)
{
    void* queue = nullptr;
    if (!manager)
    {
        return 0;
    }
    if (!read_ptr(reinterpret_cast<uintptr_t>(manager) +
                      ClientInputManagerInputQueueOffset,
                  &queue) ||
        !queue)
    {
        return 0;
    }
    return 1;
}

int enqueue_client_input_message(int type, int x, int y)
{
    static uintptr_t constructor_slot = 0;
    static uintptr_t add_input_slot = 0;
    int32_t coordinate_x = x;
    int32_t coordinate_y = y;
    int32_t input_type = type;
    uintptr_t constructor = 0;
    uintptr_t add_input = 0;
    void* battle = nullptr;
    void* manager = nullptr;
    void* message = nullptr;
    if (!coord_ok || dead)
    {
        return 0;
    }
    if (type < 0 || type >= static_cast<int>(ClientInputTypeCount))
    {
        return 0;
    }
    if (!scene_object || !scene_live())
    {
        return 0;
    }
    battle = client_input_battle_instance();
    if (!object_plausible(battle))
    {
        return 0;
    }
    manager = client_input_manager_of_battle(battle);
    if (!manager)
    {
        return 0;
    }
    constructor = client_input_resolve(&constructor_slot,
                                       ClientInputMessageConstructorRva);
    add_input = client_input_resolve(&add_input_slot,
                                     ClientInputManagerAddInputRva);
    if (!constructor || !add_input)
    {
        client_input_pipeline_state_value =
            ClientInputPipelineCallablesMissing;
        return 0;
    }
    if (!client_input_layout_confirmed)
    {
        if (!client_input_layout_probe(constructor))
        {
            client_input_pipeline_state_value =
                ClientInputPipelineLayoutMismatch;
            return 0;
        }
        client_input_layout_confirmed = 1;
    }
    if (!client_input_manager_queue_is_ready(manager))
    {
        return 0;
    }
    message = allocate_client_input_message();
    if (!message)
    {
        return 0;
    }
    reinterpret_cast<void (*)(void*, int)>(constructor)(message, input_type);
    if (!write_bytes(reinterpret_cast<uintptr_t>(message) +
                         ClientInputMessageTypeOffset,
                     &input_type,
                     sizeof(input_type)) ||
        !write_bytes(reinterpret_cast<uintptr_t>(message) +
                         ClientInputMessageXOffset,
                     &coordinate_x,
                     sizeof(coordinate_x)) ||
        !write_bytes(reinterpret_cast<uintptr_t>(message) +
                         ClientInputMessageYOffset,
                     &coordinate_y,
                     sizeof(coordinate_y)))
    {
        free(message);
        return 0;
    }
    sign_client_input_message(message, battle);
    reinterpret_cast<void (*)(void*, void*)>(add_input)(manager, message);
    client_input_pipeline_state_value = ClientInputPipelineReady;
    return 1;
}

int enqueue_move_input_with_type(int x, int y, int type)
{
    if (!enqueue_client_input_message(type, x, y))
    {
        return 0;
    }
    if (type == ClientInputMessageTypeMove)
    {
        (void)set_client_prediction_move_to(x, y);
    }
    return 1;
}

int enqueue_move_input(int x, int y)
{
    return enqueue_move_input_with_type(x, y, ClientInputMessageTypeMove);
}

int set_client_prediction_move_to(int x, int y)
{
    static uintptr_t slot = 0;
    uintptr_t function = 0;
    uintptr_t controller_object = 0;
    if (!coord_ok)
    {
        return 0;
    }
    function = client_input_resolve(
        &slot, LogicBattleModeClientSetClientPredictionMoveToRva);
    if (!function)
    {
        return 0;
    }
    controller_object = controller();
    if (!controller_object)
    {
        return 0;
    }
    reinterpret_cast<void (*)(uintptr_t, int, int, int)>(function)(
        controller_object, x, y, ClientInputMessagePredictionFlag);
    return 1;
}
