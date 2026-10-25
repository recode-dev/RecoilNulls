#include "../data.hpp"

const char *rcl_image_names[4] = {"Nulls Brawl", "Laser", "NB.app", nullptr};

BOOL rcl_setup_done = NO;

__thread BOOL rcl_inside_hook = NO;


BOOL rcl_mode_strong = NO;

uintptr_t rcl_objvote_best_owner = 0;

int rcl_objvote_best_teamcount = 0;

rcl_objhit_t rcl_objhits[64];

int rcl_objhit_count = 0;

int rcl_objvote_max_votes = 0;

int rcl_trail_best = 0;

int rcl_votescan_attempts = 0;

double rcl_votescan_last = 0.0;




uintptr_t rcl_addr_getinstance = 0;

uintptr_t rcl_addr_getownchar = 0;

uintptr_t rcl_addr_getteam = 0;

uintptr_t rcl_addr_getx = 0;

uintptr_t rcl_addr_gety = 0;

uintptr_t rcl_addr_battlescreen = 0;

uintptr_t rcl_addr_getownidx = 0;
uintptr_t rcl_addr_movedto = 0;
uintptr_t rcl_addr_getdata = 0;
uintptr_t rcl_addr_getgid = 0;
uintptr_t rcl_addr_charspeed = 0;
uintptr_t rcl_addr_charradius = 0;
uintptr_t rcl_addr_projspeed = 0;
uintptr_t rcl_addr_projradius = 0;
uintptr_t rcl_addr_projbeam = 0;

volatile int rcl_at = 0;

uintptr_t rcl_slot_object[34] = {0};

uintptr_t rcl_slot_arg[34] = {0};

uintptr_t rcl_slot_adopted = 0;

int rcl_sig_ticks = 0;

uintptr_t rcl_sig_last = 0;

uint64_t rcl_walk_tick = 0;

int rcl_walk_count = -1;

BOOL rcl_query_region(uintptr_t address, vm_prot_t *protection, vm_prot_t *maxProtection,
                      mach_vm_size_t *regionSize, uintptr_t *regionStart)
{
    vm_address_t regionAddress = (vm_address_t)address;
    vm_size_t size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t infoCount = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t objectName = MACH_PORT_NULL;
    kern_return_t result =
        vm_region_64(mach_task_self(), &regionAddress, &size, VM_REGION_BASIC_INFO_64,
                     (vm_region_info_t)&info, &infoCount, &objectName);
    if (objectName != MACH_PORT_NULL)
    {
        mach_port_deallocate(mach_task_self(), objectName);
    }
    if (result != KERN_SUCCESS || size == 0)
    {
        return NO;
    }
    if ((uintptr_t)regionAddress > address)
    {
        return NO;
    }
    if ((uintptr_t)regionAddress + (uintptr_t)size <= address)
    {
        return NO;
    }
    if (protection)
    {
        *protection = info.protection;
    }
    if (maxProtection)
    {
        *maxProtection = info.max_protection;
    }
    if (regionSize)
    {
        *regionSize = (mach_vm_size_t)size;
    }
    if (regionStart)
    {
        *regionStart = (uintptr_t)regionAddress;
    }
    return YES;
}

BOOL rcl_addr_writable(uintptr_t address, size_t length)
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
        vm_prot_t protection = 0;
        mach_vm_size_t size = 0;
        uintptr_t start = 0;
        if (!rcl_query_region(cursor, &protection, nullptr, &size, &start))
        {
            return NO;
        }
        if (size == 0)
        {
            return NO;
        }
        if ((protection & VM_PROT_WRITE) == 0)
        {
            return NO;
        }
        uintptr_t next = start + (uintptr_t)size;
        if (next <= cursor)
        {
            return NO;
        }
        cursor = next;
    }
    return cursor >= end;
}

BOOL rcl_read_bytes(uintptr_t address, void *out, size_t length)
{
    if (!out || !length)
    {
        return NO;
    }
    if (!address)
    {
        return NO;
    }
    vm_size_t got = 0;
    kern_return_t result =
        vm_read_overwrite(mach_task_self(), (mach_vm_address_t)address, (mach_vm_size_t)length,
                          (mach_vm_address_t)(uintptr_t)out, &got);
    return result == KERN_SUCCESS && got == (vm_size_t)length;
}

BOOL rcl_pointer_plausible(uintptr_t value)
{
    if (value < 0x10000)
    {
        return NO;
    }
    if (value & 7)
    {
        return NO;
    }
    return YES;
}

BOOL rcl_read_byte(uintptr_t address, uint8_t *out)
{
    return rcl_read_bytes(address, out, 1);
}

BOOL rcl_read_int(uintptr_t address, int32_t *out)
{
    if (!out)
    {
        return NO;
    }
    if (address & 3)
    {
        return NO;
    }
    return rcl_read_bytes(address, out, 4);
}

BOOL rcl_read_float(uintptr_t address, float *out)
{
    if (address & 3)
    {
        return NO;
    }
    return rcl_read_bytes(address, out, 4);
}

BOOL rcl_writable(uintptr_t address, size_t length)
{
    uintptr_t end = address + length;
    uintptr_t cursor = address;
    if (!address || !length)
    {
        return NO;
    }
    if (end < address)
    {
        return NO;
    }
    for (int guard = 0; cursor < end && guard < 64; guard++)
    {
        vm_prot_t protection = 0;
        mach_vm_size_t size = 0;
        uintptr_t start = 0;
        uintptr_t next = 0;
        if (!rcl_query_region(cursor, &protection, nullptr, &size, &start))
        {
            return NO;
        }
        if ((protection & VM_PROT_WRITE) == 0)
        {
            return NO;
        }
        if (size == 0)
        {
            return NO;
        }
        next = start + (uintptr_t)size;
        if (next <= cursor)
        {
            return NO;
        }
        cursor = next;
    }
    return cursor >= end;
}


BOOL rcl_write_bytes(uintptr_t address, const void *src, size_t length)
{
    if (!src || !length)
    {
        return NO;
    }
    if (!address)
    {
        return NO;
    }
    if (1 && !rcl_writable(address, length))
    {
        return NO;
    }
    memcpy((void *)address, src, length);
    return YES;
}
BOOL rcl_read_ptr(uintptr_t address, void **out)
{
    if (!out)
    {
        return NO;
    }
    if (address & 7)
    {
        return NO;
    }
    return rcl_read_bytes(address, out, sizeof(void *));
}

void *rcl_read_global_ptr(uintptr_t rva)
{
    if (!rcl_base || !rva)
    {
        return nullptr;
    }
    void *value = nullptr;
    if (!rcl_read_ptr(rcl_base + rva, &value))
    {
        return nullptr;
    }
    return value;
}

uintptr_t rcl_callable(uintptr_t rva)
{
    if (!rcl_base || !rva)
    {
        return 0;
    }
    uintptr_t address = rcl_base + rva;
    if (address < rcl_base || address >= rcl_base + 0x2000000ULL)
    {
        return 0;
    }
    return address;
}
BOOL rcl_copy(uintptr_t source, void *destination, size_t length)
{
    if (!source || !destination || !length)
    {
        return NO;
    }
    if (!rcl_addr_readable(source, length))
    {
        return NO;
    }
    vm_size_t copied = 0;
    kern_return_t result =
        vm_read_overwrite(mach_task_self(), (mach_vm_address_t)source, (mach_vm_size_t)length,
                          (mach_vm_address_t)(uintptr_t)destination, &copied);
    return result == KERN_SUCCESS && copied == (vm_size_t)length;
}

BOOL rcl_text_section(uintptr_t *address, uint64_t *size)
{
    if (!rcl_base)
    {
        return NO;
    }
    if (!rcl_addr_readable(rcl_base, sizeof(struct mach_header_64)))
    {
        return NO;
    }
    const struct mach_header_64 *header = (const struct mach_header_64 *)rcl_base;
    if (header->magic != MH_MAGIC_64)
    {
        return NO;
    }
    const uint8_t *cursor = (const uint8_t *)(header + 1);
    const uint8_t *limit = cursor + header->sizeofcmds;
    uintptr_t slide = rcl_image_slide(rcl_base);
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (cursor + sizeof(struct load_command) > limit)
        {
            return NO;
        }
        const struct load_command *command = (const struct load_command *)cursor;
        if (command->cmdsize < sizeof(struct load_command))
        {
            return NO;
        }
        if (cursor + command->cmdsize > limit)
        {
            return NO;
        }
        if (command->cmd == LC_SEGMENT_64 && command->cmdsize >= sizeof(struct segment_command_64))
        {
            const struct segment_command_64 *segment = (const struct segment_command_64 *)command;
            if (strcmp(segment->segname, "__TEXT") == 0)
            {
                uint64_t room = (uint64_t)command->cmdsize - sizeof(struct segment_command_64);
                uint64_t count = room / sizeof(struct section_64);
                if (count > segment->nsects)
                {
                    count = segment->nsects;
                }
                const struct section_64 *sections = (const struct section_64 *)(segment + 1);
                for (uint64_t s = 0; s < count; s++)
                {
                    if (strcmp(sections[s].sectname, "__text") != 0)
                    {
                        continue;
                    }
                    if (!sections[s].size)
                    {
                        continue;
                    }
                    if (address)
                    {
                        *address = slide + (uintptr_t)sections[s].addr;
                    }
                    if (size)
                    {
                        *size = sections[s].size;
                    }
                    return YES;
                }
            }
        }
        cursor += command->cmdsize;
    }
    return NO;
}

BOOL rcl_valid_header(uintptr_t base)
{
    if (!base)
    {
        return NO;
    }
    struct mach_header_64 header;
    if (!rcl_pointer_plausible(base))
    {
        return NO;
    }
    if (!rcl_read_bytes(base, &header, sizeof(header)))
    {
        return NO;
    }
    if (header.magic != MH_MAGIC_64)
    {
        return NO;
    }
    if (header.ncmds == 0 || header.ncmds > 4096)
    {
        return NO;
    }
    if (header.sizeofcmds == 0)
    {
        return NO;
    }
    if (header.sizeofcmds > (4u * 1024u * 1024u))
    {
        return NO;
    }
    if (!rcl_image_text_contains(base, base + RCL_IMAGE_TEXT_WINDOW))
    {
        return NO;
    }
    return YES;
}

BOOL find_game_image(uintptr_t *out_base)
{
    if (!out_base)
    {
        return NO;
    }
    uint32_t count = _dyld_image_count();
    if (count > 8192)
    {
        count = 8192;
    }
    uintptr_t fallback = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        const char *path = _dyld_get_image_name(i);
        uintptr_t base = (uintptr_t)_dyld_get_image_header(i);
        if (!path || !base)
        {
            continue;
        }
        if (!strstr(path, ".app/"))
        {
            continue;
        }
        if (strstr(path, "/System/"))
        {
            continue;
        }
        if (strstr(path, "/usr/lib/"))
        {
            continue;
        }
        if (strstr(path, ".framework/"))
        {
            continue;
        }
        if (strstr(path, ".dylib"))
        {
            continue;
        }
        if (!rcl_valid_header(base))
        {
            continue;
        }
        BOOL matched = NO;
        for (int n = 0; rcl_image_names[n]; n++)
        {
            if (strstr(path, rcl_image_names[n]))
            {
                matched = YES;
                break;
            }
        }
        if (matched)
        {
            *out_base = base;
            return YES;
        }
        if (!fallback)
        {
            fallback = base;
        }
    }
    if (fallback)
    {
        *out_base = fallback;
        return YES;
    }
    return NO;
}

