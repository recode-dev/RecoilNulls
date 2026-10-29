#include "commands.h"

#include <cstdlib>
#include <cstring>

#include "../utils/crypto.h"
#include "offsets.h"
#include "scan.h"

static constexpr uintptr_t ClientInputTypeTableScanSpan      = 0x20000;
static constexpr size_t    ClientInputTypeTableScanChunk     = 0x4000;
static const uint8_t       client_input_type_table_anchor[8] = {
          0xd2, 0xc7, 0xb1, 0xa3, 0xb8, 0xa6, 0xf5, 0xe4};

static uint32_t client_input_type_table[CLIENTINPUT__TYPECOUNT];
static uint32_t client_input_type_fallback = 0;
static uint8_t  client_input_hash_inner_mask[0x10];
static uint8_t  client_input_hash_outer_mask[0x10];
static int      client_input_constants_ready      = 0;
static int      client_input_layout_confirmed     = 0;
static int      client_input_pipeline_state_value = ClientInputPipelineUnknown;

static int   client_input_message_is_valid(void* memory, size_t size);
static void* client_input_battle_instance();
static void* client_input_manager_of_battle(void* battle);
static int   client_input_manager_queue_is_ready(void* manager);
static int   client_input_layout_probe(uintptr_t constructor);

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
    int count   = static_cast<int>(CLIENTINPUT__TYPECOUNT);
    int nonzero = 0;
    int unique  = 0;
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
    uint32_t table[CLIENTINPUT__TYPECOUNT];
    uint32_t fallback = 0;
    uint8_t  inner[0x10];
    uint8_t  outer[0x10];
    if (!base || !tableRva)
    {
        return 0;
    }
    if (tableRva <= CLIENTINPUTMESSAGE__TYPETABLEINNERMASKDISTANCE)
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
    if (!read_bytes(base + tableRva - CLIENTINPUT__TYPETABLEFALLBACKDISTANCE,
                    &fallback,
                    sizeof(fallback)))
    {
        return 0;
    }
    if (!read_bytes(base + tableRva -
                        CLIENTINPUTMESSAGE__TYPETABLEINNERMASKDISTANCE,
                    inner,
                    sizeof(inner)))
    {
        return 0;
    }
    if (!read_bytes(base + tableRva -
                        CLIENTINPUTMESSAGE__TYPETABLEOUTERMASKDISTANCE,
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
    uint8_t   buffer[ClientInputTypeTableScanChunk +
                   sizeof(client_input_type_table_anchor)];
    uintptr_t start = CLIENTINPUT__TYPETABLE - ClientInputTypeTableScanSpan;
    uintptr_t end   = CLIENTINPUT__TYPETABLE + ClientInputTypeTableScanSpan +
                    sizeof(client_input_type_table_anchor);
    uintptr_t found = 0;
    int       hits  = 0;
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
    if (client_input_constants_at(CLIENTINPUT__TYPETABLE))
    {
        client_input_constants_ready = 1;
        return 1;
    }
    if (!client_input_constants_relocate(&relocated))
    {
        return 0;
    }
    if (relocated == CLIENTINPUT__TYPETABLE)
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

static int client_input_layout_probe(uintptr_t constructor)
{
    void*   probe        = malloc(CLIENTINPUTMESSAGE__PROBESIZE);
    int32_t type         = 0;
    int32_t coordinate_x = 0;
    int32_t coordinate_y = 0;
    int     matches      = 0;
    if (!probe)
    {
        return 0;
    }
    memset(probe, 0, CLIENTINPUTMESSAGE__PROBESIZE);
    reinterpret_cast<void (*)(void*, int)>(constructor)(
        probe, ClientInputMessageTypeMove);
    memcpy(&type,
           static_cast<const uint8_t*>(probe) + CLIENTINPUTMESSAGE__TYPE,
           sizeof(type));
    memcpy(&coordinate_x,
           static_cast<const uint8_t*>(probe) + CLIENTINPUTMESSAGE__X,
           sizeof(coordinate_x));
    memcpy(&coordinate_y,
           static_cast<const uint8_t*>(probe) + CLIENTINPUTMESSAGE__Y,
           sizeof(coordinate_y));
    matches = type == ClientInputMessageTypeMove && coordinate_x == 0 &&
              coordinate_y == 0;
    free(probe);
    return matches;
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
                      CLIENTINPUTMANAGER__INPUTQUEUE,
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
    if (type < 0 || type >= static_cast<int>(CLIENTINPUT__TYPECOUNT))
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
        client_input_pipeline_state_value = ClientInputPipelineCallablesMissing;
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
        (void) engine_own_pathfind_to(x, y, 0);
    }
    return 1;
}

int enqueue_move_input(int x, int y)
{
    return enqueue_move_input_with_type(x, y, ClientInputMessageTypeMove);
}

int     engine_pathfind_calls = 0;
int     engine_pathfind_ok    = 0;
int     engine_pathfind_skip  = 0;
int32_t engine_pathfind_x     = 0;
int32_t engine_pathfind_y     = 0;

int engine_own_path_length()
{
    uintptr_t self = own_elem;
    uintptr_t function =
        callable_at_rva(LOGICCHARACTERCLIENTOWN__GETPATHLENGTH);
    if (!self || !function)
    {
        return -1;
    }
    return reinterpret_cast<int (*)(uintptr_t)>(function)(self);
}

int engine_own_tick_window(int32_t* start, int32_t* end)
{
    int32_t first = 0;
    int32_t last  = 0;
    if (!own_elem || !start || !end)
    {
        return 0;
    }
    if (!read_int(own_elem + static_cast<uintptr_t>(LOGICCHARACTER__TICKSTART),
                  &first) ||
        !read_int(own_elem + static_cast<uintptr_t>(LOGICCHARACTER__TICKEND),
                  &last))
    {
        return 0;
    }
    *start = first;
    *end   = last;
    return 1;
}

int engine_own_pos_at_tick(int32_t tick, float* x, float* y)
{
    uintptr_t self     = own_elem;
    uintptr_t function = callable_at_rva(LOGICCHARACTERCLIENTOWN__GETPOSATTICK);
    int32_t   position[2] = {0, 0};
    int32_t   previous[2] = {0, 0};
    if (!self || !function || !x || !y)
    {
        return 0;
    }
    reinterpret_cast<void (*)(
        uintptr_t, int32_t, int32_t*, int32_t*, uint32_t)>(function)(
        self, tick, position, previous, 0u);
    *x = static_cast<float>(position[0]);
    *y = static_cast<float>(position[1]);
    return 1;
}

int engine_own_pathfind_to(int x, int y, int flag)
{
    static uint64_t last_us = 0;
    static int32_t  last_x  = 0;
    static int32_t  last_y  = 0;
    uintptr_t       self    = own_elem;
    uintptr_t       battle  = client();
    uintptr_t function = callable_at_rva(LOGICCHARACTERCLIENTOWN__PATHFINDTO);
    uint64_t  elapsed  = last_us ? (us() - last_us) / 1000 : ~0ULL;
    int       remaining;
    engine_pathfind_calls++;
    if (!coord_ok || !self || !battle || !function)
    {
        return 0;
    }
    remaining = engine_own_path_length();
    // Re-planning every tick rebuilds the tick window from the remaining path,
    // so the character keeps getting a fresh short budget and outruns its real
    // speed. Only re-plan when the destination changed or the previous path is
    // nearly spent.
    if (x == last_x && y == last_y)
    {
        if (elapsed < PATHFIND_REFRESH_MS &&
            (remaining < 0 || remaining > PATHFIND_MIN_REMAINING))
        {
            engine_pathfind_skip++;
            return 0;
        }
    }
    else if (elapsed < PATHFIND_MIN_MS && remaining > PATHFIND_MIN_REMAINING)
    {
        engine_pathfind_skip++;
        return 0;
    }
    reinterpret_cast<void (*)(uintptr_t, int, int, bool, uintptr_t)>(function)(
        self, x, y, flag != 0, battle);
    engine_pathfind_ok++;
    engine_pathfind_x = x;
    engine_pathfind_y = y;
    last_us           = us();
    last_x            = x;
    last_y            = y;
    return 1;
}
