#ifndef SCAN_SCAN_H
#define SCAN_SCAN_H
#include "./offsets.h"
#include <cstdint>
#define OBJ_BLOCK_SIZE 0xe0
#ifndef NSEC_PER_SEC
#    define NSEC_PER_SEC 1000000000ULL
#endif
#ifndef NSEC_PER_USEC
#    define NSEC_PER_USEC 1000ULL
#endif
#include <mach/mach.h>
#include <frida-gum.h>
extern uintptr_t     base;
extern const char*   image_name;
extern bool          setup_done;
extern __thread bool inside_hook;
bool                 addr_writable(uintptr_t address, size_t length);
bool                 read_bytes(uintptr_t address, void* out, size_t length);
bool                 memory_protection_flags(uintptr_t address, vm_prot_t* flags);
void                 read_cache_reset();
bool                 pointer_plausible(uintptr_t value);
bool                 read_int(uintptr_t address, int32_t* out);
bool                 write_bytes(uintptr_t address, const void* src, size_t length);
bool                 read_ptr(uintptr_t address, void** out);
bool                 read_ptr(uintptr_t address, uintptr_t* out);
uintptr_t            callable(uintptr_t rva);
int                  game_tile_is_passable(uintptr_t tileMap, int cellX, int cellY);
int                  game_path_clear(uintptr_t tileMap, int x0, int y0, int x1, int y1);
int                  game_tile_to_logic(int tile);
int  game_projectile_travel_ticks(uintptr_t projectileData, int distance, int speed);
bool find_game_image(uintptr_t* out_base);
void poll_for_game(int tick);

static inline bool region_flags(uintptr_t address, vm_prot_t* flags)
{
    vm_prot_t protection = 0;
    if (flags)
    {
        *flags = 0;
    }
    if (!memory_protection_flags(address, &protection))
    {
        return false;
    }
    if (flags)
    {
        *flags = protection;
    }
    return true;
}

extern int gum_live;

static inline bool addr_readable(uintptr_t address, size_t length)
{
    vm_prot_t flags = 0;
    if (!length) return false;
    return region_flags(address, &flags) && (flags & VM_PROT_READ);
}

static inline bool addr_executable(uintptr_t address)
{
    vm_prot_t flags = 0;
    if (!region_flags(address, &flags))
    {
        return false;
    }
    return (flags & VM_PROT_EXECUTE) != 0;
}

static inline bool object_plausible(void* object)
{
    if (!object)
    {
        return false;
    }
    uintptr_t address = (uintptr_t) object;
    if (base && address < base)
    {
        return false;
    }
    if (address & 7)
    {
        return false;
    }
    if (!addr_readable(address, sizeof(void*)))
    {
        return false;
    }
    uintptr_t vtable = 0;
    if (!read_bytes(address, &vtable, sizeof(vtable)))
    {
        return false;
    }
    if (!vtable)
    {
        return false;
    }
    if (!addr_readable(vtable, sizeof(void*)))
    {
        return false;
    }
    uintptr_t firstEntry = 0;
    if (!read_bytes(vtable, &firstEntry, sizeof(firstEntry)))
    {
        return false;
    }
    if (!firstEntry)
    {
        return false;
    }
    return addr_executable(firstEntry);
}

inline constexpr int  COUNT_MAX = 512;
extern uintptr_t      snapshot[COUNT_MAX];
extern uint8_t        block[COUNT_MAX][OBJ_BLOCK_SIZE];
extern uint8_t        valid[COUNT_MAX];
extern const uint8_t* own_block;
extern int            obj_count;
extern uint32_t       own_hp;
extern float          own_speed;
extern int32_t        own_x;
extern int32_t        own_y;
extern int            ok_hp;
uint64_t              us();
extern uint64_t       frame_index;
void                  scan_world();
void                  workload();
void                  setup();
void                  run_autododge();
extern uintptr_t      scene_object;
int                   is_owned_team_object(const uint8_t* obj);
int game_segment_hits_circle(int x1, int y1, int x2, int y2, int cx, int cy, int radius, int* out);
extern int       coord_ok;
extern int       dead;
extern uintptr_t elem;
uintptr_t        client();
uintptr_t        controller();
#endif
