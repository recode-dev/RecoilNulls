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
#include <CoreFoundation/CFDate.h>
#include <frida-gum.h>
#include <mach-o/loader.h>
#include <objc/runtime.h>
extern uintptr_t base;
extern const char* image_names[2];
extern uintptr_t* starts;
extern size_t starts_count;
extern BOOL setup_done;
extern __thread BOOL inside_hook;
extern BOOL mode_strong;
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
extern uintptr_t addr_castrange;
float own_cast_range(void);
float range_for_proj(uintptr_t projData, int32_t ownerIdx);
int range_map_size(void);
extern uintptr_t owner;
extern int wired;
extern dispatch_source_t scan_timer;
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
BOOL writable(uintptr_t address, size_t length);
BOOL write_bytes(uintptr_t address, const void* src, size_t length);
BOOL read_ptr(uintptr_t address, void** out);
void* read_global_ptr(uintptr_t rva);
uintptr_t callable(uintptr_t rva);
BOOL copy(uintptr_t source, void* destination, size_t length);
BOOL text_section(uintptr_t* address, uint64_t* size);
BOOL find_game_image(uintptr_t* out_base);
const char* image_segment_name(uintptr_t value);
BOOL vtable_shaped(uintptr_t value);
BOOL heap_resident(uintptr_t value);
BOOL manager_shape(uintptr_t manager);
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
    return gum_memory_is_readable((gconstpointer)address, (gsize)length) ? YES
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
    int mapn;
    int ranged;
    int thn;
    float thx[6];
    float thy[6];
    float thvx[6];
    float thvy[6];
    float thrad[6];
    char note[64];
} dbg_t;
extern dbg_t dbg;
uint64_t us(void);
void load_function_starts(void);
BOOL start_boundary(const uint8_t* bytes, size_t offset);
BOOL start_word(uint32_t word);
extern uintptr_t pub_array;
extern int32_t pub_count;
extern uintptr_t pub_object;
extern volatile uint32_t seq;
extern uintptr_t site;
extern int site_state;
extern uintptr_t tick_array;
extern int32_t tick_count;
extern uintptr_t tick_object;
void publish(uintptr_t object,
             uintptr_t array,
             int32_t count,
             int32_t cap,
             const char* why);
int snapshot(uintptr_t* objectOut, uintptr_t* arrayOut, int32_t* countOut);
void tick_begin(void);
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
extern int hop_chosen;
extern int hop_sticky;
extern uintptr_t hop_scene;
extern int last_choice;
extern int hop_logs;
extern int coord_ok;
extern int dead;
extern int own_team_a;
extern int own_team_seen;
extern uintptr_t own_elem;
#define WORK_GAP_US 200
#define HOT_INTERVAL_US 2000
int container_header(uintptr_t object,
                     uintptr_t* arrayOut,
                     int32_t* countOut,
                     int32_t* capOut);
int32_t gid(uintptr_t element, int32_t* offOut);
int container_score(uintptr_t container);
void resolve_addresses(void);
void locate_battle_mode(void);
uintptr_t hop(uintptr_t base, int* whyOut);
uintptr_t client(void);
void state_note(int state);
uintptr_t controller(void);
const char* header_reason(uintptr_t mgrPtr, int32_t* countOut, int32_t* capOut);
int ctrl_bounds(uintptr_t base, int32_t* wOut, int32_t* hOut);
extern int own_team_b;
#define POS_BONUS 8
#define DIST_BONUS 6
#define SOFT_BASE 2
#define SOFT_MIN_POS 2
#define SOFT_MIN_DIST 2
#define COUNT_MAX 96
#define ASCII_RATIO 30
#define COORD_MAX 100000
#define VOTESCAN_INTERVAL 1.0
#define VOTESCAN_ATTEMPTS 600
int ascii_word(uintptr_t address);
int word_ascii(uint64_t value);
int element_ascii(uintptr_t element);
#endif
