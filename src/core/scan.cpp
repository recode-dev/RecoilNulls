#include <dispatch/dispatch.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/vm_map.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if __has_include(<ptrauth.h>)
#include <ptrauth.h>
#endif
#include "../features/autododge.h"
#include "../utils/crypto.h"
#include "../utils/walls.h"
#include "objc.h"
#include "offsets.h"
#include "scan.h"
#include <frida-gum.h>
const char* image_names[2] = {"Nulls Brawl", nullptr};
constexpr float CASTING_RANGE_SCALE = 100.0f;
BOOL setup_done = NO;
__thread BOOL inside_hook = NO;
uintptr_t addr_getinstance = 0;
uintptr_t addr_getownchar = 0;
uintptr_t addr_getteam = 0;
uintptr_t addr_getx = 0;
uintptr_t addr_gety = 0;
uintptr_t addr_battlescreen = 0;
uintptr_t addr_getownidx = 0;
uintptr_t addr_movedto = 0;
uintptr_t addr_getdata = 0;
uintptr_t addr_getgid = 0;
uintptr_t addr_charspeed = 0;
uintptr_t addr_charradius = 0;
uintptr_t addr_projspeed = 0;
uintptr_t addr_projradius = 0;
uintptr_t addr_projbeam = 0;
uintptr_t addr_castrange = 0;
uintptr_t addr_skillproj = 0;
uintptr_t addr_weaponskill = 0;
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
        *protection = (vm_prot_t)pageProtection;
    }
    if (maxProtection)
    {
        *maxProtection = (vm_prot_t)pageProtection;
    }
    if (regionSize)
    {
        *regionSize = (mach_vm_size_t)pageSize;
    }
    if (regionStart)
    {
        *regionStart = address & ~((uintptr_t)pageSize - 1);
    }
    return YES;
}
BOOL addr_writable(uintptr_t address, size_t length)
{
    return writable(address, length);
}
BOOL read_bytes(uintptr_t address, void* out, size_t length)
{
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
    if (!gum_memory_is_readable((gconstpointer)address, (gsize)length))
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
        next = (cursor & ~((uintptr_t)pageSize - 1)) + (uintptr_t)pageSize;
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
                            (const guint8*)src,
                            (gsize)length)
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
BOOL copy(uintptr_t source, void* destination, size_t length)
{
    if (!source || !destination || !length)
    {
        return NO;
    }
    if (!gum_live)
    {
        return NO;
    }
    if (!gum_memory_is_readable((gconstpointer)source, (gsize)length))
    {
        return NO;
    }
    memcpy(destination, (const void*)source, length);
    return YES;
}
void stage_note(const char* what);
int gum_live = 0;
static BOOL module_base_by_name(const char* name, uintptr_t* out_base)
{
    GumModule* module = NULL;
    const GumMemoryRange* range = NULL;
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
        found = (uintptr_t)range->base_address;
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
        stage_note("stage gum_init begin\n");
        gum_process_set_teardown_requirement(GUM_TEARDOWN_REQUIREMENT_MINIMAL);
        gum_init_embedded();
        gum_ready = 1;
        gum_live = 1;
        stage_note("stage gum_init end\n");
    }
    stage_note("stage module_find begin\n");
    if (!module_base_by_name(image_names[0], &found))
    {
        return NO;
    }
    stage_note("stage module_find end found=1\n");
    *out_base = found;
    return YES;
}
uintptr_t owner = 0;
int wired = 0;
dispatch_source_t scan_timer = nullptr;
const char* image_segment_name(uintptr_t value)
{
    if (!base || !value)
    {
        return nullptr;
    }
    if (!addr_readable(base, sizeof(struct mach_header_64)))
    {
        return nullptr;
    }
    const struct mach_header_64* header = (const struct mach_header_64*)base;
    if (header->magic != MH_MAGIC_64)
    {
        return nullptr;
    }
    const uint8_t* cursor = (const uint8_t*)(header + 1);
    const uint8_t* limit = cursor + header->sizeofcmds;
    uintptr_t slide = image_slide(base);
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (cursor + sizeof(struct load_command) > limit)
        {
            return nullptr;
        }
        const struct load_command* command = (const struct load_command*)cursor;
        if (command->cmdsize < sizeof(struct load_command))
        {
            return nullptr;
        }
        if (cursor + command->cmdsize > limit)
        {
            return nullptr;
        }
        if (command->cmd == LC_SEGMENT_64 &&
            command->cmdsize >= sizeof(struct segment_command_64))
        {
            const struct segment_command_64* segment =
                (const struct segment_command_64*)command;
            if (segment->vmsize)
            {
                uintptr_t start = slide + (uintptr_t)segment->vmaddr;
                if (value >= start &&
                    value < (start + (uintptr_t)segment->vmsize))
                {
                    return segment->segname;
                }
            }
        }
        cursor += command->cmdsize;
    }
    return nullptr;
}
BOOL heap_resident(uintptr_t value)
{
    if (!value)
    {
        return NO;
    }
    return image_segment_name(value) ? NO : YES;
}
BOOL manager_shape(uintptr_t mgrPtr)
{
    void* array = nullptr;
    void* probe = nullptr;
    int32_t count = 0;
    int32_t capacity = 0;
    if (!heap_resident(mgrPtr))
    {
        return NO;
    }
    if (!read_ptr(mgrPtr + MGR_ARRAY_OFF, &array))
    {
        return NO;
    }
    if (!read_int(mgrPtr + MGR_COUNT_OFF, &count))
    {
        return NO;
    }
    if (!read_int(mgrPtr + MGR_CAP_OFF, &capacity))
    {
        return NO;
    }
    if (count < 0 || count > 96)
    {
        return NO;
    }
    if (capacity < count || capacity > 4096)
    {
        return NO;
    }
    if (array && !heap_resident((uintptr_t)array))
    {
        return NO;
    }
    if (count > 0)
    {
        if (!array)
        {
            return NO;
        }
        if (!read_ptr((uintptr_t)array, &probe))
        {
            return NO;
        }
        if (!heap_resident((uintptr_t)probe))
        {
            return NO;
        }
        if (count > 1)
        {
            if (!read_ptr((uintptr_t)array +
                              (uintptr_t)(count - 1) * sizeof(void*),
                          &probe))
            {
                return NO;
            }
            if (!heap_resident((uintptr_t)probe))
            {
                return NO;
            }
        }
    }
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
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)),
        dispatch_get_main_queue(),
        ^{ poll_for_game(tick + 1); });
}
uintptr_t base = 0;
uintptr_t manager_ptr = 0;
uint64_t ticks_a = 0;
float own_speed = 720.0f;
float own_r = 0.0f;
uint32_t own_hp = 0;
uint64_t us(void)
{
    static mach_timebase_info_data_t tb;
    static int ready = 0;
    uint64_t t = 0;
    if (!ready)
    {
        ready = 1;
        mach_timebase_info(&tb);
    }
    t = mach_absolute_time();
    if (tb.denom == 0)
    {
        return t / 1000ULL;
    }
    return (t / (uint64_t)tb.denom) * (uint64_t)tb.numer / 1000ULL;
}
typedef uintptr_t (*fn1)(uintptr_t);
typedef int32_t (*fn1i)(uintptr_t);
typedef float (*fn1f)(uintptr_t);
static uintptr_t call1(uintptr_t fn, uintptr_t self)
{
    if (!fn || !self) return 0;
    return ((fn1)fn)(self);
}
static int32_t call1i(uintptr_t fn, uintptr_t self)
{
    if (!fn || !self) return 0;
    return ((fn1i)fn)(self);
}
static uintptr_t call1u(uintptr_t fn, uintptr_t self)
{
    if (!fn || !self) return 0;
    return ((uintptr_t(*)(uintptr_t))fn)(self);
}
static uintptr_t call2u(uintptr_t fn, uintptr_t self, int32_t arg)
{
    if (!fn || !self) return 0;
    return ((uintptr_t(*)(uintptr_t, int32_t))fn)(self, arg);
}
static BOOL obj_ok(uintptr_t p)
{
    uintptr_t vt = 0;
    if (!pointer_plausible(p) || !base) return NO;
    if (!read_ptr(p, (void**)&vt) || !pointer_plausible(vt)) return NO;
    uintptr_t rva = vt - base;
    return (rva >= DC_RVA_LO && rva < DC_RVA_LO + DC_RVA_SIZE) ? YES : NO;
}
uintptr_t manager_array_ptr = 0;
int32_t manager_count_v = 0;
int32_t own_x = 0;
int32_t own_y = 0;
int32_t own_index_v = -1;
int own_ok_hp = 0;
dbg_t dbg;
static int battle_state(void)
{
    uintptr_t slot = (uintptr_t)read_global_ptr(STATE_RVA);
    int32_t state = -1;
    if (!slot)
    {
        return -1;
    }
    if (!read_int(slot + (uintptr_t)STATE_ENUM_OFF, &state))
    {
        return -1;
    }
    return state;
}
static void world_reset(void)
{
    own_elem = 0;
    own_elem_scan = 0;
    own_x = 0;
    own_y = 0;
    own_hp = 0;
    own_ok_hp = 0;
    dead = 1;
    coord_ok = 0;
    manager_ptr = 0;
    manager_array_ptr = 0;
    manager_count_v = 0;
    scene_object = 0;
    wall_drop();
    autododge_reset();
}
void scan_world(void)
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
    dbg.stage = 0;
    if (!base) return;
    state = battle_state();
    if (state != prev_state)
    {
        state_note(state);
    }
    slot = (uintptr_t)read_global_ptr(STATE_RVA);
    scene_object = 0;
    if (slot && read_ptr(slot + (uintptr_t)SCENE_OFF, &sceneValue) &&
        sceneValue)
    {
        scene_object = (uintptr_t)sceneValue;
    }
    if (state != 5)
    {
        world_reset();
        dbg.stage = 1;
        return;
    }
    clientPtr = controller();
    if (!clientPtr)
    {
        world_reset();
        dbg.stage = 1;
        return;
    }
    own = addr_getownchar ? call1(addr_getownchar, clientPtr) : 0;
    if (base)
    {
        uintptr_t tileMapFn = callable(RVA_LOGICBATTLEMODECLIENT_GETTILEMAP);
        uintptr_t tileMap = tileMapFn ? call1(tileMapFn, clientPtr) : 0;
        if (pointer_plausible(tileMap))
        {
            wall_bind(tileMap);
            wall_maybe_refresh((uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0));
        }
    }
    if (!pointer_plausible(own))
    {
        own = 0;
        dbg.stage = 2;
    }
    if (addr_getteam)
    {
        ownTeam = call1i(addr_getteam, clientPtr);
    }
    if (addr_getownidx)
    {
        ownIndex = call1i(addr_getownidx, clientPtr);
    }
    if (own)
    {
        own_elem = own;
        own_elem_scan = own;
    }
    if (ownTeam >= 0 && ownTeam <= 15)
    {
        own_team_b = ownTeam;
        own_team_a = ownTeam;
        own_team_seen = 1;
    }
    if (ownIndex >= 0 && ownIndex < 64)
    {
        own_index_v = ownIndex;
    }
    if (own)
    {
        if (read_int(own + (uintptr_t)OBJ_X_OFF, &x) &&
            read_int(own + (uintptr_t)OBJ_Y_OFF, &y))
        {
            own_x = x;
            own_y = y;
        }
        if (read_int(own + (uintptr_t)OBJ_HP_OFF, &hp) && hp >= 0 &&
            hp < 1000000)
        {
            own_ok_hp = 1;
            own_hp = (uint32_t)hp;
        }
        else
        {
            own_ok_hp = 0;
            own_hp = 0;
        }
        dead = 0;
        coord_ok = 1;
    }
    else
    {
        dead = 1;
    }
    if (own)
    {
        int32_t probe = 0;
        ownData = addr_getdata ? call1(addr_getdata, own) : 0;
        if (pointer_plausible(ownData) && read_int(ownData, &probe))
        {
            int32_t radius =
                addr_charradius ? call1i(addr_charradius, ownData) : 0;
            int32_t speed =
                addr_charspeed ? call1i(addr_charspeed, ownData) : 0;
            if (radius >= 20 && radius <= 400) own_r = (float)radius;
            if (speed >= 300 && speed <= 3000) own_speed = (float)speed;
        }
    }
    if (!read_ptr(clientPtr + (uintptr_t)MODE_MANAGER_OFF, &value) || !value)
    {
        manager_ptr = 0;
        manager_array_ptr = 0;
        manager_count_v = 0;
        dbg.stage = 3;
        return;
    }
    mgrPtr = (uintptr_t)value;
    manager_ptr = mgrPtr;
    if (!read_ptr(mgrPtr + (uintptr_t)MGR_ARRAY_OFF, &value) || !value)
    {
        manager_array_ptr = 0;
        manager_count_v = 0;
        dbg.stage = 4;
        return;
    }
    array = (uintptr_t)value;
    if (!read_int(mgrPtr + (uintptr_t)MGR_COUNT_OFF, &count) || count <= 0)
    {
        manager_array_ptr = 0;
        manager_count_v = 0;
        dbg.stage = 5;
        return;
    }
    if (count > COUNT_MAX) count = COUNT_MAX;
    manager_array_ptr = array;
    manager_count_v = count;
    dbg.stage = 6;
    ticks_a++;
}
void run_workload(void)
{
    wall_maybe_refresh((uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0));
    scan_world();
    run_autododge();
}
void run_autododge(void)
{
    if (in_move)
    {
        return;
    }
    in_move = 1;
    autododge();
    in_move = 0;
}
static uint64_t work_us = 0;
static volatile int hot_pending = 0;
static dispatch_source_t hot_timer = nullptr;
static void objc_body(void)
{
    uint64_t now = 0;
    if (inside_hook)
    {
        return;
    }
    now = us();
    if (now && work_us && (now - work_us) < WORK_GAP_US)
    {
        return;
    }
    work_us = now;
    inside_hook = YES;
    run_workload();
    inside_hook = NO;
}
static void start_hot_timer(void)
{
    dispatch_queue_t queue = nullptr;
    dispatch_source_t timer = nullptr;
    uint64_t interval = 0;
    if (hot_timer)
    {
        return;
    }
    queue = dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    if (!timer)
    {
        return;
    }
    interval = (uint64_t)(HOT_INTERVAL_US * NSEC_PER_USEC);
    dispatch_source_set_timer(
        timer, dispatch_time(DISPATCH_TIME_NOW, 0), interval, 0);
    dispatch_source_set_event_handler(timer, ^{
      if (!__sync_bool_compare_and_swap(&hot_pending, 0, 1))
      {
          return;
      }
      dispatch_async(dispatch_get_main_queue(), ^{
        __sync_lock_release(&hot_pending);
        objc_body();
      });
    });
    dispatch_resume(timer);
    hot_timer = timer;
}
void setup(void)
{
    if (setup_done)
    {
        return;
    }
    setup_done = YES;
    resolve_addresses();
    objc_arm(base, "MetalView", "render", objc_body);
    objc_arm(base, "NullView", "render", objc_body);
    start_hot_timer();
}
int prev_state = -1;
void resolve_addresses(void)
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
    addr_battlescreen = base;
    addr_getownidx = callable(RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERINDEX);
    addr_movedto = callable(MOVE_RVA);
    addr_getdata = callable(RVA_LOGICGAMEOBJECTCLIENT_GETDATA);
    addr_getgid = callable(RVA_LOGICGAMEOBJECTCLIENT_GETGLOBALID);
    addr_charspeed = callable(RVA_LOGICCHARACTERDATA_GETSPEED);
    addr_charradius = callable(RVA_LOGICCHARACTERDATA_GETCOLLISIONRADIUS);
    addr_projspeed = callable(RVA_LOGICPROJECTILEDATA_GETSPEED);
    addr_projradius = callable(RVA_LOGICPROJECTILEDATA_GETRADIUS);
    addr_projbeam = callable(RVA_LOGICPROJECTILEDATA_ISBEAM);
    addr_castrange = callable(RVA_LOGICSKILLDATA_GETCASTRANGE);
    addr_skillproj = callable(RVA_LOGICSKILLDATA_GETPROJECTILEDATA);
    addr_weaponskill = callable(RVA_LOGICCHARACTERCLIENT_GETWEAPONSKILL);
    if (!addr_readable(addr_battlescreen, sizeof(void*)))
    {
        addr_battlescreen = 0;
    }
}
int coord_ok = 0;
int dead = 0;
int own_team_a = -1;
int own_team_seen = 0;
int scene_live(void)
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
uintptr_t client(void)
{
    void* clientPtr = nullptr;
    if (!scene_object || !scene_live())
    {
        return 0;
    }
    if (!read_ptr((uintptr_t)scene_object + CLIENT_OFF, &clientPtr))
    {
        return 0;
    }
    return (uintptr_t)clientPtr;
}
static int proj_read_name(uintptr_t data, char* dst, int cap);
typedef struct
{
    char name[64];
    float range;
} range_entry_t;
#define RANGE_MAP_MAX 32
#define HARVEST_MAX 32
static range_entry_t range_map[RANGE_MAP_MAX];
static int range_map_n = 0;
static uintptr_t harvested[HARVEST_MAX];
static int harvested_n = 0;
static void range_map_reset(void)
{
    range_map_n = 0;
    harvested_n = 0;
}
static int harvested_has(uintptr_t chr)
{
    int i = 0;
    for (i = 0; i < harvested_n; i++)
    {
        if (harvested[i] == chr)
        {
            return 1;
        }
    }
    return 0;
}
static void harvested_add(uintptr_t chr)
{
    if (harvested_n >= HARVEST_MAX)
    {
        return;
    }
    harvested[harvested_n++] = chr;
}
static void range_note(const char* name, float units)
{
    int i = 0;
    if (!name || !name[0] || units <= 0.0f)
    {
        return;
    }
    for (i = 0; i < range_map_n; i++)
    {
        if (strcmp(range_map[i].name, name) == 0)
        {
            if (units > range_map[i].range)
            {
                range_map[i].range = units;
            }
            return;
        }
    }
    if (range_map_n >= RANGE_MAP_MAX)
    {
        return;
    }
    strncpy(range_map[range_map_n].name, name, 63);
    range_map[range_map_n].name[63] = 0;
    range_map[range_map_n].range = units;
    range_map_n++;
}
static float range_lookup(const char* name)
{
    int i = 0;
    if (!name || !name[0])
    {
        return 0.0f;
    }
    for (i = 0; i < range_map_n; i++)
    {
        if (strcmp(range_map[i].name, name) == 0)
        {
            return range_map[i].range;
        }
    }
    return 0.0f;
}
static void harvest_skill_data(uintptr_t data)
{
    int32_t tiles = 0;
    int i = 0;
    if (!obj_ok(data) || !addr_castrange || !addr_skillproj)
    {
        return;
    }
    tiles = call1i(addr_castrange, data);
    if (tiles <= 0)
    {
        return;
    }
    for (i = 0; i < PROJ_INDEX_MAX; i++)
    {
        uintptr_t proj = call2u(addr_skillproj, data, i);
        char name[64];
        if (!obj_ok(proj) || !proj_read_name(proj, name, 64))
        {
            continue;
        }
        range_note(name, (float)tiles * CASTING_RANGE_SCALE);
    }
}
static void harvest_character(uintptr_t chr)
{
    uintptr_t weapon = 0;
    if (!obj_ok(chr) || !addr_weaponskill)
    {
        return;
    }
    weapon = call1u(addr_weaponskill, chr);
    if (!obj_ok(weapon))
    {
        return;
    }
    harvest_skill_data(weapon);
}
static uintptr_t char_by_owner(int32_t ownerIdx)
{
    int i = 0;
    if (ownerIdx < 0 || !manager_array_ptr || manager_count_v <= 0)
    {
        return 0;
    }
    for (i = 0; i < manager_count_v && i < 128; i++)
    {
        uintptr_t obj = 0;
        int32_t id = 0;
        int32_t owner = -1;
        if (!read_ptr(manager_array_ptr + (uintptr_t)i * 8ULL, (void**)&obj) ||
            !obj)
        {
            continue;
        }
        if (!obj_ok(obj))
        {
            continue;
        }
        if (!read_int(obj + (uintptr_t)OBJ_GLOBALID_OFF, &id))
        {
            continue;
        }
        if (id < 1000000 || id >= 2000000)
        {
            continue;
        }
        if (!read_int(obj + (uintptr_t)OBJ_OWNERINDEX_OFF, &owner))
        {
            continue;
        }
        if (owner == ownerIdx)
        {
            return obj;
        }
    }
    return 0;
}
float range_for_proj(uintptr_t projData, int32_t ownerIdx)
{
    char name[64];
    float hit = 0.0f;
    uintptr_t chr = 0;
    if (!obj_ok(projData) || !proj_read_name(projData, name, 64))
    {
        return 0.0f;
    }
    hit = range_lookup(name);
    if (hit > 0.0f)
    {
        return hit;
    }
    chr = char_by_owner(ownerIdx);
    if (!chr || harvested_has(chr))
    {
        return 0.0f;
    }
    harvested_add(chr);
    harvest_character(chr);
    return range_lookup(name);
}
int range_map_size(void)
{
    return range_map_n;
}
float own_cast_range(void)
{
    static float cached = 0.0f;
    static uint64_t stamp = 0;
    uint64_t now = us();
    uintptr_t weapon = 0;
    int32_t tiles = 0;
    if (stamp != 0 && now - stamp < 500000ULL)
    {
        return cached;
    }
    stamp = now;
    if (!own_elem || !addr_weaponskill || !addr_castrange)
    {
        return cached;
    }
    if (!obj_ok(own_elem))
    {
        return cached;
    }
    weapon = call1u(addr_weaponskill, own_elem);
    if (!obj_ok(weapon))
    {
        return cached;
    }
    tiles = call1i(addr_castrange, weapon);
    cached = tiles > 0 ? (float)tiles * CASTING_RANGE_SCALE : 0.0f;
    return cached;
}
void state_note(int state)
{
    if (prev_state == 5 && state != 5)
    {
        range_map_reset();
    }
    prev_state = state;
}
static int proj_read_name(uintptr_t data, char* dst, int cap)
{
    void* strp = nullptr;
    void* farp = nullptr;
    uintptr_t str = 0;
    uintptr_t src = 0;
    int32_t len = 0;
    if (!data || !dst || cap <= 1)
    {
        return 0;
    }
    if (!read_ptr(data + (uintptr_t)PROJ_DATA_NAME_OFF, &strp) || !strp)
    {
        return 0;
    }
    str = (uintptr_t)strp;
    if (!read_int(str + (uintptr_t)SC_LEN_OFF, &len))
    {
        return 0;
    }
    if (len <= 0 || len >= cap)
    {
        return 0;
    }
    if (len <= 7)
    {
        src = str + (uintptr_t)SC_DATA_OFF;
    }
    else
    {
        if (!read_ptr(str + (uintptr_t)SC_DATA_OFF, &farp) || !farp)
        {
            return 0;
        }
        src = (uintptr_t)farp;
    }
    if (!read_bytes(src, dst, (size_t)len))
    {
        return 0;
    }
    dst[len] = 0;
    return 1;
}
uintptr_t controller(void)
{
    return client();
}
uintptr_t scene_object = 0;
uintptr_t own_elem_scan = 0;
int own_team_b = -1;
uintptr_t own_elem = 0;
