#include <frida-gum.h>
#include <stdint.h>
#include <string.h>

extern "C"
{
#include "hook.h"
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

typedef struct
{
    uintptr_t address;
    uintptr_t base;
} image_lookup_t;

static gboolean image_lookup_cb(GumModule* module, gpointer userData)
{
    image_lookup_t* ctx = (image_lookup_t*)userData;
    const GumMemoryRange* range = NULL;
    uintptr_t start = 0;
    if (!module || !ctx)
    {
        return TRUE;
    }
    range = gum_module_get_range(module);
    if (!range)
    {
        return TRUE;
    }
    start = (uintptr_t)range->base_address;
    if (ctx->address >= start && ctx->address < start + (uintptr_t)range->size)
    {
        ctx->base = start;
        return FALSE;
    }
    return TRUE;
}

uintptr_t hook_image_base(uintptr_t address)
{
    image_lookup_t ctx;
    if (!address)
    {
        return 0;
    }
    ctx.address = address;
    ctx.base = 0;
    gum_init_embedded();
    gum_process_enumerate_modules(image_lookup_cb, &ctx);
    return ctx.base;
}
