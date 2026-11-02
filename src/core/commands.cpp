#include "commands.h"
#include <cstdlib>
#include "offsets.h"
#include "scan.h"
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

static uintptr_t client_input_constructor = 0;
static uintptr_t client_input_add         = 0;
static uintptr_t client_input_predict     = 0;

void client_input_resolve_all()
{
    client_input_constructor = callable(CLIENTINPUT__CONSTRUCTOR);
    client_input_add         = callable(CLIENTINPUTMANAGER__ADDINPUT);
    client_input_predict     = callable(LOGICBATTLEMODECLIENT__SETCLIENTPREDICTIONMOVETO);
}

static void* client_input_manager(void* battle)
{
    void* manager = nullptr;
    void* queue   = nullptr;
    if (!battle) return nullptr;
    if (!read_ptr(reinterpret_cast<uintptr_t>(battle) + LOGICBATTLEMODECLIENT__INPUTMANAGER,
                  &manager) ||
        !manager)
    {
        return nullptr;
    }
    if (!read_ptr(reinterpret_cast<uintptr_t>(manager) + CLIENTINPUTMANAGER__QUEUE, &queue) ||
        !queue)
    {
        return nullptr;
    }
    return manager;
}

static void pending_push(int type, int x, int y, int predict)
{
    for (int i = 0; i < pending_count; i++)
    {
        PendingCommand& cmd = pending_command[i];
        if (cmd.type != type) continue;
        if (cmd.x != x || cmd.y != y)
        {
            cmd.x = x;
            cmd.y = y;
        }
        cmd.predict = (cmd.predict || predict) ? 1 : 0;
        return;
    }
    PendingCommand* slot = nullptr;
    if (pending_count >= PendingCommandMax)
    {
        __builtin_memmove(
            pending_command, pending_command + 1, sizeof(PendingCommand) * (PendingCommandMax - 1));
        pending_count = PendingCommandMax - 1;
        slot          = &pending_command[pending_count];
    }
    else
    {
        slot = &pending_command[pending_count++];
    }
    slot->type    = type;
    slot->x       = x;
    slot->y       = y;
    slot->predict = predict ? 1 : 0;
}

static void client_input_enqueue(int type, int x, int y, void* manager)
{
    if (!client_input_constructor || !client_input_add)
    {
        return;
    }
    void* input = malloc(CLIENTINPUT__SIZE);
    if (!input)
    {
        return;
    }
    reinterpret_cast<void (*)(void*, int)>(client_input_constructor)(input, type);
    int32_t* fields            = reinterpret_cast<int32_t*>(input);
    fields[CLIENTINPUT__X / 4] = static_cast<int32_t>(x);
    fields[CLIENTINPUT__Y / 4] = static_cast<int32_t>(y);
    reinterpret_cast<void (*)(void*, void*)>(client_input_add)(manager, input);
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
    void*     battle     = reinterpret_cast<void*>(scene_object);
    void*     manager    = client_input_manager(battle);
    uintptr_t controller = client();
    if (!manager)
    {
        pending_count = 0;
        return;
    }
    for (int i = 0; i < pending_count; i++)
    {
        PendingCommand& command = pending_command[i];
        if (command.type >= 0)
        {
            client_input_enqueue(command.type, command.x, command.y, manager);
        }
        if (command.predict)
        {
            reinterpret_cast<void (*)(uintptr_t, int, int, int)>(client_input_predict)(
                controller, command.x, command.y, ClientInputMessagePredictionFlag);
        }
    }
    pending_count = 0;
}

int enqueue_move_input(int x, int y)
{
    if (!coord_ok || dead)
    {
        return 0;
    }
    pending_push(ClientInputMessageTypeMove, x, y, 1);
    return 1;
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
