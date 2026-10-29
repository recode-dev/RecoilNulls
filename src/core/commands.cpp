#include "commands.h"
#include "../utils/crypto.h"
#include "offsets.h"
#include "scan.h"
#include <malloc/malloc.h>
#include <cstdlib>
#include <cstring>
static uint32_t client_input_type_table[CLIENT_INPUT_TYPE_COUNT];
static uint32_t client_input_type_fallback = 0;
static uint8_t client_input_inner_mask[0x10];
static uint8_t client_input_outer_mask[0x10];
static int client_input_constants_ready = 0;
static int client_input_message_is_valid(void* ptr, size_t size);
int load_client_input_constants()
{
    int i = 0;
    if (client_input_constants_ready)
    {
        return 1;
    }
    if (!base)
    {
        return 0;
    }
    for (i = 0; i < static_cast<int>(CLIENT_INPUT_TYPE_COUNT); i++)
    {
        if (!read_bytes(base + CLIENT_INPUT_TYPE_TABLE_RVA +
                            static_cast<uintptr_t>(i) * 4ULL,
                        &client_input_type_table[i],
                        sizeof(uint32_t)))
        {
            return 0;
        }
    }
    if (!read_bytes(base + CLIENT_INPUT_TYPE_FALLBACK_RVA,
                    &client_input_type_fallback,
                    sizeof(uint32_t)))
    {
        return 0;
    }
    if (!read_bytes(base + CLIENT_INPUT_HASH_INNER_MASK_RVA, client_input_inner_mask, 0x10))
    {
        return 0;
    }
    if (!read_bytes(base + CLIENT_INPUT_HASH_OUTER_MASK_RVA, client_input_outer_mask, 0x10))
    {
        return 0;
    }
    client_input_constants_ready = 1;
    return 1;
}
uint32_t sign_client_input_message(void* ci, void* battle)
{
    uint8_t key[0x10];
    uint8_t cmd[0x10];
    uint8_t enabled = 0;
    uint32_t token = 0;
    if (!ci || !battle)
    {
        return 0;
    }
    if (!read_bytes(
            (uintptr_t)battle + BATTLE_MODE_HASH_ENABLED_OFF, &enabled, 1))
    {
        return 0;
    }
    if (enabled == 0)
    {
        return 0;
    }
    if (!load_client_input_constants())
    {
        return 0;
    }
    if (!read_bytes((uintptr_t)battle + BATTLE_MODE_HASH_KEY_OFF, key, 0x10))
    {
        return 0;
    }
    if (!read_bytes((uintptr_t)ci + CLIENT_INPUT_TOKEN_INPUT_OFF, cmd, 0x10))
    {
        return 0;
    }
    token =
        ci_compute_token(key, cmd, client_input_type_table, client_input_type_fallback, client_input_inner_mask, client_input_outer_mask);
    if (!write_bytes(
            (uintptr_t)ci + CLIENT_INPUT_TOKEN_OFF, &token, sizeof(token)))
    {
        return 0;
    }
    return token;
}
int enqueue_move_input_with_type(int x, int y, int type)
{
    return enqueue_skill_input(x, y, type, nullptr);
}
static uintptr_t callable_at_rva_cached(uintptr_t* cache, uintptr_t rva)
{
    if (!*cache)
    {
        *cache = callable_at_rva(rva);
    }
    return *cache;
}
static int pointer_is_heap_address(const void* p)
{
    if (!p || ((uintptr_t)p & 7ULL))
    {
        return 0;
    }
    return malloc_zone_from_ptr(const_cast<void*>(p)) != nullptr;
}
int enqueue_skill_input(int x, int y, int type, void* skillData)
{
    uintptr_t ctorFn = 0;
    uintptr_t inputFn = 0;
    uintptr_t battleFn = 0;
    int32_t vx = x;
    int32_t vy = y;
    void* battle = nullptr;
    void* mgr = nullptr;
    void* msg = nullptr;
    if (!coord_ok || dead)
    {
        return 0;
    }
    static uintptr_t c_ctor = 0;
    static uintptr_t c_input = 0;
    static uintptr_t c_battle = 0;
    ctorFn = callable_at_rva_cached(&c_ctor, RVA_CLIENT_INPUT_MESSAGE_CONSTRUCTOR);
    inputFn = callable_at_rva_cached(&c_input, RVA_CLIENT_INPUT_MANAGER_ADD_INPUT);
    battleFn = callable_at_rva_cached(&c_battle, RVA_BATTLE_MODE_GET_INSTANCE);
    if (!inputFn || !ctorFn || !battleFn)
    {
        return 0;
    }
    if (!addr_readable(ctorFn, 4) || !addr_readable(inputFn, 4) ||
        !addr_readable(battleFn, 4))
    {
        return 0;
    }
    battle = ((void* (*)())battleFn)();
    if (!object_plausible(battle))
    {
        return 0;
    }
    mgr = get_client_input_manager();
    if (!mgr)
    {
        return 0;
    }
    {
        void* mgrInner = nullptr;
        if (!read_ptr((uintptr_t)mgr + CLIENT_INPUT_MANAGER_QUEUE_OFF,
                      &mgrInner) ||
            !mgrInner)
        {
            return 0;
        }
    }
    msg = allocate_client_input_message();
    if (!client_input_message_is_valid(msg,
                static_cast<size_t>(CLIENT_INPUT_MESSAGE_SIZE) + static_cast<size_t>(CLIENT_INPUT_MESSAGE_ALLOCATION_SLACK)))
    {
        return 0;
    }
    memset(
        msg, 0, static_cast<size_t>(CLIENT_INPUT_MESSAGE_SIZE) + static_cast<size_t>(CLIENT_INPUT_MESSAGE_ALLOCATION_SLACK));
    ((void (*)(void*, int))ctorFn)(msg, CLIENT_INPUT_MESSAGE_TYPE_MOVE);
    if (!write_bytes((uintptr_t)msg + CLIENT_INPUT_MESSAGE_TYPE_OFF,
                     &type,
                     sizeof(type)) ||
        !write_bytes(
            (uintptr_t)msg + CLIENT_INPUT_MESSAGE_X_OFF, &vx, sizeof(vx)) ||
        !write_bytes(
            (uintptr_t)msg + CLIENT_INPUT_MESSAGE_Y_OFF, &vy, sizeof(vy)))
    {
        free(msg);
        return 0;
    }
    if (skillData)
    {
        if (!pointer_is_heap_address(skillData) ||
            !write_bytes((uintptr_t)msg + static_cast<uintptr_t>(
                                              CLIENT_INPUT_SKILL_DATA_OFF),
                         &skillData,
                         sizeof(skillData)))
        {
            free(msg);
            return 0;
        }
    }
    sign_client_input_message(msg, battle);
    if (type == CLIENT_INPUT_MESSAGE_TYPE_MOVE)
    {
        (void)set_client_prediction_move_to(x, y);
    }
    ((void (*)(void*, void*))inputFn)(mgr, msg);
    return 1;
}
int enqueue_move_input(int x, int y)
{
    return enqueue_move_input_with_type(x, y, CLIENT_INPUT_MESSAGE_TYPE_MOVE);
}
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
static int client_input_message_is_valid(void* ptr, size_t size)
{
    if (!ptr)
    {
        return 0;
    }
    if (((uintptr_t)ptr & 7ULL) != 0)
    {
        return 0;
    }
    if (!addr_readable((uintptr_t)ptr, size))
    {
        return 0;
    }
    if (!addr_writable((uintptr_t)ptr, size))
    {
        return 0;
    }
    return 1;
}
void* allocate_client_input_message()
{
    size_t size =
        static_cast<size_t>(CLIENT_INPUT_MESSAGE_SIZE) + static_cast<size_t>(CLIENT_INPUT_MESSAGE_ALLOCATION_SLACK);
    void* mem = malloc(size);
    if (!client_input_message_is_valid(mem, size)) return nullptr;
    memset(mem, 0, size);
    return mem;
}
void* get_client_input_manager()
{
    if (!scene_object || !scene_live())
    {
        return nullptr;
    }
    static uintptr_t c_battle = 0;
    uintptr_t battleFn =
        callable_at_rva_cached(&c_battle, RVA_BATTLE_MODE_GET_INSTANCE);
    void* battle = nullptr;
    void* mgr = nullptr;
    if (!battleFn)
    {
        return nullptr;
    }
    battle = ((void* (*)())battleFn)();
    if (!battle)
    {
        return nullptr;
    }
    if (!read_ptr((uintptr_t)battle +
                      LOGIC_BATTLE_MODE_CLIENT_INPUT_MANAGER_OFF,
                  &mgr) ||
        !mgr)
    {
        return nullptr;
    }
    return mgr;
}
int set_client_prediction_move_to(int x, int y)
{
    uintptr_t setFn = 0;
    uintptr_t ctrl = 0;
    if (!coord_ok)
    {
        return 0;
    }
    {
        static uintptr_t c_set = 0;
        setFn = callable_at_rva_cached(
            &c_set, RVA_LOGIC_BATTLE_MODE_CLIENT_SET_CLIENT_PREDICTION_MOVE_TO);
    }
    ctrl = controller();
    if (!setFn || !ctrl)
    {
        return 0;
    }
    ((void (*)(uintptr_t, int, int, int))setFn)(ctrl, x, y, CLIENT_INPUT_MESSAGE_PREDICTION_FLAG);
    return 1;
}
