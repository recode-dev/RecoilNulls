#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <cstdint>
#include <cstring>
#include "../features/autododge.h"
#include "../utils/walls.h"
#include "commands.h"
#include "offsets.h"
#include "scan.h"
#include <frida-gum.h>
const char*               image_name       = "Nulls Brawl";
bool                      setup_done       = false;
inline constexpr int      PAGE_CACHE_SLOTS = 1024;
static __thread uintptr_t page_cache_key[PAGE_CACHE_SLOTS];
static __thread uint8_t   page_cache_prot[PAGE_CACHE_SLOTS];
static __thread uint64_t  page_cache_stamp[PAGE_CACHE_SLOTS];
static uint64_t           page_cache_epoch = 0;
inline constexpr guint    GUM_IMAGE_LIMIT  = 0x2000000;

static uintptr_t fn_battle_instance = 0;
static uintptr_t fn_own_character   = 0;
static uintptr_t fn_tile_map        = 0;
static uintptr_t fn_tile_passable   = 0;
static uintptr_t fn_tile_to_logic   = 0;
static uintptr_t fn_segment_circle  = 0;
static uintptr_t fn_vec2_length     = 0;
static uintptr_t fn_vec2_normalize  = 0;
static uintptr_t fn_pt_distance     = 0;
static uintptr_t fn_get_angle       = 0;
static uintptr_t fn_norm_angle      = 0;
static uintptr_t fn_first_collision = 0;

static guint memory_page_size()
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
    if (page_cache_stamp[slot] == page_cache_epoch && page_cache_key[slot] == page)
    {
        return static_cast<GumPageProtection>(page_cache_prot[slot]);
    }
    GumPageProtection prot = GUM_PAGE_NO_ACCESS;
    if (!gum_memory_query_protection((gconstpointer) page, &prot))
    {
        prot = GUM_PAGE_NO_ACCESS;
    }
    page_cache_key[slot]   = page;
    page_cache_prot[slot]  = static_cast<uint8_t>(prot);
    page_cache_stamp[slot] = page_cache_epoch;
    return prot;
}

static bool range_allows(uintptr_t address, size_t length, GumPageProtection required)
{
    uintptr_t end      = 0;
    uintptr_t cursor   = 0;
    uintptr_t pageSize = 0;
    if (!address || !length || !gum_live)
    {
        return false;
    }
    end = address + length;
    if (end <= address)
    {
        return false;
    }
    pageSize = static_cast<uintptr_t>(memory_page_size());
    if (!pageSize)
    {
        return false;
    }
    for (cursor = address & ~(pageSize - 1); cursor < end; cursor += pageSize)
    {
        if ((page_protection(cursor) & required) != required)
        {
            return false;
        }
    }
    return true;
}

bool read_bytes(uintptr_t address, void* out, size_t length)
{
    if (!out || !length || !address || length > 0x10000)
    {
        return false;
    }
    if (!range_allows(address, length, GUM_PAGE_READ))
    {
        return false;
    }
    __builtin_memcpy(out, reinterpret_cast<const void*>(address), length);
    return true;
}

void read_cache_reset()
{
    page_cache_epoch = frame_index;
}

bool read_int(uintptr_t address, int32_t* out)
{
    if (!out)
    {
        return false;
    }
    if (address & 3)
    {
        return false;
    }
    return read_bytes(address, out, 4);
}

bool read_ptr(uintptr_t address, uintptr_t* out)
{
    if (!out || (address & 7))
    {
        return false;
    }
    return read_bytes(address, out, sizeof(uintptr_t));
}

bool read_ptr(uintptr_t address, void** out)
{
    if (!out)
    {
        return false;
    }
    if (address & 7)
    {
        return false;
    }
    return read_bytes(address, out, sizeof(void*));
}

int game_tile_is_passable(uintptr_t tileMap, int cellX, int cellY)
{
    using pass_t = uint8_t (*)(void*, int, int, int, int, int);
    return reinterpret_cast<pass_t>(fn_tile_passable)(
        reinterpret_cast<void*>(tileMap), 1, cellX, cellY, 0, 0);
}

int game_tile_to_logic(int tile)
{
    using conv_t = int (*)(int);
    return reinterpret_cast<conv_t>(fn_tile_to_logic)(tile);
}

int game_segment_hits_circle(int x1, int y1, int x2, int y2, int cx, int cy, int radius, int* out)
{
    using hit_t = int (*)(int, int, int, int, int, int, int, int*);
    return reinterpret_cast<hit_t>(fn_segment_circle)(x1, y1, x2, y2, cx, cy, radius, out);
}

int32_t game_vec2_length(int32_t x, int32_t y)
{
    int32_t v[2] = {x, y};
    return reinterpret_cast<int32_t (*)(int32_t*)>(fn_vec2_length)(v);
}

void game_vec2_normalize(int32_t* v, int32_t scale)
{
    reinterpret_cast<void (*)(int32_t*, int32_t)>(fn_vec2_normalize)(v, scale);
}

