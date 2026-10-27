#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <stdint.h>
#include <string.h>
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
uintptr_t addr_charweapon = 0;
uintptr_t addr_charskillat = 0;
uintptr_t addr_skilldata = 0;
uintptr_t addr_skillrange = 0;
uintptr_t addr_skillproj = 0;
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
    return gum_memory_write(
               (gpointer)address, (const guint8*)src, (gsize)length)
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
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)),
        dispatch_get_main_queue(),
        ^{ poll_for_game(tick + 1); });
}
uintptr_t base = 0;
uintptr_t manager_ptr = 0;
uint64_t ticks_a = 0;
float own_speed = 720.0f;
float own_r = 60.0f;
uint32_t own_hp = 0;
uint64_t us(void)
{
    return (uint64_t)g_get_monotonic_time();
}
typedef uintptr_t (*fn1)(uintptr_t);
typedef int32_t (*fn1i)(uintptr_t);
static uintptr_t call1(uintptr_t fn, uintptr_t self)
{
    if (!fn || !base || fn < base || fn >= base + 0x2000000ULL || !self) return 0;
    return ((fn1)fn)(self);
}
static int32_t call1i(uintptr_t fn, uintptr_t self)
{
    if (!fn || !base || fn < base || fn >= base + 0x2000000ULL || !self) return 0;
    return ((fn1i)fn)(self);
}
typedef uintptr_t (*fn2)(uintptr_t, uintptr_t);
static uintptr_t call2(uintptr_t fn, uintptr_t self, uintptr_t arg)
{
    if (!fn || !base || fn < base || fn >= base + 0x2000000ULL || !self) return 0;
    return ((fn2)fn)(self, arg);
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
static int battle_ticks = 0;
static void character_registry_reset(void);
static void world_reset(void)
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
    character_registry_reset();
    wall_drop();
    autododge_reset();
}
#define CHAR_REGISTRY_MAX 64
#define RANGE_CACHE_MAX 256
typedef struct
{
    char name[128];
    int units;
} range_entry_t;
static range_entry_t g_range[RANGE_CACHE_MAX];
static int g_rangeN = 0;
static uintptr_t g_charHarvested[CHAR_REGISTRY_MAX];
static void range_note(const char* name, int units)
{
    if (!name || !name[0] || units <= 0) return;
    for (int i = 0; i < g_rangeN; i++)
    {
        if (strcmp(g_range[i].name, name) == 0)
        {
            if (units > g_range[i].units) g_range[i].units = units;
            return;
        }
    }
    if (g_rangeN >= RANGE_CACHE_MAX) return;
    snprintf(g_range[g_rangeN].name, sizeof(g_range[g_rangeN].name), "%s", name);
    g_range[g_rangeN].units = units;
    g_rangeN++;
}
int cast_range_for(const char* name)
{
    if (!name || !name[0]) return 0;
    for (int i = 0; i < g_rangeN; i++)
    {
        if (strcmp(g_range[i].name, name) == 0) return g_range[i].units;
    }
    return 0;
}
static void harvest_skill_data(uintptr_t skillData)
{
    if (!skillData || !pointer_plausible(skillData)) return;
    int tiles = call1i(addr_skillrange, skillData);
    if (tiles <= 0 || tiles > 64) return;
    int units = tiles * CASTING_RANGE_SCALE;
    for (int i = 0; i < PROJ_INDEX_MAX; i++)
    {
        uintptr_t proj = call2(addr_skillproj, skillData, (uintptr_t)i);
        if (!proj || !pointer_plausible(proj)) continue;
        uintptr_t nameStr = call1(addr_dataname, proj);
        char name[128];
        if (nameStr && pointer_plausible(nameStr) && read_scstring(nameStr, SCSTRING_DEFAULT_MAX_LENGTH, name,
                                     sizeof(name)))
        {
            range_note(name, units);
        }
    }
}
static int harvest_character(uintptr_t character)
{
    int found = 0;
    if (!character || !pointer_plausible(character)) return 0;
    uintptr_t weapon = call1(addr_charweapon, character);
    if (weapon && pointer_plausible(weapon))
    {
        harvest_skill_data(weapon);
        found = 1;
    }
    for (int slot = 0; slot < SKILL_SLOT_MAX; slot++)
    {
        uintptr_t skill = call2(addr_charskillat, character, (uintptr_t)slot);
        if (!skill || !pointer_plausible(skill)) continue;
        for (int hyper = 0; hyper < 2; hyper++)
        {
            uintptr_t data = call2(addr_skilldata, skill, (uintptr_t)hyper);
            if (data && pointer_plausible(data))
            {
                harvest_skill_data(data);
                found = 1;
            }
        }
    }
    return found;
}
static void remember_character(int index, uintptr_t character)
{
    if (!CHARACTER_HARVEST_ON || battle_ticks < 30) return;
    if (!addr_charweapon || !addr_charskillat || !addr_skilldata ||
        !addr_skillrange || !addr_skillproj || !addr_dataname) return;
    if (index < 0 || index >= CHAR_REGISTRY_MAX) return;
    if (!character || !pointer_plausible(character)) return;
    if (g_charHarvested[index] == character) return;
    if (harvest_character(character))
    {
        g_charHarvested[index] = character;
    }
}
static void character_registry_reset(void)
{
    for (int i = 0; i < CHAR_REGISTRY_MAX; i++)
    {
        g_charHarvested[i] = 0;
    }
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
    if (!base) return;
    state = battle_state();
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
        return;
    }
    battle_ticks++;
    clientPtr = controller();
    if (!clientPtr)
    {
        world_reset();
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
            wall_maybe_refresh((uint64_t)(g_get_real_time() / 1000));
        }
    }
    if (!pointer_plausible(own))
    {
        own = 0;
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
            int32_t radius = 0;
            int32_t speed = 0;
            radius = call1i(addr_charradius, ownData);
            speed = call1i(addr_charspeed, ownData);
            own_r = (float)radius;
            if (speed >= 300 && speed <= 3000) own_speed = (float)speed;
        }
    }
    if (!read_ptr(clientPtr + (uintptr_t)MODE_MANAGER_OFF, &value) || !value)
    {
        manager_ptr = 0;
        manager_array_ptr = 0;
        manager_count_v = 0;
        obj_count = 0;
        return;
    }
    mgrPtr = (uintptr_t)value;
    manager_ptr = mgrPtr;
    if (!read_int(mgrPtr + (uintptr_t)MGR_COUNT_OFF, &count) || count <= 0 ||
        count > OBJECT_COUNT_MAX)
    {
        manager_array_ptr = 0;
        manager_count_v = 0;
        obj_count = 0;
        return;
    }
    if (!read_ptr(mgrPtr + (uintptr_t)MGR_ARRAY_OFF, &value) || !value)
    {
        manager_array_ptr = 0;
        manager_count_v = 0;
        obj_count = 0;
        return;
    }
    array = (uintptr_t)value;
    if (count > COUNT_MAX) count = COUNT_MAX;
    if (!read_bytes(array, obj_snapshot, (size_t)count * sizeof(uintptr_t)))
    {
        manager_array_ptr = 0;
        manager_count_v = 0;
        obj_count = 0;
        return;
    }
    manager_array_ptr = array;
    manager_count_v = count;
    obj_count = count;
    if (own) remember_character(own_index_v, own);
    for (int i = 0; i < count; i++)
    {
        uintptr_t objPtr = obj_snapshot[i];
        obj_valid[i] = (objPtr && pointer_plausible(objPtr) &&
                        read_bytes(objPtr, obj_block[i], OBJ_BLOCK_SIZE))
                           ? 1
                           : 0;
        if (!obj_valid[i]) continue;
        int32_t ownerIdx = 0;
        int32_t team = 0;
        int32_t hpNow = 0;
        memcpy(&ownerIdx, obj_block[i] + OBJ_OWNERINDEX_OFF, sizeof(ownerIdx));
        memcpy(&team, obj_block[i] + OBJ_TEAM_OFF, sizeof(team));
        memcpy(&hpNow, obj_block[i] + OBJ_HP_OFF, sizeof(hpNow));
        if (team == own_team_b || hpNow <= 0) continue;
        if (ownerIdx < 0 || ownerIdx >= CHAR_REGISTRY_MAX) continue;
    }
    ticks_a++;
}
void run_workload(void)
{
    wall_maybe_refresh((uint64_t)(g_get_real_time() / 1000));
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
static void scan_frame(void)
{
    if (inside_hook)
    {
        return;
    }
    inside_hook = YES;
    run_workload();
    inside_hook = NO;
}
static void schedule_frame(void);
static void frame_tick(void)
{
    scan_frame();
    schedule_frame();
}
static void schedule_frame(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{ frame_tick(); });
}
void setup(void)
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
    addr_getownidx = callable(RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERINDEX);
    addr_movedto = callable(MOVE_RVA);
    addr_getdata = callable(RVA_LOGICGAMEOBJECTCLIENT_GETDATA);
    addr_getgid = callable(RVA_LOGICGAMEOBJECTCLIENT_GETGLOBALID);
    addr_charspeed = callable(RVA_LOGICCHARACTERDATA_GETSPEED);
    addr_charradius = callable(RVA_LOGICCHARACTERDATA_GETCOLLISIONRADIUS);
    addr_projbeam = callable(RVA_LOGICPROJECTILEDATA_ISBEAM);
    addr_projearly = callable(RVA_LOGICPROJECTILEDATA_GETNUMEARLYTICKS);
    addr_dataname = callable(RVA_LOGICDATA_GETNAME);
    addr_charweapon = callable(RVA_LOGICCHARACTERCLIENT_GETWEAPONSKILL);
    addr_charskillat = callable(RVA_LOGICCHARACTERCLIENT_GETSKILLAT);
    addr_skilldata = callable(RVA_LOGICSKILLCLIENT_GETDATA);
    addr_skillrange = callable(RVA_LOGICSKILLDATA_GETCASTINGRANGE);
    addr_skillproj = callable(RVA_LOGICSKILLDATA_GETPROJECTILEDATA);
}
int coord_ok = 0;
int dead = 0;
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
uintptr_t controller(void)
{
    return client();
}
uintptr_t scene_object = 0;
int own_team_b = -1;
uintptr_t own_elem = 0;
