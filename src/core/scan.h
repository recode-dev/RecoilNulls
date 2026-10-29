#ifndef SCAN_SCAN_H
#define SCAN_SCAN_H
#include "./offsets.h"
#include <cstdint>
#define OBJ_BLOCK_SIZE 0xe0
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
extern uintptr_t     base;
extern const char*   image_name;
extern BOOL          setup_done;
extern __thread BOOL inside_hook;
extern uintptr_t     addr_dataname;
BOOL                 query_region(uintptr_t       address,
                                  vm_prot_t*      protection,
                                  vm_prot_t*      maxProtection,
                                  mach_vm_size_t* regionSize,
                                  uintptr_t*      regionStart);
BOOL                 addr_writable(uintptr_t address, size_t length);
BOOL                 read_bytes(uintptr_t address, void* out, size_t length);
BOOL                 memory_readable(uintptr_t address, size_t length);
BOOL                 memory_writable(uintptr_t address, size_t length);
BOOL                 memory_protection_flags(uintptr_t address,
                                             vm_prot_t*      flags);
void                 read_cache_reset();
BOOL                 pointer_plausible(uintptr_t value);
BOOL                 read_int(uintptr_t address, int32_t* out);
BOOL                 writable(uintptr_t address, size_t length);
BOOL      write_bytes(uintptr_t address, const void* src, size_t length);
BOOL      read_ptr(uintptr_t address, void** out);
BOOL      read_ptr(uintptr_t address, uintptr_t* out);
void*     read_global_ptr(uintptr_t rva);
uintptr_t callable(uintptr_t rva);
int       game_tile_is_passable(uintptr_t tileMap, int cellX, int cellY);
int       game_tile_to_logic(int tile);
int       game_logic_to_tile(int unit);
BOOL      find_game_image(uintptr_t* out_base);
void      poll_for_game(int tick);
static inline BOOL region_flags(uintptr_t address, vm_prot_t* flags)
{
    vm_prot_t protection = 0;
    if (flags)
    {
        *flags = 0;
    }
    if (!memory_protection_flags(address, &protection))
    {
        return NO;
    }
    if (flags)
    {
        *flags = protection;
    }
    return YES;
}
extern int         gum_live;
extern uint32_t    workload_ticks;
extern uint64_t    workload_us_sum;
extern uint32_t    workload_us_max, workload_us_n;
static inline BOOL addr_readable(uintptr_t address, size_t length)
{
    return memory_readable(address, length);
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
    uintptr_t address = (uintptr_t) object;
    if (base && address < base)
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
inline constexpr int COUNT_MAX        = 512;
inline constexpr int OBJECT_COUNT_MAX = 512;
extern uintptr_t     manager_ptr;
extern uintptr_t     manager_array_ptr;
extern int32_t       manager_count_v;
extern uintptr_t     obj_snapshot[COUNT_MAX];
extern uint8_t       obj_block[COUNT_MAX][OBJ_BLOCK_SIZE];
extern uint8_t       obj_valid[COUNT_MAX];
extern int           obj_count;
extern uint32_t      own_hp;
extern float         own_speed;
extern float         own_vel_x;
extern float         own_vel_y;
extern int32_t       own_x;
extern int32_t       own_y;
extern int32_t       own_index_v;
extern int           own_ok_hp;
uint64_t             us();
void                 scan_world();
void                 run_workload();
void                 setup();
void                 run_autododge();
void                 diag_tick();
extern uintptr_t     scene_object;
int                  scene_live();
extern int           coord_ok;
extern int           dead;
extern uintptr_t     own_elem;
void                 resolve_addresses();
uintptr_t            client();
uintptr_t            controller();
extern int           own_team_b;
#endif