int32_t game_distance(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    return reinterpret_cast<int32_t (*)(int32_t, int32_t, int32_t, int32_t)>(fn_pt_distance)(
        x1, y1, x2, y2);
}

int game_get_angle(int32_t dx, int32_t dy)
{
    return reinterpret_cast<int (*)(int32_t, int32_t)>(fn_get_angle)(dx, dy);
}

int game_normalize_angle_180(int angle)
{
    return reinterpret_cast<int (*)(int)>(fn_norm_angle)(angle);
}

int game_first_collision(
    int size, int x1, int y1, int x2, int y2, uintptr_t pathfinder, int32_t* out)
{
    using collide_t = int (*)(int, int, int, int, int, void*, int32_t*, int, int);
    return reinterpret_cast<collide_t>(fn_first_collision)(
        size, x1, y1, x2, y2, reinterpret_cast<void*>(pathfinder), out, 0, 0);
}

uintptr_t callable(uintptr_t rva)
{
    if (!base || !rva)
    {
        return 0;
    }
    uintptr_t address = base + rva;
    if (address < base || address >= base + GUM_IMAGE_LIMIT)
    {
        return 0;
    }
    return address;
}

int gum_live = 0;

static bool module_base_by_name(const char* name, uintptr_t* out_base)
{
    GumModule*            module = nullptr;
    const GumMemoryRange* range  = nullptr;
    uintptr_t             found  = 0;
    if (!name || !name[0] || !out_base)
    {
        return false;
    }
    module = gum_process_find_module_by_name(name);
    if (!module)
    {
        return false;
    }
    range = gum_module_get_range(module);
    if (range)
    {
        found = static_cast<uintptr_t>(range->base_address);
    }
    g_object_unref(module);
    if (!found)
    {
        return false;
    }
    *out_base = found;
    return true;
}

bool find_game_image(uintptr_t* out_base)
{
    static int gum_ready = 0;
    uintptr_t  found     = 0;
    if (!out_base)
    {
        return false;
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
        return false;
    }
    *out_base = found;
    return true;
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
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(0.5 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(),
                   ^{ poll_for_game(tick + 1); });
}

uintptr_t        base           = 0;
static uintptr_t client_cached  = 0;
const uint8_t*   own_block      = nullptr;
static int32_t   own_team_value = -1;
uint32_t         own_hp         = 0;

using self_t = uintptr_t (*)(uintptr_t);

static uintptr_t call(uintptr_t fn, uintptr_t self)
{
    if (!fn || !base || fn < base || fn >= base + GUM_IMAGE_LIMIT || !self) return 0;
    return ((self_t) fn)(self);
}

uintptr_t snapshot[COUNT_MAX];
uint8_t   block[COUNT_MAX][OBJ_BLOCK_SIZE];
uint8_t   valid[COUNT_MAX];
int       obj_count   = 0;
uint64_t  frame_index = 0;
int32_t   own_x       = 0;
int32_t   own_y       = 0;

static uintptr_t battle_instance()
{
    return reinterpret_cast<uintptr_t (*)()>(fn_battle_instance)();
}

static void world_reset()
{
    elem           = 0;
    own_x          = 0;
    own_y          = 0;
    own_hp         = 0;
    dead           = 1;
    coord_ok       = 0;
    obj_count      = 0;
    scene_object   = 0;
    client_cached  = 0;
    own_block      = nullptr;
    own_team_value = -1;
    wall_cache_bind(0);
    reset_autododge_state();
}

