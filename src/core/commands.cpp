#include "commands.h"

#include <cstdlib>
#include <cstring>

#include "../utils/crypto.h"
#include "offsets.h"
#include "scan.h"

static uint32_t client_input_type_table[CLIENT_INPUT_TYPE_COUNT];
static uint32_t client_input_type_fallback = 0;
static uint8_t client_input_hash_inner_mask[0x10];
static uint8_t client_input_hash_outer_mask[0x10];
static int client_input_constants_ready = 0;

static int client_input_message_is_valid(void* memory, size_t size);
static void* client_input_battle_instance();
static void* client_input_manager_of_battle(void* battle);
static int client_input_manager_queue_is_ready(void* manager);

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

int load_client_input_constants()
{
    if (client_input_constants_ready)
    {
        return 1;
    }
    if (!base)
    {
        return 0;
    }
    if (!read_bytes(base + CLIENT_INPUT_TYPE_TABLE_RVA,
                    client_input_type_table,
                    sizeof(client_input_type_table)))
    {
        return 0;
    }
    if (!read_bytes(base + CLIENT_INPUT_TYPE_FALLBACK_RVA,
                    &client_input_type_fallback,
                    sizeof(client_input_type_fallback)))
    {
        return 0;
    }
    if (!read_bytes(base + CLIENT_INPUT_HASH_INNER_MASK_RVA,
                    client_input_hash_inner_mask,
                    sizeof(client_input_hash_inner_mask)))
    {
        return 0;
    }
    if (!read_bytes(base + CLIENT_INPUT_HASH_OUTER_MASK_RVA,
                    client_input_hash_outer_mask,
                    sizeof(client_input_hash_outer_mask)))
    {
        return 0;
    }
    client_input_constants_ready = 1;
    return 1;
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
    if (!read_bytes(mode + BATTLE_MODE_HASH_ENABLED_OFF, &enabled, 1))
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
    if (!read_bytes(mode + BATTLE_MODE_HASH_KEY_OFF, key, sizeof(key)))
    {
        return 0;
    }
    checksum = ci_compute_token(key,
                                reinterpret_cast<const uint8_t*>(
                                    message + CLIENT_INPUT_MESSAGE_SIGNED_OFF),
                                client_input_type_table,
                                client_input_type_fallback,
                                client_input_hash_inner_mask,
                                client_input_hash_outer_mask);
    if (!write_bytes(message + CLIENT_INPUT_MESSAGE_CHECKSUM_OFF,
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
    void* memory = malloc(CLIENT_INPUT_MESSAGE_ALLOCATION_SIZE);
    if (!client_input_message_is_valid(memory,
                                       CLIENT_INPUT_MESSAGE_ALLOCATION_SIZE))
    {
        free(memory);
        return nullptr;
    }
    memset(memory, 0, CLIENT_INPUT_MESSAGE_ALLOCATION_SIZE);
    return memory;
}

static void* client_input_battle_instance()
{
    static uintptr_t slot = 0;
    uintptr_t function =
        client_input_resolve(&slot, RVA_BATTLE_MODE_GET_INSTANCE);
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
                      LOGIC_BATTLE_MODE_CLIENT_INPUT_MANAGER_OFF,
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
                      CLIENT_INPUT_MANAGER_QUEUE_OFF,
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
    if (type < 0 || type >= CLIENT_INPUT_MESSAGE_TYPE_LIMIT)
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
    constructor =
        client_input_resolve(&constructor_slot, RVA_CLIENT_INPUT_MESSAGE_CONSTRUCTOR);
    add_input =
        client_input_resolve(&add_input_slot, RVA_CLIENT_INPUT_MANAGER_ADD_INPUT);
    if (!constructor || !add_input)
    {
        return 0;
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
                         CLIENT_INPUT_MESSAGE_TYPE_OFF,
                     &input_type,
                     sizeof(input_type)) ||
        !write_bytes(reinterpret_cast<uintptr_t>(message) +
                         CLIENT_INPUT_MESSAGE_X_OFF,
                     &coordinate_x,
                     sizeof(coordinate_x)) ||
        !write_bytes(reinterpret_cast<uintptr_t>(message) +
                         CLIENT_INPUT_MESSAGE_Y_OFF,
                     &coordinate_y,
                     sizeof(coordinate_y)))
    {
        free(message);
        return 0;
    }
    sign_client_input_message(message, battle);
    reinterpret_cast<void (*)(void*, void*)>(add_input)(manager, message);
    return 1;
}

int enqueue_move_input_with_type(int x, int y, int type)
{
    if (!enqueue_client_input_message(type, x, y))
    {
        return 0;
    }
    if (type == CLIENT_INPUT_MESSAGE_TYPE_MOVE)
    {
        (void)set_client_prediction_move_to(x, y);
    }
    return 1;
}

int enqueue_move_input(int x, int y)
{
    return enqueue_move_input_with_type(x, y, CLIENT_INPUT_MESSAGE_TYPE_MOVE);
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
        &slot, RVA_LOGIC_BATTLE_MODE_CLIENT_SET_CLIENT_PREDICTION_MOVE_TO);
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
        controller_object, x, y, CLIENT_INPUT_MESSAGE_PREDICTION_FLAG);
    return 1;
}
