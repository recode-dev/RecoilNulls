#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <atomic>
#include "../features/autododge.h"
#include "../utils/walls.h"
#include "objc.h"
#include "offsets.h"
#include "scan.h"
#include "scstring.h"
#include <frida-gum.h>
const char* image_name = "Nulls Brawl";
BOOL setup_done = NO;
__thread BOOL inside_hook = NO;
uintptr_t addr_dataname = 0;

namespace
{
static std::atomic<uint64_t> autododge_scan_counters[SCAN_AD_REASON_COUNT]{};
}

void autododge_scanner_event(uint32_t reason)
{
    if (reason > 0 && reason < SCAN_AD_REASON_COUNT)
        autododge_scan_counters[reason].fetch_add(1, std::memory_order_relaxed);
}

void autododge_scanner_report()
{
    static uint64_t lastReportMs = 0;
    static uint64_t totals[SCAN_AD_REASON_COUNT]{};
    uint64_t interval[SCAN_AD_REASON_COUNT]{};
    uint64_t sum = 0;
    const uint64_t now = us() / 1000;
    if (lastReportMs && now - lastReportMs < 1000) return;
    for (uint32_t i = 1; i < SCAN_AD_REASON_COUNT; i++)
    {
        interval[i] =
            autododge_scan_counters[i].exchange(0, std::memory_order_relaxed);
        totals[i] += interval[i];
        sum += interval[i];
    }
    if (!sum) return;
    lastReportMs = now;
    char out[1200];
    int n = snprintf(out, sizeof(out), "interval(ms=1000)");
    for (uint32_t i = 1;
         i < SCAN_AD_REASON_COUNT && n > 0 && n < static_cast<int>(sizeof(out));
         i++)
    {
        const uint64_t value = interval[i];
        const char* label = "unknown";
        switch (i)
        {
        case SCAN_AD_INVALID_SNAPSHOT:
            label = "invalidSnapshot";
            break;
        case SCAN_AD_BAD_DATA_POINTER:
            label = "badDataPointer";
            break;
        case SCAN_AD_DATA_READ_FAIL:
            label = "dataReadFail";
            break;
        case SCAN_AD_BAD_VTABLE:
            label = "badVtable";
            break;
        case SCAN_AD_NOT_PROJECTILE_VTABLE:
            label = "otherVtable";
            break;
        case SCAN_AD_ZERO_PID:
            label = "pidZero";
            break;
        case SCAN_AD_OWN_TEAM:
            label = "ownTeam";
            break;
        case SCAN_AD_DEAD:
            label = "dead";
            break;
        case SCAN_AD_INDIRECT_READ_FAIL:
            label = "indirectReadFail";
            break;
        case SCAN_AD_OUT_OF_RANGE:
            label = "outOfRange";
            break;
        case SCAN_AD_BEHIND:
            label = "behind";
            break;
        case SCAN_AD_WALL_BLOCKED:
            label = "wallBlocked";
            break;
        case SCAN_AD_BLOB:
            label = "blob";
            break;
        case SCAN_AD_NO_VELOCITY:
            label = "noVelocity";
            break;
        case SCAN_AD_THREAT_CANDIDATE:
            label = "threatCandidates";
            break;
        case SCAN_AD_CACHE_REBUILT:
            label = "cacheEvicted";
            break;
        case SCAN_AD_CACHE_INDEX_REPAIRED:
            label = "cacheIndexRepaired";
            break;
        case SCAN_AD_MAX_THREATS:
            label = "maxThreatCap";
            break;
        default:
            break;
        }
        n += snprintf(out + n,
                      sizeof(out) - static_cast<size_t>(n),
                      " %s=%llu/%llu",
                      label,
                      static_cast<unsigned long long>(value),
                      static_cast<unsigned long long>(totals[i]));
    }
    if (n > 0) autododge_scanner_log(now, "%s", out);
}
BOOL query_region(uintptr_t address,
                  vm_prot_t* protection,
                  vm_prot_t* maxProtection,
                  mach_vm_size_t* regionSize,
                  uintptr_t* regionStart)
{
    GumPageProtection pageProtection = GUM_PAGE_NO_ACCESS;
    guint pageSize = 0;
    if (!address || !gum_live)
    {
        return NO;
    }
    pageSize = gum_query_page_size();
    if (!pageSize)
    {
        return NO;
    }
    if (!gum_memory_query_protection((gconstpointer)address, &pageProtection))
    {
        return NO;
    }
    if (protection)
    {
        *protection = static_cast<vm_prot_t>(pageProtection);
    }
    if (maxProtection)
    {
        *maxProtection = static_cast<vm_prot_t>(pageProtection);
    }
    if (regionSize)
    {
        *regionSize = static_cast<mach_vm_size_t>(pageSize);
    }
    if (regionStart)
    {
        *regionStart = address & ~(static_cast<uintptr_t>(pageSize) - 1);
    }
    return YES;
}
BOOL addr_writable(uintptr_t address, size_t length)
{
    return writable(address, length);
}
inline constexpr int PAGE_CACHE_SLOTS = 256;
static __thread uintptr_t page_cache_key[PAGE_CACHE_SLOTS];
static __thread uint8_t page_cache_ok[PAGE_CACHE_SLOTS];
static void read_cache_reset()
{
    for (int i = 0; i < PAGE_CACHE_SLOTS; i++) page_cache_key[i] = 0;
}
BOOL read_bytes(uintptr_t address, void* out, size_t length)
{
    GumPageProtection protection = GUM_PAGE_NO_ACCESS;
    guint pageSize = 0;
    uintptr_t end = 0;
    uintptr_t cursor = 0;
    if (!out || !length)
    {
        return NO;
    }
    if (!address)
    {
        return NO;
    }
    if (!gum_live)
    {
        return NO;
    }
    end = address + length;
    if (end <= address)
    {
        return NO;
    }
    pageSize = gum_query_page_size();
    if (!pageSize)
    {
        return NO;
    }
    cursor = address;
    for (int guard = 0; cursor < end && guard < 64; guard++)
    {
        uintptr_t page = cursor & ~(static_cast<uintptr_t>(pageSize) - 1);
        int slot = (int)((page >> 12) & (PAGE_CACHE_SLOTS - 1));
        uint8_t ok = 0;
        if (page_cache_key[slot] == page)
        {
            ok = page_cache_ok[slot];
        }
        else
        {
            if (gum_memory_query_protection((gconstpointer)page, &protection))
                ok = (protection & GUM_PAGE_READ) ? 1 : 0;
            page_cache_key[slot] = page;
            page_cache_ok[slot] = ok;
        }
        if (!ok)
        {
            return NO;
        }
        cursor = page + static_cast<uintptr_t>(pageSize);
    }
    if (cursor < end)
    {
        return NO;
    }
    memcpy(out, (const void*)address, length);
    return YES;
}
BOOL pointer_plausible(uintptr_t value)
{
    if (value < 0x10000ULL || value >= 0x00007fffffffffffULL)
    {
        return NO;
    }
    if (value & 7)
    {
        return NO;
    }
    return YES;
}
BOOL read_int(uintptr_t address, int32_t* out)
{
    if (!out)
    {
        return NO;
    }
    if (address & 3)
    {
        return NO;
    }
    return read_bytes(address, out, 4);
}
BOOL writable(uintptr_t address, size_t length)
{
    uintptr_t end = address + length;
    uintptr_t cursor = address;
    guint pageSize = 0;
    if (!address || !length || !gum_live)
    {
        return NO;
    }
    if (end < address)
    {
        return NO;
    }
    pageSize = gum_query_page_size();
    if (!pageSize)
    {
        return NO;
    }
    for (int guard = 0; cursor < end && guard < 64; guard++)
    {
        GumPageProtection protection = GUM_PAGE_NO_ACCESS;
        uintptr_t next = 0;
        if (!gum_memory_query_protection((gconstpointer)cursor, &protection))
        {
            return NO;
        }
        if ((protection & GUM_PAGE_WRITE) == 0)
        {
            return NO;
        }
        next = (cursor & ~(static_cast<uintptr_t>(pageSize) - 1)) +
               static_cast<uintptr_t>(pageSize);
        if (next <= cursor)
        {
            return NO;
        }
        cursor = next;
    }
    return cursor >= end;
}
BOOL write_bytes(uintptr_t address, const void* src, size_t length)
{
    if (!src || !length)
    {
        return NO;
    }
    if (!address)
    {
        return NO;
    }
    if (!gum_live)
    {
        return NO;
    }
    return gum_memory_write((gpointer)address,
                            static_cast<const guint8*>(src),
                            static_cast<gsize>(length))
               ? YES
               : NO;
}
BOOL read_ptr(uintptr_t address, void** out)
{
    if (!out)
    {
        return NO;
    }
    if (address & 7)
    {
        return NO;
    }
    return read_bytes(address, out, sizeof(void*));
}
void* read_global_ptr(uintptr_t rva)
{
    if (!base || !rva)
    {
        return nullptr;
    }
    void* value = nullptr;
    if (!read_ptr(base + rva, &value))
    {
        return nullptr;
    }
    return value;
}
uintptr_t callable(uintptr_t rva)
{
    if (!base || !rva)
    {
        return 0;
    }
    uintptr_t address = base + rva;
    if (address < base || address >= base + 0x2000000ULL)
    {
        return 0;
    }
    return address;
}
int gum_live = 0;
static BOOL module_base_by_name(const char* name, uintptr_t* out_base)
{
    GumModule* module = nullptr;
    const GumMemoryRange* range = nullptr;
    uintptr_t found = 0;
    if (!name || !name[0] || !out_base)
    {
        return NO;
    }
    module = gum_process_find_module_by_name(name);
    if (!module)
    {
        return NO;
    }
    range = gum_module_get_range(module);
    if (range)
    {
        found = static_cast<uintptr_t>(range->base_address);
    }
    g_object_unref(module);
    if (!found)
    {
        return NO;
    }
    *out_base = found;
    return YES;
}
BOOL find_game_image(uintptr_t* out_base)
{
    static int gum_ready = 0;
    uintptr_t found = 0;
    if (!out_base)
    {
        return NO;
    }
    if (!gum_ready)
    {
        gum_process_set_teardown_requirement(GUM_TEARDOWN_REQUIREMENT_MINIMAL);
        gum_init_embedded();
        gum_ready = 1;
        gum_live = 1;
    }
    if (!module_base_by_name(image_name, &found))
    {
        return NO;
    }
    *out_base = found;
    return YES;
}
void poll_for_game(int tick)
{
    if (setup_done)
    {
        return;
    }
    if (tick > 1200)
    {
        return;
    }
    uintptr_t imageBase = 0;
    if (find_game_image(&imageBase))
    {
        base = imageBase;
        setup();
        return;
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 static_cast<int64_t>(0.5 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(),
                   ^{ poll_for_game(tick + 1); });
}
uintptr_t base = 0;
uintptr_t manager_ptr = 0;
float own_speed = 720.0f;
float own_vel_x = 0.0f;
float own_vel_y = 0.0f;
uint32_t own_hp = 0;
uint64_t us()
{
    return static_cast<uint64_t>(g_get_monotonic_time());
}
using fn_self_t = uintptr_t (*)(uintptr_t);
static uintptr_t call(uintptr_t fn, uintptr_t self)
{
    if (!fn || !base || fn < base || fn >= base + 0x2000000ULL || !self)
        return 0;
    return ((fn_self_t)fn)(self);
}
uintptr_t manager_array_ptr = 0;
int32_t manager_count_v = 0;
uintptr_t obj_snapshot[COUNT_MAX];
uint8_t obj_block[COUNT_MAX][OBJ_BLOCK_SIZE];
uint8_t obj_valid[COUNT_MAX];
int obj_count = 0;
int32_t own_x = 0;
int32_t own_y = 0;
int32_t own_index_v = -1;
int own_ok_hp = 0;
static int battle_state()
{
    uintptr_t slot = (uintptr_t)read_global_ptr(STATE_RVA);
    int32_t state = -1;
    if (!slot)
    {
        return -1;
    }
    if (!read_int(slot + static_cast<uintptr_t>(STATE_ENUM_OFF), &state))
    {
        return -1;
    }
    return state;
}
static int battle_ticks = 0;
static int32_t last_pos_x = 0;
static int32_t last_pos_y = 0;
static uint64_t last_pos_us = 0;
static void world_reset()
{
    own_elem = 0;
    own_x = 0;
    own_y = 0;
    own_hp = 0;
    own_ok_hp = 0;
    dead = 1;
    coord_ok = 0;
    manager_ptr = 0;
    manager_array_ptr = 0;
    manager_count_v = 0;
    obj_count = 0;
    scene_object = 0;
    battle_ticks = 0;
    own_index_v = -1;
    own_team_b = -1;
    own_vel_x = 0.0f;
    own_vel_y = 0.0f;
    last_pos_us = 0;
    wall_drop();
    autododge_reset();
}
static void track_own_speed()
{
    uint64_t now_us = us();
    if (last_pos_us && now_us > last_pos_us)
    {
        float dt = (float)(now_us - last_pos_us) / 1000000.0f;
        if (dt >= 0.06f && dt < 0.25f)
        {
            float dx = (float)(own_x - last_pos_x);
            float dy = (float)(own_y - last_pos_y);
            float sp = sqrtf(dx * dx + dy * dy) / dt;
            if (sp >= 300.0f && sp <= 1100.0f)
            {
                if (sp > own_speed)
                    own_speed = sp;
                else
                    own_speed = own_speed * 0.99f + sp * 0.01f;
                own_vel_x = own_vel_x * 0.5f + (dx / dt) * 0.5f;
                own_vel_y = own_vel_y * 0.5f + (dy / dt) * 0.5f;
            }
            last_pos_x = own_x;
            last_pos_y = own_y;
            last_pos_us = now_us;
        }
        return;
    }
    last_pos_x = own_x;
    last_pos_y = own_y;
    last_pos_us = now_us;
}

static void tilemap_tick(uintptr_t clientPtr)
{
    static uint64_t last_us = 0;
    uint64_t now_us = us();
    uintptr_t tileMapFn = 0;
    uintptr_t tileMap = 0;
    if (last_us && now_us - last_us < 2000000ULL)
    {
        return;
    }
    last_us = now_us;
    tileMapFn = callable(RVA_LOGICBATTLEMODECLIENT_GETTILEMAP);
    tileMap = tileMapFn ? call(tileMapFn, clientPtr) : 0;
    if (pointer_plausible(tileMap))
    {
        wall_bind(tileMap);
    }
}

void scan_world()
{
    void* value = nullptr;
    uintptr_t clientPtr = 0;
    uintptr_t mgrPtr = 0;
    uintptr_t array = 0;
    uintptr_t own = 0;
    int32_t count = 0;
    int32_t state = -1;
    int32_t x = 0;
    int32_t y = 0;
    int32_t hp = 0;
    uintptr_t slot = 0;
    void* sceneValue = nullptr;
    if (!base) return;
    state = battle_state();
    slot = (uintptr_t)read_global_ptr(STATE_RVA);
    scene_object = 0;
    if (slot &&
        read_ptr(slot + static_cast<uintptr_t>(SCENE_OFF), &sceneValue) &&
        sceneValue)
    {
        scene_object = (uintptr_t)sceneValue;
    }
    if (state != 5)
    {
        world_reset();
        return;
    }
    battle_ticks++;
    clientPtr = controller();
    if (!clientPtr)
    {
        world_reset();
        return;
    }
    if (!read_ptr(clientPtr + static_cast<uintptr_t>(MODE_MANAGER_OFF),
                  &value) ||
        !value)
    {
        world_reset();
        return;
    }
    mgrPtr = (uintptr_t)value;
    manager_ptr = mgrPtr;
    if (!read_int(mgrPtr + static_cast<uintptr_t>(MGR_COUNT_OFF), &count) ||
        count <= 0 || count > OBJECT_COUNT_MAX)
    {
        world_reset();
        return;
    }
    if (!read_ptr(mgrPtr + static_cast<uintptr_t>(MGR_ARRAY_OFF), &value) ||
        !value)
    {
        world_reset();
        return;
    }
    array = (uintptr_t)value;
    if (count > COUNT_MAX) count = COUNT_MAX;
    if (!read_bytes(array,
                    obj_snapshot,
                    static_cast<size_t>(count) * sizeof(uintptr_t)))
    {
        world_reset();
        return;
    }
    manager_array_ptr = array;
    manager_count_v = count;
    obj_count = count;
    for (int i = 0; i < count; i++)
    {
        uintptr_t objPtr = obj_snapshot[i];
        obj_valid[i] = (objPtr && pointer_plausible(objPtr) &&
                        read_bytes(objPtr, obj_block[i], OBJ_BLOCK_SIZE))
                           ? 1
                           : 0;
        if (!obj_valid[i])
        {
            autododge_scanner_event(SCAN_AD_INVALID_SNAPSHOT);
        }
    }
    {
        uintptr_t ownFn = callable(RVA_LOGICBATTLEMODECLIENT_GETOWNCHARACTER);
        uintptr_t ownByFn = ownFn ? call(ownFn, clientPtr) : 0;
        own = (ownByFn && pointer_plausible(ownByFn)) ? ownByFn : 0;
    }
    if (!own)
    {
        own_elem = 0;
        own_ok_hp = 0;
        own_hp = 0;
        dead = 1;
        coord_ok = 0;
        return;
    }
    own_elem = own;
    if (read_int(own + static_cast<uintptr_t>(OBJ_X_OFF), &x) &&
        read_int(own + static_cast<uintptr_t>(OBJ_Y_OFF), &y))
    {
        own_x = x;
        own_y = y;
        coord_ok = 1;
    }
    if (read_int(own + static_cast<uintptr_t>(OBJ_HP_OFF), &hp) && hp >= 0 &&
        hp < 1000000)
    {
        own_ok_hp = 1;
        own_hp = static_cast<uint32_t>(hp);
    }
    else
    {
        own_ok_hp = 0;
        own_hp = 0;
    }
    dead = (own_ok_hp && own_hp == 0) ? 1 : 0;
    own_index_v = static_cast<int32_t>(
        call(callable(RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERINDEX), clientPtr));
    own_team_b = static_cast<int32_t>(
        call(callable(RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERTEAM), clientPtr));
    track_own_speed();
    tilemap_tick(clientPtr);
    wall_maybe_refresh(static_cast<uint64_t>(g_get_real_time() / 1000));
}

uint32_t workload_ticks = 0;
void run_workload()
{
    workload_ticks++;
    scan_world();
    run_autododge();
    diag_tick();
}
void run_autododge()
{
    if (in_move)
    {
        return;
    }
    in_move = 1;
    autododge();
    in_move = 0;
}
static constexpr uint64_t SCAN_PERIOD_US = 8000ULL;
static constexpr uint64_t FRAME_DEDUPE_US = 4000ULL;
static uint64_t g_last_scan_us = 0;
static void scan_frame()
{
    const uint64_t now = us();
    if (inside_hook)
    {
        return;
    }
    if (now - g_last_scan_us < FRAME_DEDUPE_US)
    {
        return;
    }
    g_last_scan_us = now;
    inside_hook = YES;
    read_cache_reset();
    run_workload();
    inside_hook = NO;
}
static dispatch_source_t scan_timer = nullptr;
static void start_scan_timer()
{
    if (scan_timer) return;
    scan_timer = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    if (!scan_timer) return;
    dispatch_source_set_timer(
        scan_timer,
        dispatch_time(DISPATCH_TIME_NOW, 0),
        SCAN_PERIOD_US * NSEC_PER_USEC,
        NSEC_PER_MSEC);
    dispatch_source_set_event_handler(scan_timer, ^{ scan_frame(); });
    dispatch_resume(scan_timer);
}
void setup()
{
    if (setup_done)
    {
        return;
    }
    resolve_addresses();
    int armed = 0;
    armed += objc_arm(base, "MetalView", "render", scan_frame);
    armed += objc_arm(base, "NullView", "render", scan_frame);
    start_scan_timer();
    setup_done = YES;
}
void resolve_addresses()
{
    if (!base)
    {
        return;
    }
    addr_dataname = callable(RVA_LOGICDATA_GETNAME);
}
int coord_ok = 0;
int dead = 0;
int scene_live()
{
    uintptr_t slot = (uintptr_t)read_global_ptr(STATE_RVA);
    void* value = nullptr;
    int32_t state = -1;
    if (!scene_object || !slot)
    {
        return 0;
    }
    if (!read_int(slot + STATE_ENUM_OFF, &state) || state != 5)
    {
        return 0;
    }
    if (!read_ptr(slot + SCENE_OFF, &value) || !value)
    {
        return 0;
    }
    return (uintptr_t)value == scene_object;
}
uintptr_t client()
{
    void* clientPtr = nullptr;
    if (!scene_object || !scene_live())
    {
        return 0;
    }
    if (!read_ptr(scene_object + CLIENT_OFF, &clientPtr))
    {
        return 0;
    }
    return (uintptr_t)clientPtr;
}
uintptr_t controller()
{
    return client();
}
uintptr_t scene_object = 0;
int own_team_b = -1;
uintptr_t own_elem = 0;