void scan_world()
{
    void*     value     = nullptr;
    uintptr_t clientPtr = 0;
    uintptr_t mgrPtr    = 0;
    uintptr_t array     = 0;
    uintptr_t own       = 0;
    int32_t   count     = 0;
    int32_t   hp        = 0;
    if (!base) return;
    scene_object = battle_instance();
    if (!scene_object)
    {
        world_reset();
        return;
    }
    if (!read_ptr(scene_object + static_cast<uintptr_t>(LOGICBATTLEMODE__CLIENT), &value) || !value)
    {
        world_reset();
        return;
    }
    clientPtr     = reinterpret_cast<uintptr_t>(value);
    client_cached = clientPtr;
    {
        int32_t team = 0;
        if (!read_int(clientPtr + static_cast<uintptr_t>(LOGICBATTLEMODECLIENT__OWNTEAM), &team) ||
            team < 0)
        {
            world_reset();
            return;
        }
        own_team_value = team;
    }
    if (!read_ptr(clientPtr + static_cast<uintptr_t>(LOGICBATTLEMODECLIENT__GAMEOBJECTMANAGER),
                  &value) ||
        !value)
    {
        world_reset();
        return;
    }
    mgrPtr = (uintptr_t) value;
    if (!read_int(mgrPtr + static_cast<uintptr_t>(LOGICGAMEOBJECTMANAGER__GAMEOBJECTCOUNT), &count))
    {
        world_reset();
        return;
    }
    if (count <= 0 || count > COUNT_MAX)
    {
        world_reset();
        return;
    }
    if (!read_ptr(mgrPtr + static_cast<uintptr_t>(LOGICGAMEOBJECTMANAGER__GAMEOBJECTS), &value) ||
        !value)
    {
        world_reset();
        return;
    }
    array = (uintptr_t) value;
    if (!read_bytes(array, snapshot, static_cast<size_t>(count) * sizeof(uintptr_t)))
    {
        world_reset();
        return;
    }
    own = call(fn_own_character, clientPtr);
    if (!own)
    {
        elem      = 0;
        own_hp    = 0;
        obj_count = 0;
        dead      = 1;
        coord_ok  = 0;
        return;
    }
    elem      = own;
    own_block = nullptr;
    obj_count = count;
    for (int i = 0; i < count; i++)
    {
        uintptr_t objPtr = snapshot[i];
        if (!objPtr)
        {
            valid[i] = 0;
            continue;
        }
        if (!read_bytes(objPtr, block[i], OBJ_BLOCK_SIZE))
        {
            world_reset();
            return;
        }
        valid[i] = 1;
        if (objPtr == own)
        {
            own_block = block[i];
        }
    }
    {
        int32_t xy[2] = {0, 0};
        if (!read_bytes(own + static_cast<uintptr_t>(LOGICGAMEOBJECTCLIENT__GETX), xy, sizeof(xy)))
        {
            world_reset();
            return;
        }
        own_x    = xy[0];
        own_y    = xy[1];
        coord_ok = 1;
    }
    if (!read_int(own + static_cast<uintptr_t>(LOGICGAMEOBJECTCLIENT__HP), &hp) || hp < 0)
    {
        world_reset();
        return;
    }
    own_hp = static_cast<uint32_t>(hp);
    dead   = own_hp == 0 ? 1 : 0;
    wall_cache_bind(call(fn_tile_map, clientPtr));
}

void workload()
{
    scan_world();
    update_autododge();
}

static constexpr uint64_t TICK_US = 50000;

static void scan_frame()
{
    frame_index += 1;
    read_cache_reset();
    workload();
    commands_flush();
}

static dispatch_source_t scan_timer = nullptr;

static void start_scan_timer()
{
    if (scan_timer)
    {
        return;
    }
    scan_timer =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    if (!scan_timer)
    {
        return;
    }
    dispatch_source_set_timer(
        scan_timer, dispatch_time(DISPATCH_TIME_NOW, 0), TICK_US * NSEC_PER_USEC, 0);
    dispatch_source_set_event_handler(scan_timer, ^{ scan_frame(); });
    dispatch_resume(scan_timer);
}

static bool resolve_game_fns()
{
    fn_battle_instance = callable(BATTLEMODE__GETINSTANCE);
    fn_own_character   = callable(LOGICBATTLEMODECLIENT__GETOWNCHARACTER);
    fn_tile_map        = callable(LOGICBATTLEMODECLIENT__GETTILEMAP);
    fn_tile_passable   = callable(LOGICTILEMAP__ISPASSABLEPATHFINDER);
    fn_tile_to_logic   = callable(LOGICTILEMAP__TILETOLOGIC);
    fn_segment_circle  = callable(LOGICGAMEPLAYUTIL__LINESEGMENTINTERSECTSCIRCLE);
    fn_vec2_length     = callable(LOGICVECTOR2__GETLENGTH);
    fn_vec2_normalize  = callable(LOGICVECTOR2__NORMALIZE);
    fn_pt_distance     = callable(LOGICGAMEPLAYUTIL__GETDISTANCEBETWEEN);
    fn_get_angle       = callable(LOGICMATH__GETANGLE);
    fn_norm_angle      = callable(LOGICMATH__NORMALIZEANGLE180);
    fn_first_collision = callable(LOGICGAMEPLAYUTIL__GETCLOSESTPATHFINDERCOLLISION);
    return fn_battle_instance && fn_own_character && fn_tile_map && fn_tile_passable &&
           fn_tile_to_logic && fn_segment_circle && fn_vec2_length && fn_vec2_normalize &&
           fn_pt_distance && fn_get_angle && fn_norm_angle && fn_first_collision;
}

void setup()
{
    if (setup_done)
    {
        return;
    }
    if (!resolve_game_fns())
    {
        return;
    }
    client_input_resolve_all();
    start_scan_timer();
    setup_done = true;
}

int coord_ok = 0;
int dead     = 0;

int owned_team_of(uintptr_t data)
{
    if (own_team_value < 0 || !data) return -1;
    int32_t team = 0;
    if (!read_int(data + static_cast<uintptr_t>(LOGICDATA__TEAM), &team)) return -1;
    return team == own_team_value ? 1 : 0;
}

uintptr_t client()
{
    return client_cached;
}

uintptr_t scene_object = 0;
uintptr_t elem         = 0;
