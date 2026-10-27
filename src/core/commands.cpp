#include "commands.h"
#include "../utils/crypto.h"
#include "offsets.h"
#include "scan.h"
#include <malloc/malloc.h>
#include <cstring>
#if CI_HASH_INNER_MASK_RVA && CI_HASH_OUTER_MASK_RVA && BM_HASH_ENABLED_OFF && \
    BM_HASH_KEY_OFF
#define CI_SIGNING_ON 1
#else
#define CI_SIGNING_ON 0
#endif
static uint32_t ci_table[CI_TABLE_TYPES];
static uint32_t ci_fallback = 0;
static uint8_t ci_inner[0x10];
static uint8_t ci_outer[0x10];
static int ci_ready = 0;
static int msg_ok(void* ptr, size_t size);
int ci_load_constants()
{
    int i = 0;
    if (ci_ready)
    {
        return 1;
    }
    if (!base)
    {
        return 0;
    }
    for (i = 0; i < static_cast<int>(CI_TABLE_TYPES); i++)
    {
        if (!read_bytes(base + CI_TYPE_TABLE_RVA +
                            static_cast<uintptr_t>(i) * 4ULL,
                        &ci_table[i],
                        sizeof(uint32_t)))
        {
            return 0;
        }
    }
    if (!read_bytes(
            base + CI_TYPE_FALLBACK_RVA, &ci_fallback, sizeof(uint32_t)))
    {
        return 0;
    }
    if (!read_bytes(base + CI_HASH_INNER_MASK_RVA, ci_inner, 0x10))
    {
        return 0;
    }
    if (!read_bytes(base + CI_HASH_OUTER_MASK_RVA, ci_outer, 0x10))
    {
        return 0;
    }
    ci_ready = 1;
    return 1;
}
uint32_t ci_sign([[maybe_unused]] void* ci, [[maybe_unused]] void* battle)
{
#if CI_SIGNING_ON
    uint8_t key[0x10];
    uint8_t cmd[0x10];
    uint8_t enabled = 0;
    uint32_t token = 0;
    if (!ci || !battle)
    {
        return 0;
    }
    if (!read_bytes((uintptr_t)battle + BM_HASH_ENABLED_OFF, &enabled, 1))
    {
        return 0;
    }
    if (enabled == 0)
    {
        return 0;
    }
    if (!ci_load_constants())
    {
        return 0;
    }
    if (!read_bytes((uintptr_t)battle + BM_HASH_KEY_OFF, key, 0x10))
    {
        return 0;
    }
    if (!read_bytes((uintptr_t)ci + CI_TOKEN_READ_OFF, cmd, 0x10))
    {
        return 0;
    }
    token =
        ci_compute_token(key, cmd, ci_table, ci_fallback, ci_inner, ci_outer);
    if (!write_bytes((uintptr_t)ci + CI_TOKEN_OFF, &token, sizeof(token)))
    {
        return 0;
    }
    return token;
#else
    return 0;
#endif
}
int enqueue_type(int x, int y, int type)
{
    return enqueue_skill(x, y, type, nullptr);
}
static uintptr_t callable_cached(uintptr_t* cache, uintptr_t rva)
{
    if (!*cache)
    {
        *cache = entry_callable(rva);
    }
    return *cache;
}
static int ptr_is_heap(const void* p)
{
    if (!p || ((uintptr_t)p & 7ULL))
    {
        return 0;
    }
    return malloc_zone_from_ptr(const_cast<void*>(p)) != nullptr;
}
int enqueue_skill(int x, int y, int type, void* skillData)
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
    ctorFn = callable_cached(&c_ctor, MSGCTOR_RVA);
    inputFn = callable_cached(&c_input, ADDINPUT_RVA);
    battleFn = callable_cached(&c_battle, RVA_BATTLEMODE_GETINSTANCE);
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
    mgr = manager();
    if (!mgr)
    {
        return 0;
    }
    {
        void* mgrInner = nullptr;
        if (!read_ptr((uintptr_t)mgr + CI_MGR_QUEUE_OFF, &mgrInner) ||
            !mgrInner)
        {
            return 0;
        }
    }
    msg = msg_alloc();
    if (!msg_ok(msg,
                static_cast<size_t>(MSG_SIZE) + static_cast<size_t>(MSG_SLACK)))
    {
        return 0;
    }
    memset(
        msg, 0, static_cast<size_t>(MSG_SIZE) + static_cast<size_t>(MSG_SLACK));
    ((void (*)(void*, int))ctorFn)(msg, TYPE_MOVE);
    write_bytes((uintptr_t)msg + TYPE_OFF, &type, sizeof(type));
    write_bytes((uintptr_t)msg + X_OFF, &vx, sizeof(vx));
    write_bytes((uintptr_t)msg + Y_OFF, &vy, sizeof(vy));
    if (skillData && ptr_is_heap(skillData))
    {
        write_bytes((uintptr_t)msg +
                        static_cast<uintptr_t>(OFF_CLIENTINPUT_SKILLDATA),
                    &skillData,
                    sizeof(skillData));
    }
    ci_sign(msg, battle);
    if (type == TYPE_MOVE)
    {
        (void)pred_set(x, y);
    }
    ((void (*)(void*, void*))inputFn)(mgr, msg);
    return 1;
}
int enqueue(int x, int y)
{
    return enqueue_type(x, y, TYPE_MOVE);
}
uintptr_t entry_callable(uintptr_t rva)
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
static int msg_ok(void* ptr, size_t size)
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
void* msg_alloc()
{
    uintptr_t stub = entry_callable(ALLOC_RVA);
    uintptr_t got = 0;
    size_t size =
        static_cast<size_t>(MSG_SIZE) + static_cast<size_t>(MSG_SLACK);
    void* mem = nullptr;
    mem = malloc(size);
    if (msg_ok(mem, size))
    {
        memset(mem, 0, size);
        return mem;
    }
    if (stub)
    {
        mem = ((void* (*)(size_t))stub)(size);
        if (msg_ok(mem, size))
        {
            memset(mem, 0, size);
            return mem;
        }
    }
    // read_ptr принимает void** (out-param) — static_cast не применим
    if (base &&
        read_ptr(base + ALLOC_GOT_RVA, reinterpret_cast<void**>(&got)) && got)
    {
        mem = ((void* (*)(size_t))got)(size);
        if (msg_ok(mem, size))
        {
            memset(mem, 0, size);
            return mem;
        }
    }
    return nullptr;
}
void* manager()
{
    if (!scene_object || !scene_live())
    {
        return nullptr;
    }
    static uintptr_t c_battle = 0;
    uintptr_t battleFn = callable_cached(&c_battle, RVA_BATTLEMODE_GETINSTANCE);
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
    if (!read_ptr((uintptr_t)battle + MGR_OFF, &mgr) || !mgr)
    {
        return nullptr;
    }
    return mgr;
}
int pred_set(int x, int y)
{
    uintptr_t setFn = 0;
    uintptr_t ctrl = 0;
    if (!coord_ok)
    {
        return 0;
    }
    {
        static uintptr_t c_set = 0;
        setFn = callable_cached(&c_set, MOVE_RVA);
    }
    ctrl = controller();
    if (!setFn || !ctrl)
    {
        return 0;
    }
    ((void (*)(uintptr_t, int, int, int))setFn)(ctrl, x, y, PRED_FLAG);
    return 1;
}
int move_to(int32_t x, int32_t y)
{
    uintptr_t fn = 0;
    uintptr_t ctrl = 0;
    if (dead)
    {
        return 0;
    }
    if (x < -200000 || x > 200000 || y < -200000 || y > 200000)
    {
        return 0;
    }
    {
        static uintptr_t c_move = 0;
        fn = callable_cached(&c_move, MOVE_RVA);
    }
    ctrl = controller();
    if (!fn || !ctrl)
    {
        return 0;
    }
    ((void (*)(void*, int, int, int))fn)(
        (void*)ctrl, static_cast<int>(x), static_cast<int>(y), 1);
    return 1;
}
