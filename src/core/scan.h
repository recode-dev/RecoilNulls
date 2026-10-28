#ifndef SCAN_SCAN_H
#define SCAN_SCAN_H
#include "./offsets.h"
#include <cstdint>
#define OBJ_BLOCK_SIZE (OBJ_DEADFLAG_OFF + sizeof(uint32_t))
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000ull
#endif
#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC 1000000ull
#endif
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC 1000ull
#endif
#include <mach/mach.h>
#include <frida-gum.h>
#include <objc/runtime.h>
extern uintptr_t base;
extern const char* image_names[2];
extern BOOL setup_done;
extern __thread BOOL inside_hook;
extern uintptr_t addr_getinstance;
extern uintptr_t addr_getownchar;
extern uintptr_t addr_getteam;
extern uintptr_t addr_getx;
extern uintptr_t addr_gety;
extern uintptr_t addr_getdata;
extern uintptr_t addr_projbeam;
extern uintptr_t addr_projearly;
extern uintptr_t addr_dataname;
BOOL query_region(uintptr_t address,
                  vm_prot_t* protection,
                  vm_prot_t* maxProtection,
                  mach_vm_size_t* regionSize,
                  uintptr_t* regionStart);
BOOL addr_writable(uintptr_t address, size_t length);
BOOL read_bytes(uintptr_t address, void* out, size_t length);
BOOL pointer_plausible(uintptr_t value);
BOOL read_byte(uintptr_t address, uint8_t* out);
BOOL read_int(uintptr_t address, int32_t* out);
BOOL writable(uintptr_t address, size_t length);
BOOL write_bytes(uintptr_t address, const void* src, size_t length);
BOOL read_ptr(uintptr_t address, void** out);
void* read_global_ptr(uintptr_t rva);
uintptr_t callable(uintptr_t rva);
BOOL find_game_image(uintptr_t* out_base);
void poll_for_game(int tick);
static inline BOOL region_flags(uintptr_t address, vm_prot_t* flags)
{
    vm_prot_t protection = 0;
    if (flags)
    {
        *flags = 0;
    }
    if (!query_region(address, &protection, nullptr, nullptr, nullptr))
    {
        return NO;
    }
    if (flags)
    {
        *flags = protection;
    }
    return YES;
}
extern int gum_live;
static inline BOOL addr_readable(uintptr_t address, size_t length)
{
    if (!address || !length)
    {
        return NO;
    }
    if (!gum_live)
    {
        return NO;
    }
    return gum_memory_is_readable((gconstpointer)address,
                                  static_cast<gsize>(length))
               ? YES
               : NO;
}
static inline BOOL addr_executable(uintptr_t address)
{
    vm_prot_t flags = 0;
    if (!region_flags(address, &flags))
    {
        return NO;
    }
    return (flags & VM_PROT_EXECUTE) ? YES : NO;
}
static inline BOOL object_plausible(void* object)
{
    if (!object)
    {
        return NO;
    }
    uintptr_t address = (uintptr_t)object;
    if (address < HEAP_REGION_MAX_SIZE)
    {
        return NO;
    }
    if (address & 7)
    {
        return NO;
    }
    if (!addr_readable(address, sizeof(void*)))
    {
        return NO;
    }
    uintptr_t vtable = 0;
    if (!read_bytes(address, &vtable, sizeof(vtable)))
    {
        return NO;
    }
    if (!vtable)
    {
        return NO;
    }
    if (!addr_readable(vtable, sizeof(void*)))
    {
        return NO;
    }
    uintptr_t firstEntry = 0;
    if (!read_bytes(vtable, &firstEntry, sizeof(firstEntry)))
    {
        return NO;
    }
    if (!firstEntry)
    {
        return NO;
    }
    return addr_executable(firstEntry);
}
inline constexpr int COUNT_MAX = 1024;
inline constexpr int OBJECT_COUNT_MAX = 1024;
extern uintptr_t manager_ptr;
extern uintptr_t manager_array_ptr;
extern int32_t manager_count_v;
extern uintptr_t obj_snapshot[COUNT_MAX];
extern uint8_t obj_block[COUNT_MAX][OBJ_BLOCK_SIZE];
extern uint8_t obj_valid[COUNT_MAX];
extern int obj_count;
extern uint64_t ticks_a;
extern uint32_t own_hp;
extern float own_r;
extern float own_speed;
extern int32_t own_x;
extern int32_t own_y;
extern int32_t own_index_v;
extern int own_ok_hp;
uint64_t us();
void scan_world();
void run_workload();
void setup();
void run_autododge();
enum : uint32_t
{
    SCAN_AD_INVALID_SNAPSHOT = 1,
    SCAN_AD_BAD_DATA_POINTER,
    SCAN_AD_DATA_READ_FAIL,
    SCAN_AD_BAD_VTABLE,
    SCAN_AD_NOT_PROJECTILE_VTABLE,
    SCAN_AD_ZERO_PID,
    SCAN_AD_OWN_TEAM,
    SCAN_AD_DEAD,
    SCAN_AD_INDIRECT_READ_FAIL,
    SCAN_AD_OUT_OF_RANGE,
    SCAN_AD_BEHIND,
    SCAN_AD_WALL_BLOCKED,
    SCAN_AD_BLOB,
    SCAN_AD_NO_VELOCITY,
    SCAN_AD_THREAT_CANDIDATE,
    SCAN_AD_CACHE_REBUILT,
    SCAN_AD_CACHE_INDEX_REPAIRED,
    SCAN_AD_MAX_THREATS,
    SCAN_AD_REASON_COUNT
};
void autododge_scanner_event(uint32_t reason);
void autododge_scanner_report();
void autododge_scanner_log(uint64_t now, const char* format, ...);
void struct_scan_capture_input_message(uintptr_t message);
extern uintptr_t scene_object;
int scene_live();
extern int coord_ok;
extern int dead;
extern uintptr_t own_elem;
void resolve_addresses();
uintptr_t client();
uintptr_t controller();
extern int own_team_b;
#endif
