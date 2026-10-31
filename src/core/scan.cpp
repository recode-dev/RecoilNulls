#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include "../features/autododge.h"
#include "../utils/walls.h"
#include "commands.h"
#include "offsets.h"
#include "scan.h"
#include <frida-gum.h>
const char*               image_name       = "Nulls Brawl";
BOOL                      setup_done       = NO;
__thread BOOL             inside_hook      = NO;
inline constexpr int      PAGE_CACHE_SLOTS = 1024;
static __thread uintptr_t page_cache_key[PAGE_CACHE_SLOTS];
static __thread uint8_t   page_cache_prot[PAGE_CACHE_SLOTS];
static guint              memory_page_size()
{
    static guint cached = 0;
    if (!cached)
    {
        cached = gum_query_page_size();
    }
    return cached;
}
static GumPageProtection page_protection(uintptr_t page)
{
    int slot = static_cast<int>((page >> 12) & (PAGE_CACHE_SLOTS - 1));
    GumPageProtection prot = GUM_PAGE_NO_ACCESS;
    if (page_cache_key[slot] == page)
    {
        return static_cast<GumPageProtection>(page_cache_prot[slot]);
    }
    if (!gum_memory_query_protection((gconstpointer) page, &prot))
    {
        prot = GUM_PAGE_NO_ACCESS;
    }
    page_cache_key[slot]  = page;
    page_cache_prot[slot] = static_cast<uint8_t>(prot);
    return prot;
}
static BOOL
range_allows(uintptr_t address, size_t length, GumPageProtection required)
{
    uintptr_t end      = 0;
    uintptr_t cursor   = 0;
    uintptr_t pageSize = 0;
    if (!address || !length || !gum_live)
    {
        return NO;
    }
    end = address + length;
    if (end <= address)
    {
        return NO;
    }
    pageSize = static_cast<uintptr_t>(memory_page_size());
    if (!pageSize)
    {
        return NO;
    }
    for (cursor = address & ~(pageSize - 1); cursor < end; cursor += pageSize)
    {
        if ((page_protection(cursor) & required) != required)
        {
            return NO;
        }
    }
    return YES;
}
BOOL addr_writable(uintptr_t address, size_t length)
{
    return range_allows(address, length, GUM_PAGE_WRITE);
}
void read_cache_reset()
{
    for (int i = 0; i < PAGE_CACHE_SLOTS; i++)
    {
        page_cache_key[i]  = 0;
        page_cache_prot[i] = 0;
    }
}
BOOL read_bytes(uintptr_t address, void* out, size_t length)
{
    if (!out || !length || !address || length > 0x10000)
    {
        return NO;
    }
    if (!range_allows(address, length, GUM_PAGE_READ))
    {
        return NO;
    }
    __builtin_memcpy(out, reinterpret_cast<const void*>(address), length);
    return YES;
}
static void require_method(uintptr_t fn)
{
    if (!fn) __builtin_trap();
}
static void require_object(uintptr_t value)
{
    if (!value) __builtin_trap();
}
BOOL pointer_plausible(uintptr_t value)
{
    if (value < 0x100000000ULL || value >= 0x00007fffffffffffULL)
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
BOOL memory_protection_flags(uintptr_t address, vm_prot_t* flags)
{
    if (!address || !flags || !gum_live)
    {
        return NO;
    }
    *flags = static_cast<vm_prot_t>(page_protection(address));
    return *flags ? YES : NO;
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
    if (!addr_writable(address, length))
    {
        return NO;
    }
    __builtin_memcpy(reinterpret_cast<void*>(address), src, length);
    return YES;
}
BOOL read_ptr(uintptr_t address, uintptr_t* out)
{
    if (!out || (address & 7))
    {
        return NO;
    }
    return read_bytes(address, out, sizeof(uintptr_t));
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
int game_tile_is_passable(uintptr_t tileMap, int cellX, int cellY)
{
    using pass_t     = uint8_t (*)(void*, int, int, int, int, int);
    static pass_t fn = nullptr;
    if (!fn)
    {
        fn = reinterpret_cast<pass_t>(
            callable(LOGICTILEMAP__ISPASSABLEPATHFINDER));
    }
    require_method(reinterpret_cast<uintptr_t>(fn));
    require_object(tileMap);
    return fn(reinterpret_cast<void*>(tileMap), 1, cellX, cellY, 0, 0);
}
static uintptr_t path_finder(uintptr_t tileMap)
{
    static uintptr_t cached_map = 0;
    static uintptr_t cached_pf  = 0;
    void*            back       = nullptr;
    if (!tileMap) return 0;
    if (tileMap == cached_map && cached_pf &&
        read_ptr(cached_pf + LOGICPATHFINDER__TILEMAP, &back) &&
        back == reinterpret_cast<void*>(tileMap))
    {
        return cached_pf;
    }
    cached_map = 0;
    cached_pf  = 0;
    void* pf   = nullptr;
    back       = nullptr;
    if (!read_ptr(tileMap + LOGICTILEMAP__PATHFINDER, &pf) || !pf)
    {
        return 0;
    }
    if (!read_ptr(reinterpret_cast<uintptr_t>(pf) + LOGICPATHFINDER__TILEMAP,
                  &back) ||
        back != reinterpret_cast<void*>(tileMap))
    {
        return 0;
    }
    cached_map = tileMap;
    cached_pf  = reinterpret_cast<uintptr_t>(pf);
    return cached_pf;
}
int game_path_clear(uintptr_t tileMap, int x0, int y0, int x1, int y1)
{
    using clear_t = uint32_t (*)(void*, int, int, int, int, int, int, int, int);
    static clear_t fn = nullptr;
    if (!fn)
    {
        fn = reinterpret_cast<clear_t>(callable(LOGICPATHFINDER__ISCLEARPATH));
    }
    require_method(reinterpret_cast<uintptr_t>(fn));
    uintptr_t pf = path_finder(tileMap);
    if (!pf) return 0;
    return static_cast<int>(
        fn(reinterpret_cast<void*>(pf), 1, x0, y0, x1, y1, 0, 1, 0));
}
int game_tile_to_logic(int tile)
{
    using conv_t     = int (*)(int);
    static conv_t fn = nullptr;
    if (!fn)
    {
        fn = reinterpret_cast<conv_t>(callable(LOGICTILEMAP__TILETOLOGIC));
    }
    require_method(reinterpret_cast<uintptr_t>(fn));
    return fn(tile);
}
int vector_length(int x, int y)
{
    using len_t       = int (*)(const int*);
    static len_t fn   = nullptr;
    int          v[2] = {x, y};
    if (!fn) fn = reinterpret_cast<len_t>(callable(LOGICVECTOR2__GETLENGTH));
    require_method(reinterpret_cast<uintptr_t>(fn));
    return fn(v);
}
int game_distance(int x1, int y1, int x2, int y2)
{
    using dist_t     = int (*)(int, int, int, int);
    static dist_t fn = nullptr;
    if (!fn)
    {
        fn = reinterpret_cast<dist_t>(
            callable(LOGICGAMEPLAYUTIL__GETDISTANCEBETWEEN));
    }
    require_method(reinterpret_cast<uintptr_t>(fn));
    return fn(x1, y1, x2, y2);
}
int game_segment_hits_circle(
    int x1, int y1, int x2, int y2, int cx, int cy, int radius, int* out)
{
    using hit_t     = int (*)(int, int, int, int, int, int, int, int*);
    static hit_t fn = nullptr;
    if (!fn)
    {
        fn = reinterpret_cast<hit_t>(
            callable(LOGICGAMEPLAYUTIL__LINESEGMENTINTERSECTSCIRCLE));
    }
    require_method(reinterpret_cast<uintptr_t>(fn));
    return fn(x1, y1, x2, y2, cx, cy, radius, out);
}
int game_projectile_travel_ticks(uintptr_t projectileData,
                                 int       distance,
                                 int       speed)
{
    using travel_t     = int (*)(uintptr_t, int, int);
    static travel_t fn = nullptr;
    if (!fn)
    {
        fn = reinterpret_cast<travel_t>(
            callable(LOGICPROJECTILECLIENT__GETTRAVELTICKS));
    }
    require_method(reinterpret_cast<uintptr_t>(fn));
    require_object(projectileData);
    return fn(projectileData, distance, speed);
}
uintptr_t callable(uintptr_t rva)
{
    if (!base || !rva)
    {
        return 0;
    }
    uintptr_t address = base + rva;
    if (address < base || address >= base + 0x2000000)
    {
        return 0;
    }
    return address;
}
int         gum_live = 0;
static BOOL module_base_by_name(const char* name, uintptr_t* out_base)
{
    GumModule*            module = nullptr;
    const GumMemoryRange* range  = nullptr;
    uintptr_t             found  = 0;
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
    uintptr_t  found     = 0;
    if (!out_base)
    {
        return NO;
    }
    if (!gum_ready)
    {
        gum_process_set_teardown_requirement(GUM_TEARDOWN_REQUIREMENT_MINIMAL);
        gum_init_embedded();
        gum_ready = 1;
        gum_live  = 1;
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
uintptr_t      base           = 0;
static int32_t own_team_value = -1;
float          own_speed      = 720.0f;
uint32_t       own_hp         = 0;
uint64_t       us()
{
    return static_cast<uint64_t>(g_get_monotonic_time());
}
using self_t = uintptr_t (*)(uintptr_t);
static uintptr_t call(uintptr_t fn, uintptr_t self)
{
    if (!fn || !base || fn < base || fn >= base + 0x2000000 || !self) return 0;
    return ((self_t) fn)(self);
}
uintptr_t        snapshot[COUNT_MAX];
uint8_t          block[COUNT_MAX][OBJ_BLOCK_SIZE];
uint8_t          valid[COUNT_MAX];
int              obj_count = 0;
int32_t          own_x     = 0;
int32_t          own_y     = 0;
int              ok_hp     = 0;
static uintptr_t battle_instance()
{
    using inst_t     = uintptr_t (*)();
    static inst_t fn = nullptr;
    if (!fn) fn = reinterpret_cast<inst_t>(callable(BATTLEMODE__GETINSTANCE));
    if (!fn) return 0;
    return fn();
}
static int32_t  last_pos_x  = 0;
static int32_t  last_pos_y  = 0;
static uint64_t last_pos_us = 0;
static void     world_reset()
{
    elem         = 0;
    own_x        = 0;
    own_y        = 0;
    own_hp       = 0;
    ok_hp        = 0;
    dead         = 1;
    coord_ok     = 0;
    obj_count    = 0;
    scene_object = 0;
    last_pos_us  = 0;
    wall_cache_bind(0);
    reset_autododge_state();
}
static void track_own_speed()
{
    uint64_t now_us = us();
    if (!last_pos_us || now_us <= last_pos_us)
    {
        last_pos_x  = own_x;
        last_pos_y  = own_y;
        last_pos_us = now_us;
        return;
    }
    float dt = static_cast<float>(now_us - last_pos_us) / 1000000.0f;
    if (dt < 0.06f) return;
    if (dt < 0.25f)
    {
        int   px = own_x - last_pos_x;
        int   py = own_y - last_pos_y;
        float sp = static_cast<float>(game_distance(0, 0, px, py)) / dt;
        if (sp >= 300.0f && sp <= 6000.0f)
        {
            if (sp > own_speed)
                own_speed = sp;
            else
                own_speed = own_speed * 0.99f + sp * 0.01f;
        }
    }
    last_pos_x  = own_x;
    last_pos_y  = own_y;
    last_pos_us = now_us;
}
void scan_world()
{
    void*     value     = nullptr;
    uintptr_t clientPtr = 0;
    uintptr_t mgrPtr    = 0;
    uintptr_t array     = 0;
    uintptr_t own       = 0;
    int32_t   count     = 0;
    int32_t   x         = 0;
    int32_t   y         = 0;
    int32_t   hp        = 0;
    if (!base) return;
    scene_object = battle_instance();
    if (!scene_object)
    {
        world_reset();
        return;
    }
    if (!read_ptr(scene_object +
                      static_cast<uintptr_t>(LOGICBATTLEMODE__CLIENT),
                  &value) ||
        !value)
    {
        world_reset();
        return;
    }
    clientPtr = reinterpret_cast<uintptr_t>(value);
    if (!read_ptr(clientPtr + static_cast<uintptr_t>(
                                  LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER),
                  &value) ||
        !value)
    {
        world_reset();
        return;
    }
    mgrPtr = (uintptr_t) value;
    if (!read_int(mgrPtr + static_cast<uintptr_t>(
                               LOGICGAMEOBJECTMANAGER__GAMEOBJECTCOUNT),
                  &count) ||
        count <= 0 || count > OBJECT_COUNT_MAX)
    {
        world_reset();
        return;
    }
    if (!read_ptr(mgrPtr + static_cast<uintptr_t>(
                               LOGICGAMEOBJECTMANAGER__GAMEOBJECTS),
                  &value) ||
        !value)
    {
        world_reset();
        return;
    }
    array = (uintptr_t) value;
    if (count > COUNT_MAX) count = COUNT_MAX;
    if (!read_bytes(
            array, snapshot, static_cast<size_t>(count) * sizeof(uintptr_t)))
    {
        world_reset();
        return;
    }
    obj_count = count;
    for (int i = 0; i < count; i++)
    {
        uintptr_t objPtr = snapshot[i];
        valid[i]         = (objPtr && pointer_plausible(objPtr) &&
                    read_bytes(objPtr, block[i], OBJ_BLOCK_SIZE))
                               ? 1
                               : 0;
    }
    {
        static uintptr_t ownFn = 0;
        if (!ownFn) ownFn = callable(LOGICBATTLEMODECLIENT__GETOWNCHARACTER);
        uintptr_t ownByFn = ownFn ? call(ownFn, clientPtr) : 0;
        own = (ownByFn && pointer_plausible(ownByFn)) ? ownByFn : 0;
        if (own)
        {
            uintptr_t ownData = 0;
            int32_t   team    = 0;
            if (read_ptr(own + static_cast<uintptr_t>(
                                   LOGICGAMEOBJECTCLIENT__GETDATA),
                         &ownData) &&
                ownData &&
                read_int(ownData + static_cast<uintptr_t>(LOGICDATA__TEAM),
                         &team))
            {
                own_team_value = team;
            }
        }
    }
    if (!own)
    {
        elem     = 0;
        ok_hp    = 0;
        own_hp   = 0;
        dead     = 1;
        coord_ok = 0;
        return;
    }
    elem = own;
    if (read_int(own + static_cast<uintptr_t>(LOGICGAMEOBJECTCLIENT__GETX),
                 &x) &&
        read_int(own + static_cast<uintptr_t>(LOGICGAMEOBJECTCLIENT__GETY), &y))
    {
        own_x    = x;
        own_y    = y;
        coord_ok = 1;
    }
    if (read_int(own + static_cast<uintptr_t>(LOGICGAMEOBJECTCLIENT__HP),
                 &hp) &&
        hp >= 0 && hp < 1000000)
    {
        ok_hp  = 1;
        own_hp = static_cast<uint32_t>(hp);
    }
    else
    {
        ok_hp  = 0;
        own_hp = 0;
    }
    dead = (ok_hp && own_hp == 0) ? 1 : 0;
    track_own_speed();
    if (clientPtr)
    {
        static uintptr_t tileMapFn = 0;
        if (!tileMapFn) tileMapFn = callable(LOGICBATTLEMODECLIENT__GETTILEMAP);
        wall_cache_bind(tileMapFn ? call(tileMapFn, clientPtr) : 0);
    }
}
void workload()
{
    scan_world();
    run_autododge();
}
void run_autododge()
{
    if (move)
    {
        return;
    }
    move = 1;
    update_autododge();
    move = 0;
}
static constexpr uint64_t TICK_US        = 50000;
static constexpr uint64_t SCAN_PERIOD_US = TICK_US / 6;
static void               scan_frame()
{
    if (inside_hook)
    {
        return;
    }
    inside_hook = YES;
    read_cache_reset();
    workload();
    commands_flush();
    inside_hook = NO;
}
static dispatch_source_t scan_timer = nullptr;
static void              start_scan_timer()
{
    if (scan_timer)
    {
        return;
    }
    scan_timer = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    if (!scan_timer)
    {
        return;
    }
    dispatch_source_set_timer(scan_timer,
                              dispatch_time(DISPATCH_TIME_NOW, 0),
                              SCAN_PERIOD_US * NSEC_PER_USEC,
                              0);
    dispatch_source_set_event_handler(scan_timer, ^{ scan_frame(); });
    dispatch_resume(scan_timer);
}
void setup()
{
    if (setup_done)
    {
        return;
    }
    start_scan_timer();
    setup_done = YES;
}
int coord_ok = 0;
int dead     = 0;
int scene_live()
{
    return battle_instance() != 0 ? 1 : 0;
}
int is_owned_team_data(uintptr_t data)
{
    int32_t team = 0;
    if (own_team_value < 0 || !data) return 0;
    if (!read_int(data + static_cast<uintptr_t>(LOGICDATA__TEAM), &team))
        return 0;
    return team == own_team_value ? 1 : 0;
}
int is_owned_by_own_team(uintptr_t obj)
{
    uintptr_t data = 0;
    int32_t   team = 0;
    if (own_team_value < 0) return 0;
    if (!read_ptr(obj + static_cast<uintptr_t>(LOGICGAMEOBJECTCLIENT__GETDATA),
                  &data) ||
        !data)
    {
        return 0;
    }
    if (!read_int(data + static_cast<uintptr_t>(LOGICDATA__TEAM), &team))
        return 0;
    return team == own_team_value ? 1 : 0;
}
uintptr_t client()
{
    void* value = nullptr;
    if (!scene_object || !scene_live()) return 0;
    if (!read_ptr(scene_object +
                      static_cast<uintptr_t>(LOGICBATTLEMODE__CLIENT),
                  &value) ||
        !value)
    {
        return 0;
    }
    return reinterpret_cast<uintptr_t>(value);
}
uintptr_t controller()
{
    return client();
}
uintptr_t scene_object = 0;
uintptr_t elem         = 0;
