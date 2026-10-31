#include "commands.h"
#include <cstdlib>
#include <cstring>
#include "offsets.h"
#include "scan.h"
inline constexpr int PendingCommandMax = 16;
inline constexpr int PendingRetryMax   = 4;
struct PendingCommand
{
    int     type;
    int     x;
    int     y;
    int     predict;
    uint8_t done_input;
    uint8_t done_predict;
    uint8_t tries;
};
static PendingCommand pending_command[PendingCommandMax];
static int            pending_count    = 0;
static uintptr_t      constructor_slot = 0;
static uintptr_t      input_slot       = 0;
static uintptr_t      predict_slot     = 0;
static uintptr_t client_input_resolve(uintptr_t* slot, uintptr_t rva)
{
    if (!*slot)
    {
        uintptr_t resolved = callable(rva);
        if (resolved && addr_readable(resolved, sizeof(uint32_t)) &&
            addr_executable(resolved))
        {
            *slot = resolved;
        }
    }
    return *slot;
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
static void* allocate_client_input_message()
{
    void* memory = malloc(CLIENTINPUTMESSAGE__SIZE);
    if (memory)
    {
        memset(memory, 0, CLIENTINPUTMESSAGE__SIZE);
    }
    return memory;
}
static void pending_push(int type, int x, int y, int predict)
{
    for (int i = 0; i < pending_count; i++)
    {
        PendingCommand& cmd = pending_command[i];
        if (cmd.type != type) continue;
        if (cmd.x != x || cmd.y != y)
        {
            cmd.x            = x;
            cmd.y            = y;
            cmd.done_input   = 0;
            cmd.done_predict = 0;
            cmd.tries        = 0;
        }
        cmd.predict = (cmd.predict || predict) ? 1 : 0;
        return;
    }
    PendingCommand* slot = nullptr;
    if (pending_count >= PendingCommandMax)
    {
        for (int i = 1; i < PendingCommandMax; i++)
        {
            pending_command[i - 1] = pending_command[i];
        }
        pending_count = PendingCommandMax - 1;
        slot          = &pending_command[pending_count];
    }
    else
    {
        slot = &pending_command[pending_count++];
    }
    slot->type         = type;
    slot->x            = x;
    slot->y            = y;
    slot->predict      = predict ? 1 : 0;
    slot->done_input   = 0;
    slot->done_predict = 0;
    slot->tries        = 0;
}
static int client_input_enqueue(
    int       type,
    int       x,
    int       y,
    void*     manager,
    int       queue_ok,
    uintptr_t constructor,
    uintptr_t input)
{
    if (!manager || !queue_ok || !constructor || !input)
    {
        return -1;
    }
    void* message = allocate_client_input_message();
    if (!message)
    {
        return -1;
    }
    reinterpret_cast<void (*)(void*, int)>(constructor)(message, type);
    int32_t* fields = reinterpret_cast<int32_t*>(message);
    fields[CLIENTINPUTMESSAGE__TYPE / 4] = static_cast<int32_t>(type);
    fields[CLIENTINPUTMESSAGE__X / 4]    = static_cast<int32_t>(x);
    fields[CLIENTINPUTMESSAGE__Y / 4]    = static_cast<int32_t>(y);
    reinterpret_cast<void (*)(void*, void*)>(input)(manager, message);
    return 1;
}
static int client_prediction_send(int        x,
                                  int        y,
                                  uintptr_t  function,
                                  uintptr_t  controller_object)
{
    if (!function || !controller_object)
    {
        return -1;
    }
    reinterpret_cast<void (*)(uintptr_t, int, int, int)>(function)(
        controller_object, x, y, ClientInputMessagePredictionFlag);
    return 1;
}
void commands_flush()
{
    if (!pending_count)
    {
        return;
    }
    if (!coord_ok || dead)
    {
        pending_count = 0;
        return;
    }
    void*     battle            = reinterpret_cast<void*>(scene_object);
    uintptr_t predict_fn        = 0;
    uintptr_t constructor       = 0;
    uintptr_t input             = 0;
    void*     manager           = nullptr;
    int       queue_ok          = 0;
    int       kept              = 0;
    int scene_ok = scene_object && object_plausible(battle) ? 1 : 0;
    if (scene_ok)
    {
        manager     = client_input_manager_of_battle(battle);
        queue_ok    = client_input_manager_queue_is_ready(manager);
        constructor = client_input_resolve(&constructor_slot,
                                           CLIENTINPUTMESSAGE__CONSTRUCTOR);
        input = client_input_resolve(&input_slot, CLIENTINPUTMANAGER__ADDINPUT);
    }
    predict_fn = client_input_resolve(
        &predict_slot, LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO);
    uintptr_t controller_object = controller();
    for (int i = 0; i < pending_count; i++)
    {
        PendingCommand cmd = pending_command[i];
        if (cmd.type >= 0 && !cmd.done_input)
        {
            if (client_input_enqueue(cmd.type,
                                     cmd.x,
                                     cmd.y,
                                     manager,
                                     queue_ok,
                                     constructor,
                                     input) > 0)
            {
                cmd.done_input = 1;
            }
        }
        if (cmd.predict && !cmd.done_predict)
        {
            if (client_prediction_send(
                    cmd.x, cmd.y, predict_fn, controller_object) > 0)
            {
                cmd.done_predict = 1;
            }
        }
        if ((cmd.type < 0 || cmd.done_input) &&
            (!cmd.predict || cmd.done_predict))
        {
            continue;
        }
        if (++cmd.tries >= PendingRetryMax)
        {
            continue;
        }
        pending_command[kept++] = cmd;
    }
    pending_count = kept;
}
int enqueue_move_input_with_type(int x, int y, int type)
{
    if (type < 0 || type >= ClientInputMessageTypeMax)
    {
        return 0;
    }
    if (!coord_ok || dead)
    {
        return 0;
    }
    pending_push(type, x, y, type == ClientInputMessageTypeMove ? 1 : 0);
    return 1;
}
int enqueue_move_input(int x, int y)
{
    return enqueue_move_input_with_type(x, y, ClientInputMessageTypeMove);
}
int client_prediction_move_to(int x, int y)
{
    if (!coord_ok)
    {
        return 0;
    }
    pending_push(-1, x, y, 1);
    return 1;
}
