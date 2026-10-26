#ifndef SCAN_SCAN_H
#define SCAN_SCAN_H
#include "./offsets.h"
#include <stdint.h>
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000ull
#endif
#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC 1000000ull
#endif
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC 1000ull
#endif
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <string.h>
typedef struct
{
    int32_t gid;
    int32_t team;
    int dead;
} objhit_t;
typedef uint64_t (*slot_fn_t)(void* a0,
                              uint64_t a1,
                              uint64_t a2,
                              uint64_t a3,
                              uint64_t a4,
                              uint64_t a5,
                              uint64_t a6,
                              uint64_t a7);
typedef struct
{
    int32_t count;
} trail_t;
typedef struct
{
    uintptr_t object;
    int32_t gid;
    int32_t x;
    int32_t y;
    int32_t ownerIndex;
    int32_t teamOld;
    int32_t teamNew;
    int32_t typeWord;
    uint8_t dead;
    uint8_t activeFlag;
} obj_t;
#include <CoreFoundation/CFDate.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <objc/runtime.h>
extern uintptr_t base;
extern const char* image_names[4];
extern uintptr_t* starts;
extern size_t starts_count;
extern BOOL setup_done;
extern __thread BOOL inside_hook;
extern int dump_np;
extern BOOL mode_strong;
extern uintptr_t players_object;
extern uintptr_t objvote_best_owner;
extern int objvote_best_teamcount;
extern objhit_t objhits[64];
extern int objhit_count;
extern int objvote_max_votes;
extern int trail_best;
extern int votescan_attempts;
extern double votescan_last;
extern uintptr_t addr_getinstance;
extern uintptr_t addr_getownchar;
extern uintptr_t addr_getteam;
extern uintptr_t addr_getx;
extern uintptr_t addr_gety;
extern uintptr_t addr_battlescreen;
extern uintptr_t addr_projspeed;
extern uintptr_t addr_projradius;
extern uintptr_t addr_projbeam;
extern volatile int at;
extern int sig_ticks;
extern uintptr_t sig_last;
extern uintptr_t players_array;
extern int players_count;
extern uint64_t walk_tick;
extern int walk_count;
extern uintptr_t owner;
extern int wired;
extern dispatch_source_t scan_timer;
extern trail_t trail[8];
extern int trail_count;
extern uintptr_t own_elem_scan;
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
BOOL read_float(uintptr_t address, float* out);
BOOL writable(uintptr_t address, size_t length);
BOOL write_bytes(uintptr_t address, const void* src, size_t length);
BOOL read_ptr(uintptr_t address, void** out);
void* read_global_ptr(uintptr_t rva);
uintptr_t callable(uintptr_t rva);
BOOL copy(uintptr_t source, void* destination, size_t length);
BOOL text_section(uintptr_t* address, uint64_t* size);
BOOL valid_header(uintptr_t imageBase);
BOOL find_game_image(uintptr_t* out_base);
BOOL segment_range(const char* name, uintptr_t* lo, uintptr_t* hi);
void image_span_refresh(void);
const char* image_segment_name(uintptr_t value);
BOOL vtable_shaped(uintptr_t value);
BOOL heap_resident(uintptr_t value);
BOOL instance_shaped(uintptr_t object);
BOOL manager_shape(uintptr_t manager);
BOOL vtable_in_image(uintptr_t vtable);
uintptr_t strip_ptr(uintptr_t value);
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
static inline BOOL addr_readable(uintptr_t address, size_t length)
{
    if (!address || !length)
    {
        return NO;
    }
    uintptr_t end = address + length;
    if (end < address)
    {
        return NO;
    }
    uintptr_t cursor = address;
    for (int guard = 0; cursor < end && guard < 64; guard++)
    {
        vm_prot_t flags = 0;
        if (!region_flags(cursor, &flags))
        {
            return NO;
        }
        if ((flags & VM_PROT_READ) == 0)
        {
            return NO;
        }
        vm_address_t region = (vm_address_t)cursor;
        vm_size_t size = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t infoCount = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t objectName = MACH_PORT_NULL;
        kern_return_t kr = vm_region_64(mach_task_self(),
                                        &region,
                                        &size,
                                        VM_REGION_BASIC_INFO_64,
                                        (vm_region_info_t)&info,
                                        &infoCount,
                                        &objectName);
        if (objectName != MACH_PORT_NULL)
        {
            mach_port_deallocate(mach_task_self(), objectName);
        }
        if (kr != KERN_SUCCESS || size == 0)
        {
            return NO;
        }
        uintptr_t next = (uintptr_t)region + (uintptr_t)size;
        if (next <= cursor)
        {
            return NO;
        }
        cursor = next;
    }
    return cursor >= end;
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
static inline uintptr_t image_slide(uintptr_t imageBase)
{
    if (!imageBase)
    {
        return 0;
    }
    const struct mach_header_64* header =
        (const struct mach_header_64*)imageBase;
    if (!addr_readable(imageBase, sizeof(struct mach_header_64)))
    {
        return 0;
    }
    if (header->magic != MH_MAGIC_64)
    {
        return 0;
    }
    if (header->ncmds == 0 || header->ncmds > 4096)
    {
        return 0;
    }
    if (header->sizeofcmds == 0 || header->sizeofcmds > (4u * 1024u * 1024u))
    {
        return 0;
    }
    const uint8_t* cursor = (const uint8_t*)(header + 1);
    const uint8_t* limit = cursor + header->sizeofcmds;
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (cursor + sizeof(struct load_command) > limit)
        {
            return 0;
        }
        const struct load_command* command = (const struct load_command*)cursor;
        if (command->cmdsize < sizeof(struct load_command))
        {
            return 0;
        }
        if (cursor + command->cmdsize > limit)
        {
            return 0;
        }
        if (command->cmd == LC_SEGMENT_64)
        {
            if (command->cmdsize < sizeof(struct segment_command_64))
            {
                return 0;
            }
            const struct segment_command_64* segment =
                (const struct segment_command_64*)command;
            if (strcmp(segment->segname, "__TEXT") == 0)
            {
                return imageBase - (uintptr_t)segment->vmaddr;
            }
        }
        cursor += command->cmdsize;
    }
    return 0;
}
static inline BOOL image_segment_contains(uintptr_t imageBase,
                                          uintptr_t address,
                                          BOOL requireExec,
                                          uintptr_t* outStart,
                                          uintptr_t* outEnd,
                                          uint32_t* outProt)
{
    if (!imageBase || !address)
    {
        return NO;
    }
    const struct mach_header_64* header =
        (const struct mach_header_64*)imageBase;
    if (!addr_readable(imageBase, sizeof(struct mach_header_64)))
    {
        return NO;
    }
    if (header->magic != MH_MAGIC_64)
    {
        return NO;
    }
    if (header->ncmds == 0 || header->ncmds > 4096)
    {
        return NO;
    }
    if (header->sizeofcmds == 0 || header->sizeofcmds > (4u * 1024u * 1024u))
    {
        return NO;
    }
    uintptr_t slide = image_slide(imageBase);
    const uint8_t* cursor = (const uint8_t*)(header + 1);
    const uint8_t* limit = cursor + header->sizeofcmds;
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (cursor + sizeof(struct load_command) > limit)
        {
            return NO;
        }
        const struct load_command* command = (const struct load_command*)cursor;
        if (command->cmdsize < sizeof(struct load_command))
        {
            return NO;
        }
        if (cursor + command->cmdsize > limit)
        {
            return NO;
        }
        if (command->cmd == LC_SEGMENT_64)
        {
            if (command->cmdsize < sizeof(struct segment_command_64))
            {
                return NO;
            }
            const struct segment_command_64* segment =
                (const struct segment_command_64*)command;
            uintptr_t start = (uintptr_t)segment->vmaddr + slide;
            uintptr_t end = start + (uintptr_t)segment->vmsize;
            if (address >= start && address < end)
            {
                BOOL executable =
                    (segment->initprot & VM_PROT_EXECUTE) ? YES : NO;
                if (requireExec && !executable)
                {
                    return NO;
                }
                if (outStart)
                {
                    *outStart = start;
                }
                if (outEnd)
                {
                    *outEnd = end;
                }
                if (outProt)
                {
                    *outProt = (uint32_t)segment->initprot;
                }
                return YES;
            }
        }
        cursor += command->cmdsize;
    }
    return NO;
}
static inline BOOL image_text_contains(uintptr_t imageBase, uintptr_t address)
{
    uintptr_t start = 0;
    uintptr_t end = 0;
    uint32_t prot = 0;
    if (!image_segment_contains(imageBase, address, NO, &start, &end, &prot))
    {
        return NO;
    }
    return (prot & VM_PROT_EXECUTE) ? YES : NO;
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
extern uintptr_t manager_ptr;
extern uintptr_t manager_array_ptr;
extern int32_t manager_count_v;
extern uint64_t ticks_a;
extern uint32_t own_hp;
extern float own_r;
extern float own_speed;
extern int32_t own_x;
extern int32_t own_y;
extern int32_t own_index_v;
extern int own_ok_hp;
typedef struct
{
    int stage;
    int hazards;
    int threats;
    int danger;
    float px;
    float py;
    float ix;
    float iy;
    float dx;
    float dy;
    float tx;
    float ty;
    float radius;
    float speed;
    int sent;
    int thn;
    float thx[6];
    float thy[6];
    float thvx[6];
    float thvy[6];
    float thrad[6];
    char note[64];
} dbg_t;
extern dbg_t dbg;
extern int logs_a;
extern int seeded;
extern int32_t last_x_a;
extern int32_t last_y_a;
int collect(uintptr_t mgrPtr, obj_t* out, int capacity);
uint64_t us(void);
#define COORD_SOFT 0
#define AREA_RADIUS_MIN 1
#define AREA_RADIUS_MAX 6000
#define AREA_TIME_MIN 1
#define AREA_TIME_MAX 60000
void load_function_starts(void);
BOOL start_boundary(const uint8_t* bytes, size_t offset);
BOOL start_word(uint32_t word);
#include "hook.h"
extern int fb_on;
extern uint64_t idle_start;
extern int live_objs;
extern int live_teams;
extern unsigned long long obj_prev;
extern uintptr_t pub_array;
extern int32_t pub_count;
extern int pub_logs;
extern uintptr_t pub_object;
extern int scan_armed;
extern volatile uint32_t seq;
extern uintptr_t setpred;
extern uintptr_t site;
extern slot_fn_t slot_orig[34];
extern const hook_t slot_specs[34];
extern int site_state;
extern uintptr_t tick_array;
extern int32_t tick_count;
extern uintptr_t tick_object;
extern const int object_slots[3];
void publish(uintptr_t object,
             uintptr_t array,
             int32_t count,
             int32_t cap,
             const char* why);
void slot_hooks_install(void);
void slot_pump(void);
uint64_t slot_repl_0(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
uint64_t slot_repl_1(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
uint64_t slot_repl_10(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_11(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_12(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_13(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_14(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_15(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_16(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_17(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_18(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_19(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_2(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
uint64_t slot_repl_20(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_21(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_22(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_23(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_24(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_25(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_26(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_27(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_28(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_29(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_3(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
uint64_t slot_repl_30(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_31(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_32(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_33(void* a0,
                      uint64_t a1,
                      uint64_t a2,
                      uint64_t a3,
                      uint64_t a4,
                      uint64_t a5,
                      uint64_t a6,
                      uint64_t a7);
uint64_t slot_repl_4(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
uint64_t slot_repl_5(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
uint64_t slot_repl_6(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
uint64_t slot_repl_7(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
uint64_t slot_repl_8(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
uint64_t slot_repl_9(void* a0,
                     uint64_t a1,
                     uint64_t a2,
                     uint64_t a3,
                     uint64_t a4,
                     uint64_t a5,
                     uint64_t a6,
                     uint64_t a7);
int snapshot(uintptr_t* objectOut, uintptr_t* arrayOut, int32_t* countOut);
void tick_begin(void);
extern uintptr_t slot_object[34];
extern uintptr_t slot_arg[34];
extern uint64_t slot_hits[34];
extern uintptr_t slot_adopted;
extern int battle_active;
extern int battle_last_tick;
extern int hb_sig_prev;
extern uint64_t ticks_b;
void start_timer(void);
int state_tick(void);
void scan_world(void);
void run_workload(void);
void setup(void);
void run_autododge(void);
#define WIRE_OWNER 1
#define HOPCHOSEN_DIRECT 2
#define ARRAY_VOTE_LOGS 8
extern uintptr_t scene_object;
int scene_live(void);
extern int prev_state;
extern uintptr_t own_ptr_a;
extern int hop_chosen;
extern int hop_sticky;
extern int modesig_hits;
extern uintptr_t own_ptr_b;
extern int own_index_3;
extern uintptr_t scan_container;
extern uintptr_t hop_scene;
extern int last_choice;
extern int hop_logs;
extern uint64_t probe_last_ms;
extern int coord_ok;
extern int coord_usable;
extern int gidless;
extern int dead;
extern int own_team_a;
extern int own_team_seen;
extern uintptr_t proj_addr;
extern uint8_t proj_bytes[0x100];
extern int proj_have;
extern int proj_dumps;
extern uint64_t proj_diff_logs;
extern uintptr_t own_elem;
extern int32_t own_gid;
extern int no_source_passes;
typedef struct
{
    const char* name;
    float angle;
    int32_t spawnX;
    int32_t spawnY;
    int32_t x;
    int32_t y;
} proj_death_t;
typedef struct
{
    float x;
    float y;
    float radius;
    const char* name;
} hazard_t;
#define WORK_GAP_US 200
#define HOT_INTERVAL_US 2000
typedef struct
{
    uintptr_t elem;
    uintptr_t classRva;
    int32_t x;
    int32_t y;
    int32_t px;
    int32_t py;
    int32_t team;
    int32_t spawnX;
    int32_t spawnY;
    int32_t gid;
    uint64_t pms;
    uint64_t qms;
    int hasPrev;
    const char* name;
    int32_t targetX;
    int32_t targetY;
    float radius;
    float angle;
    uint64_t spawnedAt;
    float speed;
    float vx;
    float vy;
    int isThrower;
    int isBeam;
    int32_t spawnAreaRadius;
    int32_t spawnAreaActiveTime;
    int32_t castRange;
} proj_t;
extern proj_t projs[16];
extern proj_death_t proj_deaths[16];
extern int proj_death_n;
int element_type(uintptr_t vt, uintptr_t* wordOut);
int container_header(uintptr_t object,
                     uintptr_t* arrayOut,
                     int32_t* countOut,
                     int32_t* capOut);
int32_t gid_at(uintptr_t element, uintptr_t off);
int32_t gid(uintptr_t element, int32_t* offOut);
int container_score(uintptr_t container);
void resolve_addresses(void);
void locate_battle_mode(void);
void gidless_scan(uintptr_t mgrPtr);
uintptr_t list_gid_off(uintptr_t array, int32_t count);
void proj_track(uintptr_t elem, uintptr_t classRva, int32_t gid, int32_t team);
uintptr_t hop(uintptr_t base, int* whyOut);
uintptr_t client(void);
void state_note(int state);
int proj_scan(uintptr_t mgrPtr,
              int32_t count,
              const obj_t* rosterArr,
              int rosterUsable);
uintptr_t controller(void);
const char* header_reason(uintptr_t mgrPtr, int32_t* countOut, int32_t* capOut);
uintptr_t coord_x_off(void);
uintptr_t coord_y_off(void);
int ctrl_bounds(uintptr_t base, int32_t* wOut, int32_t* hOut);
extern int cand_changes[3];
extern int cand_frame[3];
extern int dead_slot;
extern int dead_value;
extern int enemy_n;
extern int32_t enemy_x[12];
extern int32_t enemy_y[12];
extern int mate_n;
extern int32_t mate_x[8];
extern int32_t mate_y[8];
extern const char* own_from;
extern int own_index;
extern uintptr_t own_ptr;
extern int own_team_b;
extern int pending;
extern int pending_tick;
extern int pl_n;
extern int32_t pl_team[12];
extern int32_t pl_x[12];
extern int32_t pl_y[12];
extern int pre[3];
extern int prev_valid;
extern int respawn_tick;
extern int state_code;
extern int team_trust;
int own_side_spawn(int32_t sx, int32_t sy);
void roster(uintptr_t ownElem,
            int ownIndex,
            int ownTeam,
            const obj_t* objects,
            int usable);
int team_at(const obj_t* objects, int index);
#define CLUSTER 10000.0f
#define GIDLESS 1
#define POS_BONUS 8
#define DIST_BONUS 6
#define SOFT_BASE 2
#define SOFT_MIN_POS 2
#define SOFT_MIN_DIST 2
#define ATTRIB_R (1200.0f * 1200.0f)
#define ATTRIB_MARGIN 1.2f
#define STATE_INIT 0
#define DUMPS 3
#define DIFF_LOGS 48
#define COUNT_MAX 96
#define ASCII_RATIO 30
#define COORD_MAX 100000
#define VOTESCAN_GLOBAL_EVERY 10
#define VOTESCAN_INTERVAL 1.0
#define VOTESCAN_ATTEMPTS 600
int ascii_word(uintptr_t address);
int word_ascii(uint64_t value);
int element_ascii(uintptr_t element);
#endif
