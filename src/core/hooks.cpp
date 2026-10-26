#include <stdint.h>
#include <string.h>

extern "C"
{
#include "hook.h"
#include <mach-o/dyld.h>
}

int hooks_install(uintptr_t imageBase,
                  const hook_t* hooks,
                  int count,
                  void** originals)
{
    int i = 0;
    (void)imageBase;
    (void)hooks;
    (void)count;
    if (originals)
    {
        for (i = 0; i < count; i++)
        {
            originals[i] = NULL;
        }
    }
    return 0;
}

int hooks_uninstall(void)
{
    return 0;
}

uintptr_t hook_image_base(uintptr_t address)
{
    uint32_t count = 0;
    uint32_t i = 0;
    uintptr_t best = 0;
    if (!address)
    {
        return 0;
    }
    count = _dyld_image_count();
    if (count > 8192)
    {
        count = 8192;
    }
    for (i = 0; i < count; i++)
    {
        uintptr_t base = (uintptr_t)_dyld_get_image_header(i);
        if (base && base <= address && base > best)
        {
            best = base;
        }
    }
    return best;
}
