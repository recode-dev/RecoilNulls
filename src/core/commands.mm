#include "offsets.h"
#include "scan.h"
#include "commands.h"
#include "../utils/crypto.h"

#include <malloc/malloc.h>

#if RCL_CI_HASH_INNER_MASK_RVA && RCL_CI_HASH_OUTER_MASK_RVA && RCL_BM_HASH_ENABLED_OFF &&         \
    RCL_BM_HASH_KEY_OFF
#define RCL_CI_SIGNING_ON 1
#else
#define RCL_CI_SIGNING_ON 0
#endif

static uint32_t rcl_ci_table[RCL_CI_TABLE_TYPES];

static uint32_t rcl_ci_fallback = 0;

static uint8_t rcl_ci_inner[0x10];

static uint8_t rcl_ci_outer[0x10];

static int rcl_ci_ready = 0;

static int rcl_msg_ok(void *ptr, size_t size);

int rcl_ci_load_constants(void)
{
    int i = 0;
    if (rcl_ci_ready)
    {
        return 1;
    }
    if (!rcl_base)
    {
        return 0;
    }
    for (i = 0; i < (int)RCL_CI_TABLE_TYPES; i++)
    {
        if (!rcl_read_bytes(rcl_base + RCL_CI_TYPE_TABLE_RVA + (uintptr_t)i * 4ULL,
                            &rcl_ci_table[i], sizeof(uint32_t)))
        {
            return 0;
        }
    }
    if (!rcl_read_bytes(rcl_base + RCL_CI_TYPE_FALLBACK_RVA, &rcl_ci_fallback, sizeof(uint32_t)))
    {
        return 0;
    }
    if (!rcl_read_bytes(rcl_base + RCL_CI_HASH_INNER_MASK_RVA, rcl_ci_inner, 0x10))
    {
        return 0;
    }
    if (!rcl_read_bytes(rcl_base + RCL_CI_HASH_OUTER_MASK_RVA, rcl_ci_outer, 0x10))
    {
        return 0;
    }
    rcl_ci_ready = 1;
    return 1;
}

uint32_t rcl_ci_sign(void *ci, void *battle)
{
#if RCL_CI_SIGNING_ON
    uint8_t key[0x10];
    uint8_t cmd[0x10];
    uint8_t enabled = 0;
    uint32_t token = 0;
    if (!ci || !battle)
    {
        return 0;
    }
    if (!rcl_read_bytes((uintptr_t)battle + RCL_BM_HASH_ENABLED_OFF, &enabled, 1))
    {
        return 0;
    }
    if (enabled == 0)
    {
        return 0;
    }
    if (!rcl_ci_load_constants())
    {
        return 0;
    }
    if (!rcl_read_bytes((uintptr_t)battle + RCL_BM_HASH_KEY_OFF, key, 0x10))
    {
        return 0;
    }
    if (!rcl_read_bytes((uintptr_t)ci + RCL_CI_TOKEN_READ_OFF, cmd, 0x10))
    {
        return 0;
    }
    token = rcl_ci_compute_token(key, cmd, rcl_ci_table, rcl_ci_fallback, rcl_ci_inner, rcl_ci_outer);
    if (!rcl_write_bytes((uintptr_t)ci + RCL_CI_TOKEN_OFF, &token, sizeof(token)))
    {
        return 0;
    }
    return token;
#else
    (void)ci;
    (void)battle;
    return 0;
#endif
}

int rcl_enqueue_type(int x, int y, int type)
{
    return rcl_enqueue_skill(x, y, type, nullptr);
}

static uintptr_t rcl_callable_cached(uintptr_t *cache, uintptr_t rva)
{
    if (!*cache)
    {
        *cache = rcl_entry_callable(rva);
    }
    return *cache;
}

static int rcl_ptr_is_heap(const void *p)
{
    if (!p || ((uintptr_t)p & 7ULL))
    {
        return 0;
    }
    return malloc_zone_from_ptr((void *)p) != NULL;
}

