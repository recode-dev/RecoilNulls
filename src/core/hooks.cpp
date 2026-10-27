#include <frida-gum.h>
#include <stdint.h>

extern "C"
{
#include "hook.h"
}

uintptr_t hook_image_base(uintptr_t address)
{
    GumModule* module = NULL;
    const GumMemoryRange* range = NULL;
    uintptr_t found = 0;
    if (!address)
    {
        return 0;
    }
    module = gum_process_find_module_by_address((GumAddress)address);
    if (!module)
    {
        return 0;
    }
    range = gum_module_get_range(module);
    if (range)
    {
        found = (uintptr_t)range->base_address;
    }
    g_object_unref(module);
    return found;
}
