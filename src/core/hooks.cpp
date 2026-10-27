#include <frida-gum.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern "C"
{
#include "hook.h"
}

extern int gum_live;

typedef struct
{
    uintptr_t address;
    uintptr_t base;
} image_lookup_t;

static gboolean image_lookup_cb(GumModule* module, gpointer userData)
{
    image_lookup_t* ctx = (image_lookup_t*)userData;
    const GumMemoryRange* range = NULL;
    if (!module || !ctx)
    {
        return TRUE;
    }
    range = gum_module_get_range(module);
    if (!range)
    {
        return TRUE;
    }
    if (ctx->address >= (uintptr_t)range->base_address &&
        ctx->address < (uintptr_t)range->base_address + (uintptr_t)range->size)
    {
        ctx->base = (uintptr_t)range->base_address;
        return FALSE;
    }
    return TRUE;
}

uintptr_t hook_image_base(uintptr_t address)
{
    image_lookup_t ctx;
    if (!address || !gum_live)
    {
        return 0;
    }
    ctx.address = address;
    ctx.base = 0;
    gum_process_enumerate_modules(image_lookup_cb, &ctx);
    return ctx.base;
}