uintptr_t rcl_owner = 0;

int rcl_wired = 0;

dispatch_source_t rcl_scan_timer = nullptr;

BOOL rcl_segment_range(const char *name, uintptr_t *lo, uintptr_t *hi)
{
    if (!rcl_base || !name)
    {
        return NO;
    }
    if (!rcl_addr_readable(rcl_base, sizeof(struct mach_header_64)))
    {
        return NO;
    }
    const struct mach_header_64 *header = (const struct mach_header_64 *)rcl_base;
    if (header->magic != MH_MAGIC_64)
    {
        return NO;
    }
    const uint8_t *cursor = (const uint8_t *)(header + 1);
    const uint8_t *limit = cursor + header->sizeofcmds;
    uintptr_t slide = rcl_image_slide(rcl_base);
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (cursor + sizeof(struct load_command) > limit)
        {
            return NO;
        }
        const struct load_command *command = (const struct load_command *)cursor;
        if (command->cmdsize < sizeof(struct load_command))
        {
            return NO;
        }
        if (cursor + command->cmdsize > limit)
        {
            return NO;
        }
        if (command->cmd == LC_SEGMENT_64 && command->cmdsize >= sizeof(struct segment_command_64))
        {
            const struct segment_command_64 *segment = (const struct segment_command_64 *)command;
            if (strcmp(segment->segname, name) == 0)
            {
                if (lo)
                {
                    *lo = slide + (uintptr_t)segment->vmaddr;
                }
                if (hi)
                {
                    *hi = slide + (uintptr_t)segment->vmaddr + (uintptr_t)segment->vmsize;
                }
                return YES;
            }
        }
        cursor += command->cmdsize;
    }
    return NO;
}
void rcl_image_span_refresh(void)
{
    if (!rcl_base || !rcl_addr_readable(rcl_base, sizeof(struct mach_header_64)))
    {
        return;
    }
    const struct mach_header_64 *header = (const struct mach_header_64 *)rcl_base;
    if (header->magic != MH_MAGIC_64)
    {
        return;
    }
    const uint8_t *cursor = (const uint8_t *)(header + 1);
    const uint8_t *limit = cursor + header->sizeofcmds;
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (cursor + sizeof(struct load_command) > limit)
        {
            break;
        }
        const struct load_command *command = (const struct load_command *)cursor;
        if (command->cmdsize < sizeof(struct load_command))
        {
            break;
        }
        if (cursor + command->cmdsize > limit)
        {
            break;
        }
        cursor += command->cmdsize;
    }
}
const char *rcl_image_segment_name(uintptr_t value)
{
    if (!rcl_base || !value)
    {
        return nullptr;
    }
    if (!rcl_addr_readable(rcl_base, sizeof(struct mach_header_64)))
    {
        return nullptr;
    }
    const struct mach_header_64 *header = (const struct mach_header_64 *)rcl_base;
    if (header->magic != MH_MAGIC_64)
    {
        return nullptr;
    }
    const uint8_t *cursor = (const uint8_t *)(header + 1);
    const uint8_t *limit = cursor + header->sizeofcmds;
    uintptr_t slide = rcl_image_slide(rcl_base);
    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (cursor + sizeof(struct load_command) > limit)
        {
            return nullptr;
        }
        const struct load_command *command = (const struct load_command *)cursor;
        if (command->cmdsize < sizeof(struct load_command))
        {
            return nullptr;
        }
        if (cursor + command->cmdsize > limit)
        {
            return nullptr;
        }
        if (command->cmd == LC_SEGMENT_64 && command->cmdsize >= sizeof(struct segment_command_64))
        {
            const struct segment_command_64 *segment = (const struct segment_command_64 *)command;
            if (segment->vmsize)
            {
                uintptr_t start = slide + (uintptr_t)segment->vmaddr;
                if (value >= start && value < (start + (uintptr_t)segment->vmsize))
                {
                    return segment->segname;
                }
            }
        }
        cursor += command->cmdsize;
    }
    return nullptr;
}

BOOL rcl_vtable_shaped(uintptr_t value)
{
    const char *segment = rcl_image_segment_name(value);
    if (!segment)
    {
        return NO;
    }
    if (value % 8)
    {
        return NO;
    }
    if (strcmp(segment, "__DATA_CONST") == 0)
    {
        return YES;
    }
    if (strcmp(segment, "__DATA") == 0)
    {
        return YES;
    }
    return NO;
}

BOOL rcl_heap_resident(uintptr_t value)
{
    if (!value)
    {
        return NO;
    }
    return rcl_image_segment_name(value) ? NO : YES;
}
BOOL rcl_instance_shaped(uintptr_t object)
{
    void *vtable = nullptr;
    if (!rcl_pointer_plausible(object))
    {
        return NO;
    }
    if (!rcl_heap_resident(object))
    {
        return NO;
    }
    if (!rcl_read_ptr(object, &vtable))
    {
        return NO;
    }
    if (!vtable)
    {
        return NO;
    }
    if ((uintptr_t)vtable == object)
    {
        return NO;
    }
    if (!rcl_vtable_shaped((uintptr_t)vtable))
    {
        return NO;
    }
    return YES;
}

BOOL rcl_manager_shape(uintptr_t manager)
{
    void *array = nullptr;
    void *probe = nullptr;
    int32_t count = 0;
    int32_t capacity = 0;
    if (!rcl_heap_resident(manager))
    {
        return NO;
    }
    if (!rcl_read_ptr(manager + RCL_MGR_ARRAY_OFF, &array))
    {
        return NO;
    }
    if (!rcl_read_int(manager + RCL_MGR_COUNT_OFF, &count))
    {
        return NO;
    }
    if (!rcl_read_int(manager + RCL_MGR_CAP_OFF, &capacity))
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
    if (array && !rcl_heap_resident((uintptr_t)array))
    {
        return NO;
    }
    if (count > 0)
    {
        if (!array)
        {
            return NO;
        }
        if (!rcl_read_ptr((uintptr_t)array, &probe))
        {
            return NO;
        }
        if (!rcl_heap_resident((uintptr_t)probe))
        {
            return NO;
        }
        if (count > 1)
        {
            if (!rcl_read_ptr((uintptr_t)array + (uintptr_t)(count - 1) * sizeof(void *), &probe))
            {
                return NO;
            }
            if (!rcl_heap_resident((uintptr_t)probe))
            {
                return NO;
            }
        }
    }
    return YES;
}
int rcl_trail_count = 0;

BOOL rcl_vtable_in_image(uintptr_t vtable)
{
    uintptr_t lo = 0;
    uintptr_t hi = 0;
    if (!vtable)
    {
        return NO;
    }
    if (vtable >= rcl_base + RCL_DC_RVA_LO && vtable < rcl_base + RCL_DC_RVA_LO + RCL_DC_RVA_SIZE)
    {
        return YES;
    }
    if (rcl_segment_range("__DATA", &lo, &hi) && vtable >= lo && vtable < hi)
    {
        return YES;
    }
    return NO;
}

uintptr_t rcl_strip_ptr(uintptr_t value)
{
    uintptr_t stripped = value;
#if __has_feature(ptrauth_calls)
    stripped = (uintptr_t)ptrauth_strip((void *)value, ptrauth_key_function_pointer);
#endif
    if (stripped >= rcl_base && stripped < rcl_base + RCL_IMAGE_SPAN)
    {
        return stripped;
    }
    if ((stripped & 0xffffffffULL) < RCL_IMAGE_SPAN)
    {
        uintptr_t viaLow = rcl_base + (stripped & 0xffffffffULL);
        if (viaLow >= rcl_base && viaLow < rcl_base + RCL_IMAGE_SPAN)
        {
            return viaLow;
        }
    }
    return stripped;
}

void poll_for_game(int tick)
{
    if (rcl_setup_done)
    {
        return;
    }
    if (tick > 1200)
    {
        return;
    }
    uintptr_t base = 0;
    if (find_game_image(&base))
    {
        rcl_base = base;
        setup();
        return;
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     poll_for_game(tick + 1);
                   });
}

uintptr_t rcl_base = 0;


uintptr_t rcl_manager_ptr = 0;

uint64_t rcl_ticks_a = 0;

float rcl_own_speed = 720.0f;
uint32_t rcl_own_hp = 0;

int rcl_collect(uintptr_t manager, rcl_obj_t *out, int capacity)
{
    void *data = nullptr;
    int32_t count = 0;
    int usable = 0;
    uintptr_t gidOff = RCL_OBJ_GLOBALID_OFF;
    uint32_t walkSeq = 0;
    rcl_gidless_scan(manager);
    if (!manager)
    {
        return 0;
    }
    if (!rcl_read_ptr(manager + RCL_MGR_ARRAY_OFF, &data) || !data)
    {
        return 0;
    }
    if (!rcl_read_int((uintptr_t)manager + RCL_MGR_COUNT_OFF, &count))
    {
        return 0;
    }
    if (count <= 0)
    {
        return 0;
    }
    if (count > capacity)
    {
        count = capacity;
    }
    gidOff = rcl_list_gid_off((uintptr_t)data, count);
    walkSeq = rcl_seq;
    for (int32_t i = 0; i < count && usable < capacity; i++)
    {
        void *element = nullptr;
        void *vtable = nullptr;
        rcl_obj_t entry;
        uintptr_t vtRva = 0;
        memset(&entry, 0, sizeof(entry));
        if (rcl_seq != walkSeq)
        {
            break;
        }
        if (!rcl_read_ptr((uintptr_t)data + (uintptr_t)i * sizeof(void *), &element))
        {
            continue;
        }
        if (!element)
        {
            continue;
        }
        entry.object = (uintptr_t)element;
        if (rcl_element_ascii(entry.object))
        {
            continue;
        }
        if (!rcl_read_ptr(entry.object, &vtable) || !vtable)
        {
            continue;
        }
        vtRva = (uintptr_t)vtable - rcl_base;
        if (vtRva < RCL_DC_RVA_LO || vtRva >= RCL_DC_RVA_LO + RCL_DC_RVA_SIZE)
        {
            continue;
        }
        entry.gid = rcl_gid_at(entry.object, gidOff);
        {
            uintptr_t tw = 0;
            rcl_element_type((uintptr_t)vtable, &tw);
            entry.typeWord = (int32_t)tw;
        }
        if (!rcl_read_int(entry.object + rcl_coord_x_off(), &entry.x) ||
            !rcl_read_int(entry.object + rcl_coord_y_off(), &entry.y) ||
            !rcl_read_int(entry.object + RCL_OBJ_OWNERINDEX_OFF, &entry.ownerIndex) ||
            !rcl_read_int(entry.object + RCL_OBJ_TEAM_OFF, &entry.teamOld) ||
            !rcl_read_int(entry.object + RCL_TEAM_OFF, &entry.teamNew) ||
            !rcl_read_byte(entry.object + RCL_OBJ_DEADFLAG_OFF, &entry.dead) ||
            !rcl_read_byte(entry.object + RCL_OBJ_ACTIVEFLAG_OFF, &entry.activeFlag))
        {
            continue;
        }
        if (entry.gid >= 2000000)
        {
            rcl_proj_track(entry.object, vtRva, entry.gid, entry.teamOld);
            continue;
        }
        if (entry.gid == 0 && !rcl_gidless)
        {
            continue;
        }
        if (entry.x <= -1000000 || entry.x >= 1000000 || entry.y <= -1000000 || entry.y >= 1000000)
        {
            if (!RCL_COORD_SOFT)
            {
                continue;
            }
        }
        if ((entry.teamOld < 0 || entry.teamOld > 7) && (entry.teamNew < 0 || entry.teamNew > 7))
        {
            continue;
        }
        out[usable++] = entry;
    }
    return usable;
}






