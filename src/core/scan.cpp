#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <cstdint>
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
const char* image_names[2] = {"Nulls Brawl", nullptr};
BOOL setup_done = NO;
__thread BOOL inside_hook = NO;
uintptr_t addr_getinstance = 0;
uintptr_t addr_getownchar = 0;
uintptr_t addr_getteam = 0;
uintptr_t addr_getx = 0;
uintptr_t addr_gety = 0;
uintptr_t addr_getownidx = 0;
uintptr_t addr_movedto = 0;
uintptr_t addr_getdata = 0;
uintptr_t addr_getgid = 0;
uintptr_t addr_charspeed = 0;
uintptr_t addr_charradius = 0;
uintptr_t addr_projbeam = 0;
uintptr_t addr_projearly = 0;
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
    // Validate readability at page granularity instead of
    // `gum_memory_is_readable`, which allocates a buffer, copies the whole
    // range via `gum_memory_read` and then discards it — so the same bytes
    // would be read twice per call. A protection query is a single region
    // lookup with no allocation or copy.
    cursor = address;
    for (int guard = 0; cursor < end && guard < 64; guard++)
    {
        if (!gum_memory_query_protection((gconstpointer)cursor, &protection))
        {
            return NO;
        }
        if ((protection & GUM_PAGE_READ) == 0)
        {
            return NO;
        }
        cursor = (cursor & ~(static_cast<uintptr_t>(pageSize) - 1)) +
                 static_cast<uintptr_t>(pageSize);
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
BOOL read_byte(uintptr_t address, uint8_t* out)
{
    return read_bytes(address, out, 1);
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
    if (!module_base_by_name(image_names[0], &found))
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
uint64_t ticks_a = 0;
float own_speed = 720.0f;
float own_r = 60.0f;
uint32_t own_hp = 0;
uint64_t us()
{
    return static_cast<uint64_t>(g_get_monotonic_time());
}
using fn_self_t = uintptr_t (*)(uintptr_t);
using fn_self_int_t = int32_t (*)(uintptr_t);
static uintptr_t call(uintptr_t fn, uintptr_t self)
{
    if (!fn || !base || fn < base || fn >= base + 0x2000000ULL || !self)
        return 0;
    return ((fn_self_t)fn)(self);
}
static int32_t call_int(uintptr_t fn, uintptr_t self)
{
    if (!fn || !base || fn < base || fn >= base + 0x2000000ULL || !self)
        return 0;
    return ((fn_self_int_t)fn)(self);
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
    wall_drop();
    autododge_reset();
}
void scan_world()
{
    uintptr_t clientPtr = 0;
    uintptr_t own = 0;
    uintptr_t ownData = 0;
    uintptr_t mgrPtr = 0;
    uintptr_t array = 0;
    void* value = nullptr;
    int32_t count = 0;
    int32_t ownIndex = -1;
    int32_t ownTeam = -1;
    int32_t x = 0;
    int32_t y = 0;
    int32_t hp = 0;
    int32_t state = -1;
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
    own = addr_getownchar ? call(addr_getownchar, clientPtr) : 0;
    if (base)
    {
        uintptr_t tileMapFn = callable(RVA_LOGICBATTLEMODECLIENT_GETTILEMAP);
        uintptr_t tileMap = tileMapFn ? call(tileMapFn, clientPtr) : 0;
        if (pointer_plausible(tileMap))
        {
            wall_bind(tileMap);
            wall_maybe_refresh(static_cast<uint64_t>(g_get_real_time() / 1000));
        }
    }
    if (!pointer_plausible(own))
    {
        own = 0;
    }
    if (clientPtr)
    {
        int32_t teamFromClient = -1;
        int32_t indexFromClient = -1;
        if (read_int(clientPtr + static_cast<uintptr_t>(CLIENT_OWNTEAM_OFF),
                     &teamFromClient))
            ownTeam = teamFromClient;
        if (read_int(clientPtr + static_cast<uintptr_t>(CLIENT_OWNINDEX_OFF),
                     &indexFromClient))
            ownIndex = indexFromClient;
    }
    if (own)
    {
        own_elem = own;
    }
    if (ownTeam >= 0 && ownTeam <= 15)
    {
        own_team_b = ownTeam;
    }
    if (ownIndex >= 0 && ownIndex < 64)
    {
        own_index_v = ownIndex;
    }
    if (own)
    {
        if (read_int(own + static_cast<uintptr_t>(OBJ_X_OFF), &x) &&
            read_int(own + static_cast<uintptr_t>(OBJ_Y_OFF), &y))
        {
            own_x = x;
            own_y = y;
        }
        if (read_int(own + static_cast<uintptr_t>(OBJ_HP_OFF), &hp) &&
            hp >= 0 && hp < 1000000)
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
        coord_ok = 1;
    }
    else
    {
        dead = 1;
    }
    if (own)
    {
        int32_t probe = 0;
        ownData = addr_getdata ? call(addr_getdata, own) : 0;
        if (pointer_plausible(ownData) && read_int(ownData, &probe))
        {
            int32_t radius = 0;
            int32_t speed = 0;
            radius = call_int(addr_charradius, ownData);
            speed = call_int(addr_charspeed, ownData);
            own_r = static_cast<float>(radius);
            if (speed >= 300 && speed <= 3000)
                own_speed = static_cast<float>(speed);
        }
    }
    if (!read_ptr(clientPtr + static_cast<uintptr_t>(MODE_MANAGER_OFF),
                  &value) ||
        !value)
    {
        manager_ptr = 0;
        manager_array_ptr = 0;
        manager_count_v = 0;
        obj_count = 0;
        return;
    }
    mgrPtr = (uintptr_t)value;
    manager_ptr = mgrPtr;
    if (!read_int(mgrPtr + static_cast<uintptr_t>(MGR_COUNT_OFF), &count) ||
        count <= 0 || count > OBJECT_COUNT_MAX)
    {
        manager_array_ptr = 0;
        manager_count_v = 0;
        obj_count = 0;
        return;
    }
    if (!read_ptr(mgrPtr + static_cast<uintptr_t>(MGR_ARRAY_OFF), &value) ||
        !value)
    {
        manager_array_ptr = 0;
        manager_count_v = 0;
        obj_count = 0;
        return;
    }
    array = (uintptr_t)value;
    if (count > COUNT_MAX) count = COUNT_MAX;
    if (!read_bytes(array,
                    obj_snapshot,
                    static_cast<size_t>(count) * sizeof(uintptr_t)))
    {
        manager_array_ptr = 0;
        manager_count_v = 0;
        obj_count = 0;
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
            continue;
        }
        int32_t team = 0;
        int32_t hpNow = 0;
        memcpy(&team, obj_block[i] + OBJ_TEAM_OFF, sizeof(team));
        memcpy(&hpNow, obj_block[i] + OBJ_HP_OFF, sizeof(hpNow));
        if (team == own_team_b || hpNow <= 0) continue;
    }
    ticks_a++;
}
void run_workload()
{
    wall_maybe_refresh(static_cast<uint64_t>(g_get_real_time() / 1000));
    scan_world();
    run_autododge();
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
// Client updates its in-memory world at 60 Hz (16.67 ms) even though the server
// simulates at 30 Hz — projectiles move every ~16 ms in client memory
// (autododge.cpp: earlyTicks * 16, SEND_MIN_MS = 16). Scan at that cadence so
// we catch every client update, but never busy-loop.
static constexpr uint64_t SCAN_PERIOD_US = 16667ULL;
static uint64_t g_last_scan_us = 0;
static void scan_frame()
{
    if (inside_hook)
    {
        return;
    }
    const uint64_t now = us();
    if (now - g_last_scan_us < SCAN_PERIOD_US)
    {
        return;
    }
    g_last_scan_us = now;
    inside_hook = YES;
    run_workload();
    inside_hook = NO;
}
static void schedule_frame();
static void frame_tick()
{
    scan_frame();
    schedule_frame();
}
static void schedule_frame()
{
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      static_cast<int64_t>(SCAN_PERIOD_US * NSEC_PER_USEC)),
        dispatch_get_main_queue(),
        ^{ frame_tick(); });
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
    if (!armed)
    {
        schedule_frame();
    }
    setup_done = YES;
}
void resolve_addresses()
{
    if (!base)
    {
        return;
    }
    addr_getinstance = callable(RVA_BATTLEMODE_GETINSTANCE);
    addr_getownchar = callable(RVA_LOGICBATTLEMODECLIENT_GETOWNCHARACTER);
    addr_getteam = callable(RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERTEAM);
    addr_getx = callable(RVA_LOGICGAMEOBJECTCLIENT_GETX);
    addr_gety = callable(RVA_LOGICGAMEOBJECTCLIENT_GETY);
    addr_getownidx = callable(RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERINDEX);
    addr_movedto = callable(MOVE_RVA);
    addr_getdata = callable(RVA_LOGICGAMEOBJECTCLIENT_GETDATA);
    addr_getgid = callable(RVA_LOGICGAMEOBJECTCLIENT_GETGLOBALID);
    addr_charspeed = callable(RVA_LOGICCHARACTERDATA_GETSPEED);
    addr_charradius = callable(RVA_LOGICCHARACTERDATA_GETCOLLISIONRADIUS);
    addr_projbeam = callable(RVA_LOGICPROJECTILEDATA_ISBEAM);
    addr_projearly = callable(RVA_LOGICPROJECTILEDATA_GETNUMEARLYTICKS);
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
