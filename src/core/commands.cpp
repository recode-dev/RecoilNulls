#include "commands.h"
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include "offsets.h"
#include "scan.h"
static pthread_mutex_t command_lock = PTHREAD_MUTEX_INITIALIZER;
namespace
{
struct CommandGuard
{
    CommandGuard()
    {
        pthread_mutex_lock(&command_lock);
    }
    ~CommandGuard()
    {
        pthread_mutex_unlock(&command_lock);
    }
};
}
inline constexpr int PendingCommandMax = 16;
struct PendingCommand
{
    int type;
    int x;
    int y;
    int predict;
};
static PendingCommand pending_command[PendingCommandMax];
static int            pending_count = 0;
static int            pending_is_move(const PendingCommand& command)
{
    return static_cast<int>(command.type < 0 ||
                            command.type == ClientInputMessageTypeMove);
}
static int pending_push(int type, int x, int y, int predict)
{
    if (pending_count)
    {
        PendingCommand& tail = pending_command[pending_count - 1];
        if (pending_is_move(tail) && tail.type == type)
        {
            tail.x = x;
            tail.y = y;
            tail.predict =
                static_cast<int>((tail.predict != 0) || (predict) != 0);
            return 1;
        }
    }
    if (pending_count >= PendingCommandMax)
    {
        return 0;
    }
    PendingCommand& slot = pending_command[pending_count++];
    slot.type            = type;
    slot.x               = x;
    slot.y               = y;
    slot.predict         = predict;
    return 1;
}
static int   client_input_message_is_valid(void* memory, size_t size);
static void* client_input_manager_of_battle(void* battle);
static int   client_input_manager_queue_is_ready(void* manager);
uintptr_t    callable_at_rva(uintptr_t rva)
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
    void* memory = malloc(CLIENTINPUTMESSAGE__SIZE);
    if (!client_input_message_is_valid(memory, CLIENTINPUTMESSAGE__SIZE))
    {
        free(memory);
        return nullptr;
    }
    memset(memory, 0, CLIENTINPUTMESSAGE__SIZE);
    return memory;
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
    if (!read_ptr(reinterpret_cast<uintptr_t>(manager) +
                      CLIENTINPUTMANAGER__QUEUE,
                  &queue) ||
        !queue)
    {
        return 0;
    }
    return 1;
}
static int client_input_enqueue_locked(int type, int x, int y)
{
    static uintptr_t constructor_slot = 0;
    static uintptr_t input_slot       = 0;
    int32_t          coordinate_x     = x;
    int32_t          coordinate_y     = y;
    int32_t          input_type       = type;
    uintptr_t        constructor      = 0;
    uintptr_t        input            = 0;
    void*            battle           = nullptr;
    void*            manager          = nullptr;
    void*            message          = nullptr;
    if (!coord_ok || dead)
    {
        return 0;
    }
    if (type < 0 || type >= ClientInputMessageTypeMax)
    {
        return 0;
    }
    if (!scene_object)
    {
        return 0;
    }
    battle = reinterpret_cast<void*>(scene_object);
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
    input = client_input_resolve(&input_slot, CLIENTINPUTMANAGER__ADDINPUT);
    if (!constructor || !input)
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
                         CLIENTINPUTMESSAGE__TYPE,
                     &input_type,
                     sizeof(input_type)) ||
        !write_bytes(reinterpret_cast<uintptr_t>(message) +
                         CLIENTINPUTMESSAGE__X,
                     &coordinate_x,
                     sizeof(coordinate_x)) ||
        !write_bytes(reinterpret_cast<uintptr_t>(message) +
                         CLIENTINPUTMESSAGE__Y,
                     &coordinate_y,
                     sizeof(coordinate_y)))
    {
        free(message);
        return 0;
    }
    reinterpret_cast<void (*)(void*, void*)>(input)(manager, message);
    return 1;
}
static int client_prediction_locked(int x, int y)
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
void commands_flush()
{
    CommandGuard guard;
    for (int i = 0; i < pending_count; i++)
    {
        const PendingCommand& command = pending_command[i];
        if (command.type >= 0)
        {
            (void) client_input_enqueue_locked(
                command.type, command.x, command.y);
        }
        if (command.predict)
        {
            (void) client_prediction_locked(command.x, command.y);
        }
    }
    pending_count = 0;
}
int enqueue_move_input_with_type(int x, int y, int type)
{
    CommandGuard guard;
    if (type < 0 || type >= ClientInputMessageTypeMax)
    {
        return 0;
    }
    if (!coord_ok || dead)
    {
        return 0;
    }
    return pending_push(type, x, y, type == ClientInputMessageTypeMove ? 1 : 0);
}
int enqueue_move_input(int x, int y)
{
    return enqueue_move_input_with_type(x, y, ClientInputMessageTypeMove);
}
int client_prediction_move_to(int x, int y)
{
    CommandGuard guard;
    if (!coord_ok)
    {
        return 0;
    }
    return pending_push(-1, x, y, 1);
}