uint64_t rcl_us(void)
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













BOOL rcl_start_word(uint32_t word)
{
    if (word == 0xD503233F || word == 0xD503237F)
    {
        return YES;
    }
    if ((word & 0xFFFFFF1Fu) == 0xD503241Fu)
    {
        return YES;
    }
    if ((word & 0xFF800000u) == 0xA9800000u && ((word >> 5) & 31u) == 31u)
    {
        return YES;
    }
    if ((word & 0xFF8003FFu) == 0xD10003FFu)
    {
        return YES;
    }
    return NO;
}

BOOL rcl_start_boundary(const uint8_t *bytes, size_t offset)
{
    if (offset < 4)
    {
        return NO;
    }
    for (size_t back = 4, seen = 0; back <= offset && seen < 8; back += 4, seen++)
    {
        uint32_t word = 0;
        memcpy(&word, bytes + offset - back, 4);
        if (word == 0xD65F03C0)
        {
            return YES;
        }
        if (word == 0xD503201F)
        {
            continue;
        }
        if ((word & 0xFFFFFF1Fu) == 0xD503241Fu)
        {
            continue;
        }
        return NO;
    }
    return NO;
}

void rcl_load_function_starts(void)
{
    if (rcl_starts || !rcl_base)
    {
        return;
    }
    uintptr_t textAddress = 0;
    uint64_t textSize = 0;
    if (!rcl_text_section(&textAddress, &textSize))
    {
        return;
    }
    if (textSize < 64 || textSize > (64ull * 1024ull * 1024ull))
    {
        return;
    }
    uint8_t *bytes = (uint8_t *)malloc((size_t)textSize);
    if (!bytes)
    {
        return;
    }
    if (!rcl_copy(textAddress, bytes, (size_t)textSize))
    {
        free(bytes);
        return;
    }
    size_t capacity = 32768;
    uintptr_t *starts = (uintptr_t *)malloc(capacity * sizeof(uintptr_t));
    if (!starts)
    {
        free(bytes);
        return;
    }
    size_t count = 0;
    for (size_t offset = 4; offset + 4 <= (size_t)textSize; offset += 4)
    {
        uint32_t word = 0;
        memcpy(&word, bytes + offset, 4);
        if (!rcl_start_word(word))
        {
            continue;
        }
        if (!rcl_start_boundary(bytes, offset))
        {
            continue;
        }
        if (count >= capacity)
        {
            size_t grown = capacity * 2;
            uintptr_t *larger = (uintptr_t *)realloc(starts, grown * sizeof(uintptr_t));
            if (!larger)
            {
                break;
            }
            starts = larger;
            capacity = grown;
        }
        starts[count++] = textAddress + offset;
    }
    free(bytes);
    if (!count)
    {
        free(starts);
        return;
    }
    rcl_starts = starts;
    rcl_starts_count = count;
}








uintptr_t *rcl_starts = nullptr;

size_t rcl_starts_count = 0;

uintptr_t rcl_setpred = 0;





uintptr_t rcl_site = 0;

int rcl_site_state = -1;


volatile uint32_t rcl_seq = 0;

uintptr_t rcl_pub_object = 0;

uintptr_t rcl_pub_array = 0;

int32_t rcl_pub_count = 0;

uintptr_t rcl_tick_object = 0;

uintptr_t rcl_tick_array = 0;

int32_t rcl_tick_count = 0;

void rcl_publish(uintptr_t object, uintptr_t array, int32_t count, int32_t, const char *)
{
    __sync_synchronize();
    rcl_seq++;
    __sync_synchronize();
    rcl_pub_object = object;
    rcl_pub_array = array;
    rcl_pub_count = count;
    rcl_players_object = object;
    rcl_players_array = array;
    rcl_players_count = count;
    __sync_synchronize();
    rcl_seq++;
    __sync_synchronize();
}

int rcl_snapshot(uintptr_t *objectOut, uintptr_t *arrayOut, int32_t *countOut)
{
    int tries;
    for (tries = 0; tries < 8; tries++)
    {
        uint32_t s1 = rcl_seq;
        uintptr_t o;
        uintptr_t a;
        int32_t c;
        if (s1 & 1u)
        {
            continue;
        }
        o = rcl_pub_object;
        a = rcl_pub_array;
        c = rcl_pub_count;
        __sync_synchronize();
        if (rcl_seq != s1)
        {
            continue;
        }
        if (objectOut)
        {
            *objectOut = o;
        }
        if (arrayOut)
        {
            *arrayOut = a;
        }
        if (countOut)
        {
            *countOut = c;
        }
        return 1;
    }
    return 0;
}

void rcl_tick_begin(void)
{
    uintptr_t o = 0;
    uintptr_t a = 0;
    int32_t c = 0;
    if (!rcl_snapshot(&o, &a, &c))
    {
        return;
    }
    rcl_tick_object = o;
    rcl_tick_array = a;
    rcl_tick_count = c;
}


static const uint32_t rcl_slot_site_hash[34] = {
    0x7744a9f0u, 0xd8ef9922u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x3888a7e0u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0xe5674c94u,
    0x00000000u, 0x6e078ab4u, 0x6e078ab4u, 0x00000000u,
    0x4d5cf2adu, 0x04be5d31u, 0xa7ee96fau, 0xcfef7319u,
    0x140cf28cu, 0x00000000u, 0x00000000u, 0x75481000u,
    0x1a66e6cfu, 0x00000000u, 0x00000000u, 0x00000000u,
    0xdba51345u, 0x1f08716fu, 0xd537a3b5u, 0x41120014u,
    0x00000000u, 0x1d5fea33u,
};

static uint32_t rcl_site_hash(uintptr_t address)
{
    uint8_t bytes[16];
    uint32_t hash = 0x811c9dc5u;
    int i = 0;

    if (!rcl_read_bytes(address, bytes, sizeof(bytes)))
    {
        return 0;
    }

    for (i = 0; i < (int)sizeof(bytes); i++)
    {
        hash ^= (uint32_t)bytes[i];
        hash *= 0x01000193u;
    }

    return hash;
}

void rcl_slot_hooks_install(void)
{
    rcl_hook_t specs[34];
    int i = 0;

    if (!rcl_base)
    {
        return;
    }

    memcpy(specs, rcl_slot_specs, sizeof(specs));

    for (i = 0; i < 34; i++)
    {
        if (!specs[i].rva || !rcl_slot_site_hash[i])
        {
            continue;
        }

        if (rcl_site_hash(rcl_base + specs[i].rva) != rcl_slot_site_hash[i])
        {
            specs[i].rva = 0;
        }
    }

    rcl_hooks_install(rcl_base, specs, 34, (void **)rcl_slot_orig);
}

const int rcl_object_slots[3] = {2, 3, 4};


int rcl_pub_logs = 0;

void rcl_slot_pump(void)
{
    int first = -1;
    for (int i = 0; i < 34; i++)
    {
        if (!rcl_slot_object[i])
        {
            continue;
        }
        if (!rcl_slot_specs[i].control && first < 0)
        {
            first = i;
        }
    }
    if (first < 0)
    {
        return;
    }
    uintptr_t object = rcl_slot_object[first];
    if (rcl_slot_adopted == object)
    {
        return;
    }
    rcl_slot_adopted = object;
    {
        void *vtable = nullptr;
        if (!(rcl_read_ptr(object, &vtable) && vtable && rcl_vtable_in_image((uintptr_t)vtable)) &&
            !rcl_pointer_plausible((uintptr_t)vtable))
        {
            return;
        }
    }
}

rcl_slot_fn_t rcl_slot_orig[34] = {nullptr};

