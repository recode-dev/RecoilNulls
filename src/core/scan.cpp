#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "../features/autododge.h"
#include "../utils/walls.h"
#include "offsets.h"
#include "scan.h"
#include "scstring.h"
#include <frida-gum.h>
const char* image_name = "Nulls Brawl";
BOOL setup_done = NO;
__thread BOOL inside_hook = NO;
uintptr_t addr_dataname = 0;
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
    if (!last_pos_us || now_us <= last_pos_us)
    {
        last_pos_x = own_x;
        last_pos_y = own_y;
        last_pos_us = now_us;
        return;
    }
    float dt = (float)(now_us - last_pos_us) / 1000000.0f;
    if (dt < 0.06f) return; // window not long enough yet, keep accumulating
    // The sample point has to advance even when the sample is rejected. It
    // used to advance only inside the accept branch, so the first stop or
    // loading stall (dt >= 0.25) left dt above the limit for good and the
    // estimate stayed frozen for the rest of the session - the dodge then
    // planned with a stale speed and cleared threats it could not clear.
    if (dt < 0.25f)
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
    if (last_us && now_us - last_us < 5000000ULL)
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
static constexpr uint64_t TICK_US = 50000ULL;
// The scan loop is not a 50 ms tick. It runs at 120 Hz (8.33 ms) so a heading
// change can go out on the next frame; ad::TICK_MS == 50 is the step of the
// prediction grid inside the dodge and has nothing to do with this period.
// The two used to look like the same thing, which is why the dense log rows
// were read as 50 ms frames when they are 8 ms frames.
static constexpr uint64_t SCAN_PERIOD_US = TICK_US / 6;
// How long one pass over the world costs. The timer runs on the main queue,
// so every microsecond here is taken straight out of the game's own frame -
// that is the latency the player feels.
uint64_t workload_us_sum = 0;
uint32_t workload_us_max = 0;
uint32_t workload_us_n = 0;
static void scan_frame()
{
    if (inside_hook)
    {
        return;
    }
    inside_hook = YES;
    read_cache_reset();
    uint64_t t0 = us();
    run_workload();
    uint64_t spent = us() - t0;
    workload_us_sum += spent;
    if (spent > workload_us_max) workload_us_max = (uint32_t)spent;
    workload_us_n++;
    inside_hook = NO;
}
static dispatch_source_t scan_timer = nullptr;
static void start_scan_timer()
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
    resolve_addresses();
    start_scan_timer();
    diag_tick();
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
