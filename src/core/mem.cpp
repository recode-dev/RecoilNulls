#include <CoreFoundation/CFDate.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <cstdint>
#include <cstring>
#include "offsets.h"
#include "scan.h"
#include <frida-gum.h>

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
        *protection = static_cast<vm_prot_t>(pageProtection);
    }
    if (maxProtection)
    {
        *maxProtection = static_cast<vm_prot_t>(pageProtection);
    }
    if (regionSize)
    {
        *regionSize = static_cast<mach_vm_size_t>(pageSize);
    }
    if (regionStart)
    {
        *regionStart = address & ~(static_cast<uintptr_t>(pageSize) - 1);
    }
    return YES;
}
BOOL addr_writable(uintptr_t address, size_t length)
{
    return writable(address, length);
}
inline constexpr int PAGE_CACHE_SLOTS = 256;
static __thread uintptr_t page_cache_key[PAGE_CACHE_SLOTS];
static __thread uint8_t page_cache_ok[PAGE_CACHE_SLOTS];
void read_cache_reset()
{
    for (int i = 0; i < PAGE_CACHE_SLOTS; i++)
        page_cache_key[i] = 0;
}
BOOL read_bytes(uintptr_t address, void* out, size_t length)
{
    GumPageProtection protection = GUM_PAGE_NO_ACCESS;
    guint pageSize = 0;
    uintptr_t end = 0;
    uintptr_t cursor = 0;
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
    end = address + length;
    if (end <= address)
    {
        return NO;
    }
    pageSize = gum_query_page_size();
    if (!pageSize)
    {
        return NO;
    }
    cursor = address;
    for (int guard = 0; cursor < end && guard < 64; guard++)
    {
        uintptr_t page = cursor & ~(static_cast<uintptr_t>(pageSize) - 1);
        int slot = (int)((page >> 12) & (PAGE_CACHE_SLOTS - 1));
        uint8_t ok = 0;
        if (page_cache_key[slot] == page)
        {
            ok = page_cache_ok[slot];
        }
        else
        {
            if (gum_memory_query_protection((gconstpointer)page, &protection))
                ok = (protection & GUM_PAGE_READ) ? 1 : 0;
            page_cache_key[slot] = page;
            page_cache_ok[slot] = ok;
        }
        if (!ok)
        {
            return NO;
        }
        cursor = page + static_cast<uintptr_t>(pageSize);
    }
    if (cursor < end)
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
        next = (cursor & ~(static_cast<uintptr_t>(pageSize) - 1)) +
               static_cast<uintptr_t>(pageSize);
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
    return gum_memory_write((gpointer)address,
                            static_cast<const guint8*>(src),
                            static_cast<gsize>(length))
               ? YES
               : NO;
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
    GumModule* module = nullptr;
    const GumMemoryRange* range = nullptr;
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
