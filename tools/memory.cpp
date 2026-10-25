#include "memory.hpp"

#include <mach/mach.h>
#include <mach/vm_map.h>
#include <string.h>

namespace
{
    constexpr uintptr_t PAGE_MASK = ~(uintptr_t)0xFFF;

    struct Region
    {
        uintptr_t start;
        uintptr_t end;
        int prot;
        int maxprot;
    };

    uintptr_t g_regionStart = 0;
    uintptr_t g_regionEnd = 0;
    int g_regionProt = 0;
    int g_regionMax = 0;
    bool g_regionValid = false;

    uintptr_t g_windowStart = 0;
    uintptr_t g_windowEnd = 0;
    int g_windowSaved = 0;
    bool g_windowOpen = false;

    bool region_query(uintptr_t address, Region &out)
    {
        if (address == 0) return false;

        vm_address_t region = (vm_address_t)address;
        vm_size_t size = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;

        const kern_return_t kr = vm_region_64(
            mach_task_self(),
            &region,
            &size,
            VM_REGION_BASIC_INFO_64,
            (vm_region_info_t)&info,
            &count,
            &object);

        if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
        if (kr != KERN_SUCCESS || size == 0) return false;

        out.start = (uintptr_t)region;
        out.end = (uintptr_t)region + (uintptr_t)size;
        out.prot = (int)info.protection;
        out.maxprot = (int)info.max_protection;

        return true;
    }

    bool region_cached(uintptr_t address, Region &out)
    {
        if (g_regionValid && address >= g_regionStart && address < g_regionEnd)
        {
            out.start = g_regionStart;
            out.end = g_regionEnd;
            out.prot = g_regionProt;
            out.maxprot = g_regionMax;

            return true;
        }

        Region fresh;
        if (!region_query(address, fresh))
        {
            g_regionValid = false;
            return false;
        }

        g_regionStart = fresh.start;
        g_regionEnd = fresh.end;
        g_regionProt = fresh.prot;
        g_regionMax = fresh.maxprot;
        g_regionValid = true;

        out = fresh;

        return true;
    }

    bool page_protect(uintptr_t start, uintptr_t end, int prot, bool asMaximum)
    {
        if (end <= start) return false;

        const kern_return_t kr = vm_protect(
            mach_task_self(),
            (vm_address_t)start,
            (vm_size_t)(end - start),
            asMaximum ? TRUE : FALSE,
            (vm_prot_t)prot);

        return kr == KERN_SUCCESS;
    }

    void window_close()
    {
        if (!g_windowOpen) return;

        page_protect(g_windowStart, g_windowEnd, g_windowSaved, false);

        g_windowStart = 0;
        g_windowEnd = 0;
        g_windowSaved = 0;
        g_windowOpen = false;
    }

    bool window_open(uintptr_t address, const Region &region)
    {
        if (g_windowOpen && address >= g_windowStart && address < g_windowEnd) return true;
        if (g_windowOpen) window_close();

        if (region.prot & VM_PROT_EXECUTE) return false;
        if ((region.prot & VM_PROT_WRITE) == 0 && (region.maxprot & VM_PROT_WRITE) == 0) return false;

        const uintptr_t pageStart = region.start & PAGE_MASK;
        const uintptr_t pageEnd = (region.end + 0xFFF) & PAGE_MASK;

        if ((region.prot & VM_PROT_WRITE) == 0)
        {
            const int lift = region.maxprot | VM_PROT_READ | VM_PROT_WRITE;

            if (!page_protect(pageStart, pageEnd, lift, true)) return false;
            if (!page_protect(pageStart, pageEnd, region.prot | VM_PROT_READ | VM_PROT_WRITE, false)) return false;
        }

        g_windowStart = pageStart;
        g_windowEnd = pageEnd;
        g_windowSaved = region.prot;
        g_windowOpen = true;

        return true;
    }

    bool vm_read_raw(uintptr_t address, void *out, size_t size)
    {
        vm_size_t got = 0;

        const kern_return_t kr = vm_read_overwrite(
            mach_task_self(),
            (vm_address_t)address,
            (vm_size_t)size,
            (vm_address_t)out,
            &got);

        return kr == KERN_SUCCESS && got == (vm_size_t)size;
    }
}

namespace tools::memory
{
    bool readable(uintptr_t address, size_t size)
    {
        if (address == 0 || size == 0) return false;

        Region region;
        if (!region_cached(address, region)) return false;
        if ((region.prot & VM_PROT_READ) == 0) return false;

        return (address + size) <= region.end;
    }

    bool read(uintptr_t address, void *out, size_t size)
    {
        if (address == 0 || out == nullptr || size == 0) return false;

        Region region;
        if (region_cached(address, region) &&
            (region.prot & VM_PROT_READ) != 0 &&
            (address + size) <= region.end)
        {
            memcpy(out, (const void *)address, size);
            return true;
        }

        return vm_read_raw(address, out, size);
    }

    bool write(uintptr_t address, const void *data, size_t size)
    {
        if (address == 0 || data == nullptr || size == 0) return false;

        Region region;
        if (!region_cached(address, region)) return false;
        if ((address + size) > region.end) return false;
        if (region.prot & VM_PROT_EXECUTE) return false;

        if ((region.prot & VM_PROT_WRITE) == 0 && !window_open(address, region)) return false;

        memcpy((void *)address, data, size);

        uintptr_t check = 0;
        if (size == sizeof(check))
        {
            if (!read(address, &check, sizeof(check))) return false;

            return check == *(const uintptr_t *)data;
        }

        return true;
    }

    void close_windows()
    {
        window_close();
    }

    bool plausible(uintptr_t pointer)
    {
        if (pointer < MIN_ADDRESS) return false;
        if ((pointer & 0x7) != 0) return false;

        return readable(pointer, sizeof(uintptr_t));
    }
}