uint64_t rcl_slot_repl_0(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[0])
    {
        return rcl_slot_orig[0](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_1(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[1])
    {
        return rcl_slot_orig[1](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_2(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[2])
    {
        return rcl_slot_orig[2](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_3(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[3])
    {
        return rcl_slot_orig[3](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_4(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[4])
    {
        return rcl_slot_orig[4](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_5(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[5])
    {
        return rcl_slot_orig[5](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_6(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[6])
    {
        return rcl_slot_orig[6](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_7(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[7])
    {
        return rcl_slot_orig[7](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_8(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[8])
    {
        return rcl_slot_orig[8](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_9(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[9])
    {
        return rcl_slot_orig[9](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_10(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[10])
    {
        return rcl_slot_orig[10](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_11(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[11])
    {
        return rcl_slot_orig[11](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_12(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[12])
    {
        return rcl_slot_orig[12](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_13(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[13])
    {
        return rcl_slot_orig[13](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_14(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[14])
    {
        return rcl_slot_orig[14](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_15(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[15])
    {
        return rcl_slot_orig[15](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_16(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[16])
    {
        return rcl_slot_orig[16](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_17(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[17])
    {
        return rcl_slot_orig[17](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_18(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[18])
    {
        return rcl_slot_orig[18](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_19(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[19])
    {
        return rcl_slot_orig[19](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_20(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[20])
    {
        return rcl_slot_orig[20](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_21(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[21])
    {
        return rcl_slot_orig[21](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_22(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[22])
    {
        return rcl_slot_orig[22](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_23(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[23])
    {
        return rcl_slot_orig[23](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_24(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[24])
    {
        return rcl_slot_orig[24](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_25(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[25])
    {
        return rcl_slot_orig[25](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_26(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[26])
    {
        return rcl_slot_orig[26](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_27(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[27])
    {
        return rcl_slot_orig[27](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_28(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[28])
    {
        return rcl_slot_orig[28](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_29(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[29])
    {
        return rcl_slot_orig[29](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_30(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[30])
    {
        return rcl_slot_orig[30](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_31(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    if (rcl_slot_orig[31])
    {
        return rcl_slot_orig[31](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return 0;
}

uint64_t rcl_slot_repl_32(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    uint64_t r = 0;
    if (rcl_slot_orig[32])
    {
        r = rcl_slot_orig[32](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return r;
}

uint64_t rcl_slot_repl_33(void *a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6, uint64_t a7)
{
    uint64_t r = 0;
    if (rcl_slot_orig[33])
    {
        r = rcl_slot_orig[33](a0, a1, a2, a3, a4, a5, a6, a7);
    }
    return r;
}

const rcl_hook_t rcl_slot_specs[34] = {
    {RCL_SLOT_RVA_0, (void *)rcl_slot_repl_0, 0},
    {RCL_SLOT_RVA_1, (void *)rcl_slot_repl_1, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_2, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_3, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_4, 0},
    {RCL_SLOT_RVA_5, (void *)rcl_slot_repl_5, 1},
    {0, (void *)rcl_slot_repl_6, 1},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_7, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_8, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_9, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_10, 0},
    {RCL_SLOT_RVA_11, (void *)rcl_slot_repl_11, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_12, 1},
    {RCL_SLOT_RVA_13, (void *)rcl_slot_repl_13, 0},
    {RCL_SLOT_RVA_14, (void *)rcl_slot_repl_14, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_15, 0},
    {RCL_SLOT_RVA_16, (void *)rcl_slot_repl_16, 0},
    {RCL_SLOT_RVA_17, (void *)rcl_slot_repl_17, 1},
    {RCL_SLOT_RVA_18, (void *)rcl_slot_repl_18, 1},
    {RCL_SLOT_RVA_19, (void *)rcl_slot_repl_19, 0},
    {RCL_SLOT_RVA_20, (void *)rcl_slot_repl_20, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_21, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_22, 0},
    {RCL_SLOT_RVA_23, (void *)rcl_slot_repl_23, 0},
    {RCL_SLOT_RVA_24, (void *)rcl_slot_repl_24, 0},
    {0, (void *)rcl_slot_repl_25, 0},
    {0, (void *)rcl_slot_repl_26, 0},
    {RCL_SLOT_NONE, (void *)rcl_slot_repl_27, 0},
    {RCL_SLOT_RVA_28, (void *)rcl_slot_repl_28, 0},
    {RCL_SLOT_RVA_29, (void *)rcl_slot_repl_29, 0},
    {RCL_SLOT_RVA_30, (void *)rcl_slot_repl_30, 0},
    {RCL_SLOT_RVA_31, (void *)rcl_slot_repl_31, 0},
    {RVA_LOGICBATTLEMODECLIENT_UPDATE, (void *)rcl_slot_repl_32, 0},
    {RVA_BATTLESCREEN__UPDATEMOVEMENT, (void *)rcl_slot_repl_33, 0},
};





int rcl_state_tick(void)
{
    uintptr_t slot = (uintptr_t)rcl_read_global_ptr(RCL_STATE_RVA);
    int32_t state = -1;
    void *value = nullptr;
    uintptr_t scene = 0;
    uintptr_t client = 0;
    uintptr_t inner = 0;
    uintptr_t players = 0;
    void *array = nullptr;
    int32_t count = 0;
    int32_t capacity = 0;
    uintptr_t hopArray[2] = {0};
    int32_t hopCount[2] = {0};
    int32_t hopCap[2] = {0};
    int hopOk[2] = {0};
    int score[2];
    int chosen = -1;
    if (slot)
    {
        rcl_read_int(slot + RCL_STATE_ENUM_OFF, &state);
    }
    if (slot != rcl_site || state != rcl_site_state)
    {
        rcl_state_note(state);
        rcl_site = slot;
        rcl_site_state = state;
    }
    if (!slot)
    {
        rcl_scene_object = 0;
        return 0;
    }
    if (state != 5)
    {
        rcl_scene_object = 0;
        return 0;
    }
    if (!rcl_read_ptr(slot + RCL_SCENE_OFF, &value) || !value)
    {
        rcl_scene_object = 0;
        return 0;
    }
    scene = (uintptr_t)value;
    if (scene != rcl_scene_object)
    {
        rcl_scene_object = scene;
    }
    if (!rcl_read_ptr(scene + RCL_MODE_MANAGER_OFF, &value) || !value)
    {
        return 1;
    }
    client = (uintptr_t)value;
    if (!rcl_read_ptr(client + RCL_CLIENT_HOP_OFF, &value) || !value)
    {
        return 1;
    }
    inner = (uintptr_t)value;
    hopOk[0] = rcl_container_header(client, &hopArray[0], &hopCount[0], &hopCap[0]);
    hopOk[1] = rcl_container_header(inner, &hopArray[1], &hopCount[1], &hopCap[1]);
    score[0] = hopOk[0] ? rcl_container_score(client) : -1;
    score[1] = hopOk[1] ? rcl_container_score(inner) : -1;
    if (score[1] > score[0])
    {
        chosen = 1;
        rcl_hop_sticky = 1;
    }
    else if (score[0] > score[1])
    {
        chosen = 0;
        rcl_hop_sticky = 0;
    }
    else
    {
        chosen = hopOk[1] ? 1 : (hopOk[0] && !rcl_hop_sticky ? 0 : -1);
    }
    if (chosen != rcl_last_choice)
    {
        rcl_last_choice = chosen;
    }
    else if (rcl_hop_logs < 6)
    {
        rcl_hop_logs++;
    }
    if (scene != rcl_hop_scene)
    {
        rcl_hop_scene = scene;
    }
    if (RCL_WIRE_OWNER && rcl_owner)
    {
        void *directArray = nullptr;
        int32_t directCount = 0;
        if (rcl_read_ptr(rcl_owner + RCL_MGR_ARRAY_OFF, &directArray) && directArray &&
            rcl_read_int(rcl_owner + RCL_MGR_COUNT_OFF, &directCount) && directCount > 0)
        {
            rcl_players_object = rcl_owner;
            rcl_players_array = (uintptr_t)directArray;
            rcl_players_count = directCount;
            rcl_hop_chosen = RCL_HOPCHOSEN_DIRECT;
            if (!rcl_wired)
            {
                rcl_wired = 1;
            }
            return 0;
        }
    }
    if (chosen < 0)
    {
        return 1;
    }
    players = (chosen == 1) ? inner : client;
    array = (void *)hopArray[chosen];
    count = hopCount[chosen];
    capacity = hopCap[chosen];
    rcl_hop_chosen = chosen;
    {
        static int v141_array_vote_logs = 0;
        if ((uintptr_t)array != rcl_pub_array && players == rcl_pub_object &&
            v141_array_vote_logs < RCL_ARRAY_VOTE_LOGS)
        {
            v141_array_vote_logs++;
        }
    }
    if (players != rcl_pub_object || (uintptr_t)array != rcl_pub_array || count != rcl_pub_count)
    {
        rcl_publish(players, (uintptr_t)array, count, capacity, "hop-adopt");
    }
    return 1;
}



void rcl_start_timer(void)
{
    if (rcl_scan_timer)
    {
        return;
    }
    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    if (!timer)
    {
        return;
    }
    uint64_t interval = (uint64_t)(1.0 * NSEC_PER_SEC);
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, (int64_t)interval), interval,
                              (uint64_t)(0.25 * NSEC_PER_SEC));
    dispatch_source_set_event_handler(timer, ^{
      rcl_slot_pump();
      rcl_state_tick();
      rcl_ticks_b++;
    });
    dispatch_resume(timer);
    rcl_scan_timer = timer;
}

uint64_t rcl_ticks_b = 0;






typedef uintptr_t (*rcl_fn1)(uintptr_t);
typedef int32_t (*rcl_fn1i)(uintptr_t);
typedef float (*rcl_fn1f)(uintptr_t);

static uintptr_t rcl_call1(uintptr_t fn, uintptr_t self)
{
    if (!fn || !self) return 0;

    return ((rcl_fn1)fn)(self);
}



static int32_t rcl_call1i(uintptr_t fn, uintptr_t self)
{
    if (!fn || !self) return 0;

    return ((rcl_fn1i)fn)(self);
}

static float rcl_call1f(uintptr_t fn, uintptr_t self)
{
    if (!fn || !self) return 0.0f;

    return ((rcl_fn1f)fn)(self);
}
















uintptr_t rcl_manager_array_ptr = 0;
int32_t rcl_manager_count_v = 0;
int32_t rcl_own_x = 0;
int32_t rcl_own_y = 0;
int32_t rcl_own_index_v = -1;
int rcl_own_ok_hp = 0;
rcl_dbg_t rcl_dbg;

void rcl_scan_world(void)
{
    uintptr_t client = 0;
    uintptr_t own = 0;
    uintptr_t ownData = 0;
    uintptr_t manager = 0;
    uintptr_t array = 0;
    void *value = nullptr;
    int32_t count = 0;
    int32_t ownIndex = -1;
    int32_t ownTeam = -1;
    int32_t x = 0;
    int32_t y = 0;
    int32_t hp = 0;

    rcl_dbg.stage = 0;

    if (!rcl_base) return;

    client = rcl_controller();
    if (!client)
    {
        rcl_own_elem = 0;
        rcl_own_elem_scan = 0;
        rcl_own_x = 0;
        rcl_own_y = 0;
        rcl_dead = 1;
        rcl_dbg.stage = 1;
        return;
    }

    own = rcl_addr_getownchar ? rcl_call1(rcl_addr_getownchar, client) : 0;
    if (!rcl_pointer_plausible(own))
    {
        own = 0;
        rcl_dbg.stage = 2;
    }

    if (rcl_addr_getteam)
    {
        ownTeam = rcl_call1i(rcl_addr_getteam, client);
    }
    if (rcl_addr_getownidx)
    {
        ownIndex = rcl_call1i(rcl_addr_getownidx, client);
    }

    if (own)
    {
        rcl_own_elem = own;
        rcl_own_elem_scan = own;
    }
    if (ownTeam >= 0 && ownTeam <= 15)
    {
        rcl_own_team_b = ownTeam;
        rcl_own_team_a = ownTeam;
        rcl_own_team_seen = 1;
    }
    if (ownIndex >= 0 && ownIndex < 64)
    {
        rcl_own_index_v = ownIndex;
    }

    /* own position + hp straight off the character object */
    if (own)
    {
        if (rcl_read_int(own + (uintptr_t)RCL_OBJ_X_OFF, &x) &&
            rcl_read_int(own + (uintptr_t)RCL_OBJ_Y_OFF, &y))
        {
            rcl_own_x = x;
            rcl_own_y = y;
        }

        if (rcl_read_int(own + (uintptr_t)RCL_OBJ_HP_OFF, &hp))
        {
            rcl_own_ok_hp = 1;
            rcl_own_hp = (uint32_t)(hp > 0 ? hp : 0);
            rcl_dead = (hp > 0) ? 0 : 1;
        }
        else
        {
            rcl_own_ok_hp = 0;
            rcl_dead = 0;
        }
    }
    else
    {
        rcl_dead = 1;
    }

    if (own)
    {
        int32_t probe = 0;

        ownData = rcl_addr_getdata ? rcl_call1(rcl_addr_getdata, own) : 0;
        if (rcl_pointer_plausible(ownData) && rcl_read_int(ownData, &probe))
        {
            float radius = rcl_addr_charradius ? rcl_call1f(rcl_addr_charradius, ownData) : 0.0f;
            float speed = rcl_addr_charspeed ? rcl_call1f(rcl_addr_charspeed, ownData) : 0.0f;

            if (radius > 1.0f && radius < 4000.0f) rcl_own_r = radius;
            if (speed > 1.0f && speed < 4000.0f) rcl_own_speed = speed;
        }
    }

    if (!rcl_read_ptr(client + (uintptr_t)RCL_MODE_MANAGER_OFF, &value) || !value)
    {
        rcl_dbg.stage = 3;
        return;
    }
    manager = (uintptr_t)value;
    rcl_manager_ptr = manager;

    if (!rcl_read_ptr(manager + (uintptr_t)RCL_MGR_ARRAY_OFF, &value) || !value)
    {
        rcl_dbg.stage = 4;
        return;
    }
    array = (uintptr_t)value;

    if (!rcl_read_int(manager + (uintptr_t)RCL_MGR_COUNT_OFF, &count) || count <= 0)
    {
        rcl_dbg.stage = 5;
        return;
    }
    if (count > RCL_COUNT_MAX) count = RCL_COUNT_MAX;

    rcl_manager_array_ptr = array;
    rcl_manager_count_v = count;
    rcl_dbg.stage = 6;

    rcl_ticks_a++;
}

void rcl_run_workload(void)
{
    rcl_tick_begin();
    rcl_wall_maybe_refresh((uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0));
    rcl_locate_battle_mode();
    rcl_scan_world();
    rcl_run_autododge();
}

void rcl_run_autododge(void)
{
    if (rcl_in_move)
    {
        return;
    }
    rcl_in_move = 1;
    rcl_autododge();
    rcl_in_move = 0;
}

static uint64_t rcl_work_us = 0;

static dispatch_source_t rcl_hot_timer = nullptr;

static void rcl_objc_body(void)
{
    uint64_t now = 0;
    if (rcl_inside_hook)
    {
        return;
    }
    now = rcl_us();
    if (now && rcl_work_us && (now - rcl_work_us) < RCL_WORK_GAP_US)
    {
        return;
    }
    rcl_work_us = now;
    rcl_inside_hook = YES;
    rcl_run_workload();
    rcl_inside_hook = NO;
}

static void rcl_start_hot_timer(void)
{
    dispatch_queue_t queue = nullptr;
    dispatch_source_t timer = nullptr;
    uint64_t interval = 0;

    if (rcl_hot_timer)
    {
        return;
    }
    queue = dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    if (!timer)
    {
        return;
    }
    interval = (uint64_t)(RCL_HOT_INTERVAL_US * NSEC_PER_USEC);
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, 0), interval, 0);
    dispatch_source_set_event_handler(timer, ^{
      rcl_objc_body();
    });
    dispatch_resume(timer);
    rcl_hot_timer = timer;
}

void setup(void)
{
    if (rcl_setup_done)
    {
        return;
    }
    rcl_setup_done = YES;
    rcl_load_function_starts();
    rcl_resolve_addresses();
    rcl_objc_arm(rcl_base, "MetalView", "render", rcl_objc_body);
    rcl_objc_arm(rcl_base, "NullView", "render", rcl_objc_body);
    rcl_slot_hooks_install();
    rcl_start_timer();
    rcl_start_hot_timer();
}

int rcl_prev_state = -1;

int rcl_hop_chosen = -1;

int rcl_hop_sticky = 0;


uintptr_t rcl_scan_container = 0;




int rcl_element_type(uintptr_t vt, uintptr_t *wordOut)
{
    void *slotPtr = nullptr;
    uintptr_t slot = 0;
    if (wordOut)
    {
        *wordOut = 0;
    }
    if (!vt)
    {
        return -1;
    }
    if (!rcl_read_ptr(vt + RCL_TYPE_SLOT_OFF, &slotPtr) || !slotPtr)
    {
        return -1;
    }
    slot = rcl_strip_ptr((uintptr_t)slotPtr) - rcl_base;
    if (wordOut)
    {
        *wordOut = slot;
    }
    switch (slot)
    {
    case RCL_TYPE_SLOT_RVA_0:
        return 0;
    case RCL_TYPE_SLOT_RVA_1:
        return 1;
    case RCL_TYPE_SLOT_RVA_2:
        return 2;
    case RCL_TYPE_SLOT_RVA_3:
        return 3;
    case RCL_TYPE_SLOT_RVA_4:
        return 4;
    case RCL_TYPE_SLOT_RVA_5:
        return 5;
    case RCL_TYPE_SLOT_RVA_6:
        return 6;
    case RCL_TYPE_SLOT_RVA_8:
        return 8;
    default:
        return -1;
    }
}

uintptr_t rcl_hop_scene = 0;

int rcl_container_header(uintptr_t object, uintptr_t *arrayOut, int32_t *countOut, int32_t *capOut)
{
    void *array = nullptr;
    if (arrayOut)
    {
        *arrayOut = 0;
    }
    if (countOut)
    {
        *countOut = 0;
    }
    if (capOut)
    {
        *capOut = 0;
    }
    if (!object)
    {
        return 0;
    }
    if (rcl_header_reason(object, countOut, capOut))
    {
        return 0;
    }
    if (!rcl_read_ptr(object + RCL_MGR_ARRAY_OFF, &array) || !array)
    {
        return 0;
    }
    if (!rcl_heap_resident((uintptr_t)array))
    {
        return 0;
    }
    if (arrayOut)
    {
        *arrayOut = (uintptr_t)array;
    }
    return 1;
}

int32_t rcl_gid_at(uintptr_t element, uintptr_t off)
{
    int32_t gid = 0;
    if (off && rcl_read_int(element + off, &gid))
    {
        return gid;
    }
    return 0;
}

int32_t rcl_gid(uintptr_t element, int32_t *offOut)
{
    int32_t gid = 0;
    int32_t alt = 0;
    if (offOut)
    {
        *offOut = 0;
    }
    if (!element)
    {
        return 0;
    }
    if (rcl_read_int(element + RCL_OBJ_GLOBALID_OFF, &gid) && gid)
    {
        if (offOut)
        {
            *offOut = (int32_t)RCL_OBJ_GLOBALID_OFF;
        }
        return gid;
    }
    if (rcl_read_int(element + RCL_GID_FALLBACK_OFF, &alt) && alt)
    {
        if (offOut)
        {
            *offOut = (int32_t)RCL_GID_FALLBACK_OFF;
        }
        return alt;
    }
    return 0;
}

int rcl_last_choice = -2;

int rcl_hop_logs = 0;

int rcl_container_score(uintptr_t container)
{
    void *array = nullptr;
    int32_t count = 0;
    int32_t own = -1;
    int32_t ownTeam = -1;
    int32_t gid = 0;
    int32_t team = 0;
    int samples = 0;
    int gidOk = 0;
    int teamOk = 0;
    int posOk = 0;
    int posDistinct = 0;
    int asciiCount = 0;
    int soft = 0;
    int32_t px = 0;
    int32_t py = 0;
    int32_t qx = 0;
    int32_t qy = 0;
    int score = 0;
    int i;
    int j;
    if (!container)
    {
        return -1;
    }
    if (!rcl_read_ptr(container + RCL_MGR_ARRAY_OFF, &array) || !array)
    {
        return -1;
    }
    if (!rcl_read_int(container + RCL_MGR_COUNT_OFF, &count))
    {
        return -1;
    }
    if (count <= 0 || count > RCL_COUNT_MAX)
    {
        return -1;
    }
    if (own < 0 || own >= count)
    {
        soft = 1;
    }
    for (i = 0; i < count && samples < 4; i++)
    {
        void *element = nullptr;
        if (!rcl_read_ptr((uintptr_t)array + (uintptr_t)i * 8ULL, &element) || !element)
        {
            continue;
        }
        samples++;
        team = 0;
        px = 0;
        py = 0;
        if (rcl_element_ascii((uintptr_t)element))
        {
            asciiCount++;
        }
        gid = rcl_gid((uintptr_t)element, nullptr);
        rcl_read_int((uintptr_t)element + RCL_TEAM_OFF, &team);
        if (rcl_read_int((uintptr_t)element + RCL_OBJ_X_OFF, &px) &&
            rcl_read_int((uintptr_t)element + RCL_OBJ_Y_OFF, &py) && px > -RCL_COORD_MAX &&
            px < RCL_COORD_MAX && py > -RCL_COORD_MAX && py < RCL_COORD_MAX && (px != 0 || py != 0))
        {
            int dup = 0;
            posOk++;
            for (j = 0; j < i; j++)
            {
                void *other = nullptr;
                if (!rcl_read_ptr((uintptr_t)array + (uintptr_t)j * 8ULL, &other) || !other)
                {
                    continue;
                }
                if (!rcl_read_int((uintptr_t)other + RCL_OBJ_X_OFF, &qx))
                {
                    continue;
                }
                if (!rcl_read_int((uintptr_t)other + RCL_OBJ_Y_OFF, &qy))
                {
                    continue;
                }
                if (qx == px && qy == py)
                {
                    dup = 1;
                    break;
                }
            }
            if (!dup)
            {
                posDistinct++;
            }
        }
        if (gid)
        {
            gidOk++;
        }
        if (team >= 0 && team <= 7)
        {
            teamOk++;
        }
    }
    if (samples > 0 && (asciiCount * 100) / samples > RCL_ASCII_RATIO)
    {
        return -1;
    }
    if (soft)
    {
        if (posOk < RCL_SOFT_MIN_POS || posDistinct < RCL_SOFT_MIN_DIST)
        {
            return -1;
        }
        score = RCL_SOFT_BASE;
    }
    else
    {
        score = 6;
        if (ownTeam >= 0 && ownTeam <= 15)
        {
            score += 2;
        }
    }
    if (gidOk)
    {
        score += 1;
    }
    if (teamOk)
    {
        score += 1;
    }
    if (count >= 3)
    {
        score += 2;
    }
    if (count >= 6)
    {
        score += 1;
    }
    score += posOk * RCL_POS_BONUS + posDistinct * RCL_DIST_BONUS;
    return score;
}

void rcl_resolve_addresses(void)
{
    if (!rcl_base)
    {
        return;
    }
    rcl_addr_getinstance = rcl_callable(RVA_BATTLEMODE_GETINSTANCE);
    rcl_addr_getownchar = rcl_callable(RVA_LOGICBATTLEMODECLIENT_GETOWNCHARACTER);
    rcl_addr_getteam = rcl_callable(RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERTEAM);
    rcl_addr_getx = rcl_callable(RVA_LOGICGAMEOBJECTCLIENT_GETX);
    rcl_addr_gety = rcl_callable(RVA_LOGICGAMEOBJECTCLIENT_GETY);
    rcl_addr_battlescreen = rcl_base;
    rcl_addr_getownidx = rcl_callable(RVA_LOGICBATTLEMODECLIENT_GETOWNPLAYERINDEX);
    rcl_addr_movedto = rcl_callable(RVA_LOGICBATTLEMODECLIENT_SETCLIENTPREDICTIONMOVETO);
    rcl_addr_getdata = rcl_callable(RVA_LOGICGAMEOBJECTCLIENT_GETDATA);
    rcl_addr_getgid = rcl_callable(RVA_LOGICGAMEOBJECTCLIENT_GETGLOBALID);
    rcl_addr_charspeed = rcl_callable(RVA_LOGICCHARACTERDATA_GETSPEED);
    rcl_addr_charradius = rcl_callable(RVA_LOGICCHARACTERDATA_GETCOLLISIONRADIUS);
    rcl_addr_projspeed = rcl_callable(RVA_LOGICPROJECTILEDATA_GETSPEED);
    rcl_addr_projradius = rcl_callable(RVA_LOGICPROJECTILEDATA_GETRADIUS);
    rcl_addr_projbeam = rcl_callable(RVA_LOGICPROJECTILEDATA_ISBEAM);
    if (!rcl_addr_readable(rcl_addr_battlescreen, sizeof(void *)))
    {
        rcl_addr_battlescreen = 0;
    }
}
void rcl_locate_battle_mode(void)
{
    if (rcl_mode_strong)
    {
        return;
    }
    if (rcl_votescan_attempts >= RCL_VOTESCAN_ATTEMPTS)
    {
        return;
    }
    double now = CFAbsoluteTimeGetCurrent();
    if (rcl_votescan_last > 0.0 && (now - rcl_votescan_last) < RCL_VOTESCAN_INTERVAL)
    {
        return;
    }
    rcl_votescan_last = now;
    rcl_votescan_attempts++;
    if (rcl_votescan_attempts == 1)
    {
        rcl_image_span_refresh();
    }
    if ((rcl_votescan_attempts % RCL_VOTESCAN_GLOBAL_EVERY) == 1)
    {
        rcl_image_span_refresh();
    }
}



uint64_t rcl_probe_last_ms = 0;

int rcl_coord_ok = 0;

int rcl_coord_usable = 0;

int rcl_team_off = (int)RCL_OBJ_TEAM_OFF;



int rcl_gidless = 0;

void rcl_gidless_scan(uintptr_t manager)
{
    void *data = nullptr;
    int32_t count = 0;
    int32_t i = 0;
    int seen = 0;
    int withGid = 0;
    rcl_gidless = 0;
    if (!RCL_GIDLESS)
    {
        return;
    }
    if (!manager)
    {
        return;
    }
    if (!rcl_read_ptr(manager + RCL_MGR_ARRAY_OFF, &data) || !data)
    {
        return;
    }
    if (!rcl_read_int(manager + RCL_MGR_COUNT_OFF, &count))
    {
        return;
    }
    for (i = 0; i < count && i < 8; i++)
    {
        void *element = nullptr;
        if (!rcl_read_ptr((uintptr_t)data + (uintptr_t)i * 8ULL, &element) || !element)
        {
            continue;
        }
        seen++;
        if (rcl_gid((uintptr_t)element, nullptr) != 0)
        {
            withGid++;
        }
    }
    if (seen > 0 && withGid == 0)
    {
        rcl_gidless = 1;
        return;
    }
}

uintptr_t rcl_list_gid_off(uintptr_t array, int32_t count)
{
    uintptr_t off = RCL_OBJ_GLOBALID_OFF;
    int sawAt8 = 0;
    int sawAt50 = 0;
    int i = 0;
    int32_t v = 0;
    if (!array || count <= 0)
    {
        return off;
    }
    for (i = 0; i < count && i < 4; i++)
    {
        void *element = nullptr;
        if (!rcl_read_ptr(array + (uintptr_t)i * sizeof(void *), &element) || !element)
        {
            continue;
        }
        if (rcl_read_int((uintptr_t)element + RCL_OBJ_GLOBALID_OFF, &v) && v != 0)
        {
            sawAt8++;
        }
        if (rcl_read_int((uintptr_t)element + RCL_GID_FALLBACK_OFF, &v) && v != 0)
        {
            sawAt50++;
        }
    }
    if (sawAt8 > 0)
    {
        off = RCL_OBJ_GLOBALID_OFF;
    }
    else if (sawAt50 > 0)
    {
        off = RCL_GID_FALLBACK_OFF;
    }
    return off;
}

int rcl_dead = 0;

int rcl_own_team_a = -1;

int rcl_own_team_seen = 0;

uintptr_t rcl_proj_addr = 0;

uint8_t rcl_proj_bytes[0x100];

int rcl_proj_have = 0;

int rcl_proj_dumps = 0;

uint64_t rcl_proj_diff_logs = 0;

void rcl_proj_track(uintptr_t elem, uintptr_t, int32_t, int32_t)
{
    uint8_t now[0x100];
    int i;
    if (!elem)
    {
        return;
    }
    if (!rcl_read_bytes(elem, now, sizeof(now)))
    {
        return;
    }
    if (elem != rcl_proj_addr)
    {
        rcl_proj_addr = elem;
        memcpy(rcl_proj_bytes, now, sizeof(now));
        rcl_proj_have = 1;
        if (rcl_proj_dumps < RCL_DUMPS)
        {
            rcl_proj_dumps++;
            for (i = 0; i < 0x100; i += 8)
            {
                uint64_t q = 0;
                int32_t lo = 0;
                int32_t hi = 0;
                float loF = 0.0f;
                float hiF = 0.0f;
                memcpy(&q, now + i, 8);
                memcpy(&lo, now + i, 4);
                memcpy(&hi, now + i + 4, 4);
                memcpy(&loF, now + i, 4);
                memcpy(&hiF, now + i + 4, 4);
            }
        }
        return;
    }
    if (!rcl_proj_have)
    {
        memcpy(rcl_proj_bytes, now, sizeof(now));
        rcl_proj_have = 1;
        return;
    }
    for (i = 0; i < 0x100; i++)
    {
        if (rcl_proj_bytes[i] == now[i])
        {
            continue;
        }
        rcl_proj_diff_logs++;
        if (rcl_proj_diff_logs <= RCL_DIFF_LOGS)
        {
            uint64_t oldQ = 0;
            uint64_t newQ = 0;
            int base = i & ~7;
            memcpy(&oldQ, rcl_proj_bytes + base, 8);
            memcpy(&newQ, now + base, 8);
        }
    }
    memcpy(rcl_proj_bytes, now, sizeof(now));
}





uintptr_t rcl_hop(uintptr_t base, int *whyOut)
{
    void *p = nullptr;
    void *q = nullptr;
    if (whyOut)
    {
        *whyOut = 0;
    }
    if (!base)
    {
        if (whyOut)
        {
            *whyOut = 1;
        }
        return 0;
    }
    if (!rcl_read_ptr(base + RCL_CTRL_MODE_OFF, &p) || !p)
    {
        if (whyOut)
        {
            *whyOut = 2;
        }
        return 0;
    }
    if (!rcl_read_ptr((uintptr_t)p + RCL_OWN_INNER_OFF, &q) || !q)
    {
        if (whyOut)
        {
            *whyOut = 3;
        }
        return 0;
    }
    return (uintptr_t)q;
}
int rcl_scene_live(void)
{
    uintptr_t slot = (uintptr_t)rcl_read_global_ptr(RCL_STATE_RVA);
    void *value = nullptr;
    int32_t state = -1;
    if (!rcl_scene_object || !slot)
    {
        return 0;
    }
    if (!rcl_read_int(slot + RCL_STATE_ENUM_OFF, &state) || state != 5)
    {
        return 0;
    }
    if (!rcl_read_ptr(slot + RCL_SCENE_OFF, &value) || !value)
    {
        return 0;
    }
    return (uintptr_t)value == rcl_scene_object;
}

uintptr_t rcl_client(void)
{
    void *client = nullptr;
    if (!rcl_scene_object || !rcl_scene_live())
    {
        return 0;
    }
    if (!rcl_read_ptr((uintptr_t)rcl_scene_object + RCL_CLIENT_OFF, &client))
    {
        return 0;
    }
    return (uintptr_t)client;
}

void rcl_state_note(int state)
{
    if (rcl_prev_state == 5 && state != 5)
    {
        rcl_own_index = -1;
        rcl_own_ptr = 0;
        rcl_own_ptr_a = 0;
    }
    if (state == 5 && rcl_prev_state != 5)
    {
        rcl_owner = 0;
        rcl_wired = 0;
    }
    rcl_prev_state = state;
}

rcl_proj_t rcl_projs[16];

rcl_proj_death_t rcl_proj_deaths[16];

int rcl_proj_death_n = 0;

static char rcl_proj_name_buf[16][64];

static int rcl_proj_read_name(uintptr_t data, char *dst, int cap)
{
    void *strp = nullptr;
    void *farp = nullptr;
    uintptr_t str = 0;
    uintptr_t src = 0;
    int32_t len = 0;
    if (!data || !dst || cap <= 1)
    {
        return 0;
    }
    if (!rcl_read_ptr(data + (uintptr_t)RCL_PROJ_DATA_NAME_OFF, &strp) || !strp)
    {
        return 0;
    }
    str = (uintptr_t)strp;
    if (!rcl_read_int(str + (uintptr_t)RCL_SC_LEN_OFF, &len))
    {
        return 0;
    }
    if (len <= 0 || len >= cap)
    {
        return 0;
    }
    if (len <= 7)
    {
        src = str + (uintptr_t)RCL_SC_DATA_OFF;
    }
    else
    {
        if (!rcl_read_ptr(str + (uintptr_t)RCL_SC_DATA_OFF, &farp) || !farp)
        {
            return 0;
        }
        src = (uintptr_t)farp;
    }
    if (!rcl_read_bytes(src, dst, (size_t)len))
    {
        return 0;
    }
    dst[len] = 0;
    return 1;
}

int rcl_proj_scan(uintptr_t manager, int32_t count, const rcl_obj_t *roster, int rosterUsable)
{
    int32_t ownerTeam[64];
    void *array = nullptr;
    int found = 0;
    int k;
    int32_t i;
    uint64_t nowMs = 0;
    if (!manager || count <= 0)
    {
        return 0;
    }
    if (count > RCL_COUNT_MAX)
    {
        count = RCL_COUNT_MAX;
    }
    if (!rcl_read_ptr(manager + RCL_MGR_ARRAY_OFF, &array) || !array)
    {
        return 0;
    }
    for (k = 0; k < 16; k++)
    {
        rcl_projs[k].classRva = (uintptr_t)-1;
    }
    rcl_proj_death_n = 0;
    nowMs = (uint64_t)(CFAbsoluteTimeGetCurrent() * 1000.0);
    {
        int rk = 0;

        for (rk = 0; rk < 64; rk++)
        {
            ownerTeam[rk] = -1;
        }
        for (rk = 0; rk < rosterUsable && rk < 64; rk++)
        {
            int32_t idx = roster[rk].ownerIndex;

            if (idx < 0 || idx >= 64)
            {
                continue;
            }
            if (roster[rk].gid < 1000000 || roster[rk].gid >= 2000000)
            {
                continue;
            }
            ownerTeam[idx] = rcl_team_at(roster, rk);
        }
    }

    for (i = 0; i < count && found < 16; i++)
    {
        void *element = nullptr;
        void *vtable = nullptr;
        uintptr_t vtRva = 0;
        int32_t gid = 0;
        int32_t px = 0;
        int32_t py = 0;
        int slot = -1;
        if (!rcl_read_ptr((uintptr_t)array + (uintptr_t)i * 8ULL, &element) || !element)
        {
            continue;
        }
        if (!rcl_read_ptr((uintptr_t)element, &vtable) || !vtable)
        {
            continue;
        }
        vtRva = (uintptr_t)vtable - rcl_base;
        gid = rcl_gid((uintptr_t)element, nullptr);
        if (gid < 2000000)
        {
            gid = 0;
        }
        {
            static uintptr_t seenCls[4] = {0, 0, 0, 0};
            static int seenClsN = 0;
            int si;
            int known = 0;
            for (si = 0; si < seenClsN; si++)
            {
                if (seenCls[si] == vtRva)
                {
                    known = 1;
                    break;
                }
            }
            if (!known && seenClsN < 4)
            {
                seenCls[seenClsN++] = vtRva;
            }
        }
        if (1 && vtRva != (uintptr_t)RCL_CLASS_PROJ_RVA)
        {
            continue;
        }
        if (!rcl_read_int((uintptr_t)element + RCL_OBJ_X_OFF, &px))
        {
            continue;
        }
        if (!rcl_read_int((uintptr_t)element + RCL_OBJ_Y_OFF, &py))
        {
            continue;
        }
        for (k = 0; k < 16; k++)
        {
            if (rcl_projs[k].elem != (uintptr_t)element)
            {
                continue;
            }
            slot = k;
            break;
        }
        if (slot < 0)
        {
            for (k = 0; k < 16; k++)
            {
                if (rcl_projs[k].classRva != (uintptr_t)-1)
                {
                    continue;
                }
                slot = k;
                break;
            }
        }
        if (slot < 0)
        {
            continue;
        }
        if (rcl_projs[slot].elem == (uintptr_t)element)
        {
            if (px != rcl_projs[slot].x || py != rcl_projs[slot].y)
            {
                rcl_projs[slot].px = rcl_projs[slot].x;
                rcl_projs[slot].py = rcl_projs[slot].y;
                rcl_projs[slot].pms = rcl_projs[slot].qms;
                rcl_projs[slot].hasPrev = 1;
            }
        }
        else
        {
            rcl_projs[slot].elem = (uintptr_t)element;
            rcl_projs[slot].px = px;
            rcl_projs[slot].py = py;
            rcl_projs[slot].spawnX = px;
            rcl_projs[slot].spawnY = py;
            rcl_projs[slot].pms = nowMs;
            rcl_projs[slot].qms = nowMs;
            rcl_projs[slot].hasPrev = 0;
            rcl_projs[slot].spawnedAt = nowMs;
            rcl_projs[slot].vx = 0.0f;
            rcl_projs[slot].vy = 0.0f;
        }
        {
            uintptr_t teamOff =
                (rcl_team_off == (int)RCL_OBJ_TEAM_OFF) ? RCL_OBJ_TEAM_OFF : RCL_TEAM_OFF;
            int32_t pteam = -1;
            int32_t ownerIdx = -1;
            int settled = 0;
            if (roster && rosterUsable > 0 &&
                rcl_read_int((uintptr_t)element + (uintptr_t)RCL_OBJ_OWNERINDEX_OFF, &ownerIdx) &&
                ownerIdx >= 0 && ownerIdx < 64)
            {
                pteam = ownerTeam[ownerIdx];
                settled = pteam >= 0;
            }
            if (!settled && rcl_read_int((uintptr_t)element + teamOff, &pteam) && pteam >= 0 && pteam <= 7)
            {
                settled = 1;
            }
            if (settled && pteam >= 0 && pteam <= 15)
            {
                rcl_projs[slot].team = pteam;
            }
            else
            {
                int side = rcl_own_side_spawn(px, py);
                if (side == 1 && rcl_own_team_b >= 0)
                {
                    rcl_projs[slot].team = rcl_own_team_b;
                }
                else
                {
                    rcl_projs[slot].team = -1;
                }
            }
        }
        {
            void *def = nullptr;
            uint8_t indirect = 0;
            rcl_projs[slot].name = nullptr;
            rcl_projs[slot].angle = 0.0f;
            rcl_projs[slot].radius = 0.0f;
            rcl_projs[slot].isThrower = 0;
            rcl_projs[slot].isBeam = 0;
            rcl_projs[slot].targetX = 0;
            rcl_projs[slot].targetY = 0;
            rcl_projs[slot].spawnAreaRadius = 0;
            rcl_projs[slot].spawnAreaActiveTime = 0;
            rcl_projs[slot].castRange = 0;
            if (rcl_read_ptr((uintptr_t)element + (uintptr_t)RCL_ELEM_DEF_OFF, &def) && def)
            {
                if (rcl_proj_read_name((uintptr_t)def, rcl_proj_name_buf[slot], 64))
                {
                    rcl_projs[slot].name = rcl_proj_name_buf[slot];
                }
                if (rcl_read_bytes((uintptr_t)def + (uintptr_t)RCL_PROJ_ISTHROWER_OFF, &indirect,
                                   sizeof(indirect)))
                {
                    rcl_projs[slot].isThrower = indirect ? 1 : 0;
                }
                {
                    uint8_t beam = 0;
                    if (rcl_read_bytes((uintptr_t)def + (uintptr_t)OFF_PROJECTILEDATA_ISBEAM, &beam, sizeof(beam)))
                    {
                        rcl_projs[slot].isBeam = beam ? 1 : 0;
                    }
                }
                {
                    int32_t rad = 0;
                    if (rcl_read_int((uintptr_t)def + (uintptr_t)OFF_PROJECTILEDATA_RADIUS, &rad) && rad > 0)
                    {
                        rcl_projs[slot].radius = (float)rad;
                    }
                }
                {
                    void *area = nullptr;
                    if (rcl_read_ptr((uintptr_t)def + (uintptr_t)OFF_PROJECTILEDATA_SPAWNAREA, &area) && area)
                    {
                        int32_t areaRadius = 0;
                        int32_t areaTime = 0;
                        if (rcl_read_int((uintptr_t)area + (uintptr_t)OFF_AREAEFFECT_RADIUS, &areaRadius) &&
                            areaRadius >= RCL_AREA_RADIUS_MIN && areaRadius <= RCL_AREA_RADIUS_MAX)
                        {
                            rcl_projs[slot].spawnAreaRadius = areaRadius;
                        }
                        if (rcl_read_int((uintptr_t)area + (uintptr_t)OFF_AREAEFFECT_TIMEMS, &areaTime) &&
                            areaTime >= RCL_AREA_TIME_MIN && areaTime <= RCL_AREA_TIME_MAX)
                        {
                            rcl_projs[slot].spawnAreaActiveTime = areaTime;
                        }
                    }
                }
            }
            {
                int32_t raw = 0;
                if (rcl_read_int((uintptr_t)element + (uintptr_t)RCL_PROJ_ANGLE_OFF, &raw) &&
                    raw > 0 && raw <= 360)
                {
                    rcl_projs[slot].angle = (float)raw;
                }
            }
        }
        if (rcl_projs[slot].hasPrev && nowMs > rcl_projs[slot].pms)
        {
            float dt = (float)(nowMs - rcl_projs[slot].pms);

            if (dt < 1.0f) dt = 1.0f;

            rcl_projs[slot].vx = (float)(px - rcl_projs[slot].px) * 1000.0f / dt;
            rcl_projs[slot].vy = (float)(py - rcl_projs[slot].py) * 1000.0f / dt;
        }

        rcl_projs[slot].classRva = vtRva;
        rcl_projs[slot].x = px;
        rcl_projs[slot].y = py;
        rcl_projs[slot].gid = gid;
        rcl_projs[slot].qms = nowMs;
        found++;
    }
    for (k = 0; k < 16; k++)
    {
        if (rcl_projs[k].classRva != (uintptr_t)-1)
        {
            continue;
        }
        if (rcl_projs[k].elem && rcl_projs[k].name && rcl_proj_death_n < 16)
        {
            rcl_proj_death_t *rec = &rcl_proj_deaths[rcl_proj_death_n];
            rec->name = rcl_projs[k].name;
            rec->angle = rcl_projs[k].angle;
            rec->spawnX = rcl_projs[k].spawnX;
            rec->spawnY = rcl_projs[k].spawnY;
            rec->x = rcl_projs[k].x;
            rec->y = rcl_projs[k].y;
            rcl_proj_death_n++;
        }
        rcl_projs[k].elem = 0;
        rcl_projs[k].hasPrev = 0;
        rcl_projs[k].name = nullptr;
    }
    return found;
}

static int rcl_ctrl_logs = 0;

static int rcl_bounds_try(uintptr_t receiver, int32_t *wOut, int32_t *hOut)
{
    uintptr_t bounds = 0;
    int32_t w = 0;
    int32_t h = 0;
    if (!receiver)
    {
        return 0;
    }
    if (!rcl_pointer_plausible(receiver))
    {
        return 0;
    }
    {
        void *box = nullptr;
        if (!rcl_read_ptr(receiver + (uintptr_t)RCL_BOX_PTR_OFF, &box))
        {
            return 0;
        }
        bounds = (uintptr_t)box;
    }
    if (!bounds || (bounds & 7))
    {
        return 0;
    }
    if (!rcl_addr_readable(bounds, 0x100))
    {
        return 0;
    }
    if (!rcl_read_int(bounds + RCL_BOUNDS_X_OFF, &w))
    {
        return 0;
    }
    if (!rcl_read_int(bounds + RCL_BOUNDS_Y_OFF, &h))
    {
        return 0;
    }
    if (w <= 3 || h <= 3 || w > 200000 || h > 200000)
    {
        return 0;
    }
    if (wOut)
    {
        *wOut = w;
    }
    if (hOut)
    {
        *hOut = h;
    }
    return 1;
}

int rcl_ctrl_bounds(uintptr_t base, int32_t *wOut, int32_t *hOut)
{
    if (!base)
    {
        return 0;
    }
    if (!rcl_pointer_plausible(base))
    {
        return 0;
    }
    return rcl_bounds_try(base, wOut, hOut);
}

uintptr_t rcl_controller(void)
{
    uintptr_t client = rcl_client();
    int32_t cw = 0;
    int32_t ch = 0;
    if (client && rcl_ctrl_bounds(client, &cw, &ch))
    {
        return client;
    }
    if (!rcl_ctrl_logs)
    {
        rcl_ctrl_logs = 1;
    }
    return client;
}





const char *rcl_header_reason(uintptr_t manager, int32_t *countOut, int32_t *capOut)
{
    void *array = nullptr;
    int32_t count = 0;
    int32_t capacity = 0;
    if (countOut)
    {
        *countOut = 0;
    }
    if (capOut)
    {
        *capOut = 0;
    }
    if (!manager)
    {
        return "no-manager";
    }
    if (manager & 7ULL)
    {
        return "manager-unaligned";
    }
    if (!rcl_read_ptr(manager + RCL_MGR_ARRAY_OFF, &array))
    {
        return "array-unreadable";
    }
    if (!array)
    {
        return "array-null";
    }
    if (!rcl_read_int(manager + RCL_MGR_COUNT_OFF, &count))
    {
        return "count-unreadable";
    }
    if (count <= 0)
    {
        return "count-zero";
    }
    if (!rcl_read_int(manager + RCL_MGR_CAP_OFF, &capacity))
    {
        return "cap-unreadable";
    }
    if (count > capacity)
    {
        return "count-above-cap";
    }
    if (capacity > 4096)
    {
        return "cap-above-ceiling";
    }
    if (count > 96)
    {
        return "count-above-ceiling";
    }
    if (countOut)
    {
        *countOut = count;
    }
    if (capOut)
    {
        *capOut = capacity;
    }
    return nullptr;
}

uintptr_t rcl_coord_x_off(void)
{
    return RCL_OBJ_X_OFF;
}

uintptr_t rcl_coord_y_off(void)
{
    return RCL_OBJ_Y_OFF;
}

rcl_trail_t rcl_trail[8];



uintptr_t rcl_scene_object = 0;

int rcl_team_at(const rcl_obj_t *objects, int index)
{
    if (!objects || index < 0)
    {
        return -1;
    }
    return (rcl_team_off == (int)RCL_OBJ_TEAM_OFF) ? objects[index].teamOld
                                                   : objects[index].teamNew;
}



int rcl_own_side_spawn(int32_t sx, int32_t sy)
{
    int i = 0;
    int best = -1;
    float bestD = 0.0f;
    float secondD = -1.0f;
    if (rcl_own_team_b < 0)
    {
        return -1;
    }
    if (rcl_pl_n <= 0)
    {
        return -1;
    }
    for (i = 0; i < rcl_pl_n; i++)
    {
        float dx = (float)sx - (float)rcl_pl_x[i];
        float dy = (float)sy - (float)rcl_pl_y[i];
        float d2 = dx * dx + dy * dy;
        if (best < 0 || d2 < bestD)
        {
            secondD = (best < 0) ? -1.0f : bestD;
            bestD = d2;
            best = i;
        }
        else if (secondD < 0.0f || d2 < secondD)
        {
            secondD = d2;
        }
    }
    if (best < 0)
    {
        return -1;
    }
    if (bestD > RCL_ATTRIB_R)
    {
        return -1;
    }
    if (secondD >= 0.0f && bestD * RCL_ATTRIB_MARGIN > secondD)
    {
        return -1;
    }
    return (rcl_pl_team[best] == rcl_own_team_b) ? 1 : 0;
}


uintptr_t rcl_own_ptr = 0;

int rcl_own_index = -1;

const char *rcl_own_from = "none";








uintptr_t rcl_players_object = 0;

uintptr_t rcl_players_array = 0;

int rcl_players_count = 0;

uintptr_t rcl_own_elem_scan = 0;

int rcl_state_code = RCL_STATE_INIT;

int rcl_pl_n = 0;

int32_t rcl_pl_x[12];

int32_t rcl_pl_y[12];

int32_t rcl_pl_team[12];

int rcl_mate_n = 0;

int32_t rcl_mate_x[8];

int32_t rcl_mate_y[8];

int rcl_own_team_b = -1;

int rcl_team_trust = 1;

int32_t rcl_enemy_x[12];

int32_t rcl_enemy_y[12];

int rcl_enemy_n = 0;

void rcl_roster(uintptr_t ownElem, int ownIndex, int ownTeam, const rcl_obj_t *objects, int usable)
{
    int i = 0;
    int ownSide = 0;
    int hist[12];
    int hn = 0;
    rcl_pl_n = 0;
    rcl_mate_n = 0;
    rcl_enemy_n = 0;
    rcl_own_team_b = ownTeam;
    rcl_team_trust = 1;
    for (i = 0; i < usable && rcl_pl_n < 12; i++)
    {
        int32_t team = 0;
        int isOwn = 0;
        int h = 0;
        if (objects[i].gid < 1000000)
        {
            continue;
        }
        if (objects[i].gid >= 2000000)
        {
            continue;
        }
        team = rcl_team_at(objects, i);
        isOwn =
            (ownElem && objects[i].object == ownElem) ? 1 : ((!ownElem && i == ownIndex) ? 1 : 0);
        rcl_pl_x[rcl_pl_n] = objects[i].x;
        rcl_pl_y[rcl_pl_n] = objects[i].y;
        rcl_pl_team[rcl_pl_n] = team;
        rcl_pl_mine[rcl_pl_n] = isOwn;
        rcl_pl_n++;
        if (isOwn)
        {
            continue;
        }
        for (h = 0; h < hn; h++)
        {
            if (hist[h] == team)
            {
                break;
            }
        }
        if (h == hn && hn < 12)
        {
            hist[hn++] = team;
        }
        if (team != ownTeam)
        {
            continue;
        }
        ownSide++;
        if (rcl_mate_n >= 8)
        {
            continue;
        }
        rcl_mate_x[rcl_mate_n] = objects[i].x;
        rcl_mate_y[rcl_mate_n] = objects[i].y;
        rcl_mate_n++;
    }
    ownSide++;
    if (rcl_pl_n >= 4 && (ownSide > (rcl_pl_n / 2) || hn < 2))
    {
        rcl_team_trust = 0;
        rcl_mate_n = 0;
    }
    {
        int ownSpawn = -1;
        int k = 0;
        for (k = 0; k < rcl_pl_n; k++)
        {
            if (rcl_pl_mine[k])
            {
                ownSpawn = k;
                break;
            }
        }
        {
            int same = 1;
            for (k = 1; k < rcl_pl_n; k++)
            {
                if (rcl_pl_team[k] != rcl_pl_team[0])
                {
                    same = 0;
                }
            }
            if (rcl_pl_n > 1 && same && rcl_team_trust)
            {
                rcl_team_trust = 0;
            }
        }
        if (!rcl_team_trust)
        {
            int m = 0;
            int e = 0;
            rcl_mate_n = 0;
            rcl_enemy_n = 0;
            for (k = 0; k < rcl_pl_n; k++)
            {
                float dx = 0.0f;
                float dy = 0.0f;
                if (rcl_pl_mine[k])
                {
                    continue;
                }
                dx = (float)(rcl_pl_x[k] - rcl_pl_x[ownSpawn]);
                dy = (float)(rcl_pl_y[k] - rcl_pl_y[ownSpawn]);
                if (sqrtf(dx * dx + dy * dy) <= RCL_CLUSTER)
                {
                    if (m < 8)
                    {
                        rcl_mate_x[m] = rcl_pl_x[k];
                        rcl_mate_y[m] = rcl_pl_y[k];
                        m++;
                    }
                }
                else if (e < 12)
                {
                    rcl_enemy_x[e] = rcl_pl_x[k];
                    rcl_enemy_y[e] = rcl_pl_y[k];
                    e++;
                }
            }
            rcl_mate_n = m;
            rcl_enemy_n = e;
        }
    }
    if (rcl_team_trust)
    {
        for (i = 0; i < rcl_pl_n && rcl_enemy_n < 12; i++)
        {
            if (rcl_pl_mine[i])
            {
                continue;
            }
            if (rcl_pl_team[i] == ownTeam)
            {
                continue;
            }
            rcl_enemy_x[rcl_enemy_n] = rcl_pl_x[i];
            rcl_enemy_y[rcl_enemy_n] = rcl_pl_y[i];
            rcl_enemy_n++;
        }
    }
}

int rcl_cand_frame[3];

int rcl_cand_changes[3];

int rcl_dead_slot = -1;

int rcl_dead_value = 0;

int rcl_pending = 0;

int rcl_pending_tick = 0;

int rcl_pre[3];

int rcl_prev_valid = 0;

int rcl_respawn_tick = 0;











uintptr_t rcl_own_ptr_a = 0;

uintptr_t rcl_own_ptr_b = 0;

int rcl_own_index_3 = -1;





uintptr_t rcl_own_elem = 0;
int32_t rcl_own_gid = 0;
int rcl_no_source_passes = 0;







int rcl_resolve_own_fallback(const rcl_obj_t *objects, int usable, int *indexOut,
                             const char **fromOut)
{
    if (indexOut)
    {
        *indexOut = -1;
    }
    if (fromOut)
    {
        *fromOut = "none";
    }
    if (!objects || usable <= 0)
    {
        return 0;
    }
    if (rcl_own_index >= 0 && rcl_own_ptr)
    {
        int seen = 0;
        int i;
        for (i = 0; i < usable; i++)
        {
            if (objects[i].object == rcl_own_ptr)
            {
                seen = 1;
                break;
            }
        }
        if (seen)
        {
            if (indexOut)
            {
                *indexOut = i;
            }
            if (fromOut)
            {
                *fromOut = rcl_own_from;
            }
            return 1;
        }
    }
    if (rcl_own_index_3 >= 0 && rcl_own_index_3 < usable &&
        objects[rcl_own_index_3].object == rcl_own_ptr_b)
    {
        if (indexOut)
        {
            *indexOut = rcl_own_index_3;
        }
        if (fromOut)
        {
            *fromOut = "scan";
        }
        return 1;
    }
    return 0;
}




int rcl_ascii_word(uintptr_t address)
{
    uint8_t bytes[8];
    int printable = 0;
    if (!rcl_read_bytes(address, bytes, sizeof(bytes)))
    {
        return 0;
    }
    for (int i = 0; i < 8; i++)
    {
        if (bytes[i] >= 0x20 && bytes[i] <= 0x7e)
        {
            printable++;
        }
    }
    return printable == 8 ? 1 : 0;
}

int rcl_word_ascii(uint64_t value)
{
    uint8_t bytes[8];
    int printable = 0;
    memcpy(bytes, &value, sizeof(bytes));
    for (int i = 0; i < 8; i++)
    {
        if (bytes[i] >= 0x20 && bytes[i] <= 0x7e)
        {
            printable++;
        }
    }
    return printable;
}

int rcl_element_ascii(uintptr_t element)
{
    if (rcl_ascii_word(element))
    {
        return 1;
    }
    return rcl_word_ascii((uint64_t)element) == 8 ? 1 : 0;
}