int rcl_enqueue_skill(int x, int y, int type, void *skillData)
{
    uintptr_t ctorFn = 0;
    uintptr_t inputFn = 0;
    uintptr_t battleFn = 0;
    int32_t vx = x;
    int32_t vy = y;
    void *battle = nullptr;
    void *mgr = nullptr;
    void *msg = nullptr;
    if (!rcl_coord_ok || rcl_dead)
    {
        return 0;
    }
    static uintptr_t c_ctor = 0;
    static uintptr_t c_input = 0;
    static uintptr_t c_battle = 0;
    ctorFn = rcl_callable_cached(&c_ctor, RCL_MSGCTOR_RVA);
    inputFn = rcl_callable_cached(&c_input, RCL_ADDINPUT_RVA);
    battleFn = rcl_callable_cached(&c_battle, RVA_BATTLEMODE_GETINSTANCE);
    if (!inputFn || !ctorFn || !battleFn)
    {
        return 0;
    }
    if (!rcl_addr_readable(ctorFn, 4) || !rcl_addr_readable(inputFn, 4) ||
        !rcl_addr_readable(battleFn, 4))
    {
        return 0;
    }
    battle = ((void *(*)(void))battleFn)();
    if (!rcl_object_plausible(battle))
    {
        return 0;
    }
    mgr = rcl_manager();
    if (!mgr)
    {
        return 0;
    }
    {
        void *mgrInner = nullptr;
        if (!rcl_read_ptr((uintptr_t)mgr + RCL_CI_MGR_QUEUE_OFF, &mgrInner) || !mgrInner)
        {
            return 0;
        }
    }
    if (RCL_QUEUE_GUARD_MGR && !rcl_manager_shape((uintptr_t)mgr))
    {
        return 0;
    }
    msg = rcl_msg_alloc();
    if (!rcl_msg_ok(msg, (size_t)RCL_MSG_SIZE + (size_t)RCL_MSG_SLACK))
    {
        return 0;
    }
    memset(msg, 0, (size_t)RCL_MSG_SIZE + (size_t)RCL_MSG_SLACK);
    ((void (*)(void *, int))ctorFn)(msg, RCL_TYPE_MOVE);
    rcl_write_bytes((uintptr_t)msg + RCL_TYPE_OFF, &type, sizeof(type));
    rcl_write_bytes((uintptr_t)msg + RCL_X_OFF, &vx, sizeof(vx));
    rcl_write_bytes((uintptr_t)msg + RCL_Y_OFF, &vy, sizeof(vy));
    if (skillData && rcl_ptr_is_heap(skillData))
    {
        rcl_write_bytes((uintptr_t)msg + (uintptr_t)OFF_CLIENTINPUT_SKILLDATA, &skillData,
                        sizeof(skillData));
    }
    rcl_ci_sign(msg, battle);
    if (type == (int)RCL_TYPE_MOVE)
    {
        (void)rcl_pred_set(x, y);
    }
    ((void (*)(void *, void *))inputFn)(mgr, msg);
    return 1;
}

int rcl_enqueue(int x, int y)
{
    return rcl_enqueue_type(x, y, (int)RCL_TYPE_MOVE);
}











uintptr_t rcl_entry_callable(uintptr_t rva)
{
    if (!rcl_base || !rva)
    {
        return 0;
    }
    if (!rcl_callable(rva))
    {
        return 0;
    }
    return rcl_base + rva;
}

static int rcl_msg_ok(void *ptr, size_t size)
{
    if (!ptr)
    {
        return 0;
    }
    if (((uintptr_t)ptr & 7ULL) != 0)
    {
        return 0;
    }
    if (!rcl_addr_readable((uintptr_t)ptr, size))
    {
        return 0;
    }
    if (!rcl_addr_writable((uintptr_t)ptr, size))
    {
        return 0;
    }
    return 1;
}

void *rcl_msg_alloc(void)
{
    uintptr_t stub = rcl_entry_callable(RCL_ALLOC_RVA);
    uintptr_t got = 0;
    size_t size = (size_t)RCL_MSG_SIZE + (size_t)RCL_MSG_SLACK;
    void *mem = nullptr;
    mem = malloc(size);
    if (rcl_msg_ok(mem, size))
    {
        memset(mem, 0, size);
        return mem;
    }
    if (stub)
    {
        mem = ((void *(*)(size_t))stub)(size);
        if (rcl_msg_ok(mem, size))
        {
            memset(mem, 0, size);
            return mem;
        }
    }
    if (rcl_base && rcl_read_ptr(rcl_base + RCL_ALLOC_GOT_RVA, (void **)&got) && got)
    {
        mem = ((void *(*)(size_t))got)(size);
        if (rcl_msg_ok(mem, size))
        {
            memset(mem, 0, size);
            return mem;
        }
    }
    return nullptr;
}

void *rcl_manager(void)
{
    if (!rcl_scene_object || !rcl_scene_live())
    {
        return nullptr;
    }
    static uintptr_t c_battle = 0;
    uintptr_t battleFn = rcl_callable_cached(&c_battle, RVA_BATTLEMODE_GETINSTANCE);
    void *battle = nullptr;
    void *mgr = nullptr;
    if (!battleFn)
    {
        return nullptr;
    }
    battle = ((void *(*)(void))battleFn)();
    if (!battle)
    {
        return nullptr;
    }
    if (!rcl_read_ptr((uintptr_t)battle + RCL_MGR_OFF, &mgr) || !mgr)
    {
        return nullptr;
    }
    return mgr;
}

int rcl_pred_set(int x, int y)
{
    uintptr_t setFn = 0;
    uintptr_t ctrl = 0;

    if (!RCL_PRED_SET || !rcl_coord_ok)
    {
        return 0;
    }

    {
        static uintptr_t c_set = 0;

        setFn = rcl_callable_cached(&c_set, RCL_MOVE_RVA);
    }

    ctrl = rcl_controller();
    if (!setFn || !ctrl)
    {
        return 0;
    }


    ((void (*)(uintptr_t, int, int, int))setFn)(ctrl, x, y, RCL_PRED_FLAG);

    return 1;
}

int rcl_move_to(int32_t x, int32_t y, float ox, float oy)
{
    uintptr_t fn = 0;
    uintptr_t ctrl = 0;

    (void)ox;
    (void)oy;

    if (!RCL_MOVE_ON || rcl_dead)
    {
        return 0;
    }
    if (x < -200000 || x > 200000 || y < -200000 || y > 200000)
    {
        return 0;
    }

    {
        static uintptr_t c_move = 0;

        fn = rcl_callable_cached(&c_move, RCL_MOVE_RVA);
    }

    ctrl = rcl_controller();
    if (!fn || !ctrl)
    {
        return 0;
    }


    ((void (*)(void *, int, int, int))fn)((void *)ctrl, (int)x, (int)y, 1);

    return 1;
}
