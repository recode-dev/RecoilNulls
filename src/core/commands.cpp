#include "commands.h"
#include <cstdlib>
#include <cstring>
#include "offsets.h"
#include "scan.h"
static int   client_input_message_is_valid(void* memory, size_t size);
static void* client_input_battle_instance();
static void* client_input_manager_of_battle(void* battle);
static int   client_input_manager_queue_is_ready(void* manager);
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
static int client_input_message_is_valid(void* memory, size_t size)
{
    uintptr_t address = reinterpret_cast<uintptr_t>(memory);
    if (!memory || !size)
    {
        return 0;
    }
    if ((address & 7) != 0)
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
    void* memory = malloc(0x48);
    if (!client_input_message_is_valid(memory, 0x48))
    {
        free(memory);
        return nullptr;
    }
    memset(memory, 0, 0x48);
    return memory;
}
static void* client_input_battle_instance()
{
    static uintptr_t slot = 0;
    uintptr_t function = client_input_resolve(&slot, BATTLEMODE__GETINSTANCE);
    if (!function)
    {
        return nullptr;
    }
    return reinterpret_cast<void* (*) ()>(function)();
}
static void* client_input_manager_of_battle(void* battle)
{
    void* manager = nullptr;
    if (!battle)
    {
        return nullptr;
    }
    if (!read_ptr(reinterpret_cast<uintptr_t>(battle) +
                      LOGICBATTLEMODECLIENT__INPUTMANAGER,
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
static int client_input_manager_queue_is_ready(void* manager)
{
    void* queue = nullptr;
    if (!manager)
    {
        return 0;
    }
    if (!read_ptr(reinterpret_cast<uintptr_t>(manager) + 0x20, &queue) ||
        !queue)
    {
        return 0;
    }
    return 1;
}
int enqueue_client_input_message(int type, int x, int y)
{
    static uintptr_t constructor_slot = 0;
    static uintptr_t add_input_slot   = 0;
    int32_t          coordinate_x     = x;
    int32_t          coordinate_y     = y;
    int32_t          input_type       = type;
    uintptr_t        constructor      = 0;
    uintptr_t        add_input        = 0;
    void*            battle           = nullptr;
    void*            manager          = nullptr;
    void*            message          = nullptr;
    if (!coord_ok || dead)
    {
        return 0;
    }
    if (type < 0 || type >= 0x17)
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
                                       CLIENTINPUTMESSAGE__CONSTRUCTOR);
    add_input =
        client_input_resolve(&add_input_slot, CLIENTINPUTMANAGER__ADDINPUT);
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
    if (!write_bytes(reinterpret_cast<uintptr_t>(message) + 0x8,
                     &input_type,
                     sizeof(input_type)) ||
        !write_bytes(reinterpret_cast<uintptr_t>(message) + 0xc,
                     &coordinate_x,
                     sizeof(coordinate_x)) ||
        !write_bytes(reinterpret_cast<uintptr_t>(message) + 0x10,
                     &coordinate_y,
                     sizeof(coordinate_y)))
    {
        free(message);
        return 0;
    }
    reinterpret_cast<void (*)(void*, void*)>(add_input)(manager, message);
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
        (void) set_client_prediction_move_to(x, y);
    }
    return 1;
}
int enqueue_move_input(int x, int y)
{
    return enqueue_move_input_with_type(x, y, ClientInputMessageTypeMove);
}
int set_client_prediction_move_to(int x, int y)
{
    static uintptr_t slot              = 0;
    uintptr_t        function          = 0;
    uintptr_t        controller_object = 0;
    if (!coord_ok)
    {
        return 0;
    }
    function = client_input_resolve(
        &slot, LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO);
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
