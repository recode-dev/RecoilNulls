#include <frida-gum.h>
#include <stdint.h>
#include <string.h>

extern "C"
{
#include "hook.h"
}

#define HOOKS_MAX 64

static GumInterceptor* interceptor = NULL;
static gpointer hooked[HOOKS_MAX];
static int hooked_count = 0;

int hooks_install(uintptr_t imageBase,
                  const hook_t* hooks,
                  int count,
                  void** originals)
{
    int i = 0;
    int installed = 0;
    if (!imageBase || !hooks || count <= 0)
    {
        return 0;
    }
    if (!interceptor)
    {
        interceptor = gum_interceptor_obtain();
    }
    if (!interceptor)
    {
        return 0;
    }
    gum_interceptor_begin_transaction(interceptor);
    for (i = 0; i < count; i++)
    {
        gpointer target = NULL;
        gpointer original = NULL;
        if (originals)
        {
            originals[i] = NULL;
        }
        if (!hooks[i].rva || !hooks[i].replacement)
        {
            continue;
        }
        target = (gpointer)(imageBase + hooks[i].rva);
        original = NULL;
        gum_interceptor_replace(interceptor,
                                target,
                                (gpointer)hooks[i].replacement,
                                NULL,
                                &original);
        if (!original)
        {
            continue;
        }
        if (originals)
        {
            originals[i] = original;
        }
        if (hooked_count < HOOKS_MAX)
        {
            hooked[hooked_count++] = target;
        }
        installed++;
    }
    gum_interceptor_end_transaction(interceptor);
    gum_interceptor_flush(interceptor);
    return installed;
}

int hooks_uninstall(void)
{
    int i = 0;
    int removed = 0;
    if (!interceptor)
    {
        return 0;
    }
    gum_interceptor_begin_transaction(interceptor);
    for (i = 0; i < hooked_count; i++)
    {
        gum_interceptor_revert(interceptor, hooked[i]);
        removed++;
    }
    gum_interceptor_end_transaction(interceptor);
    gum_interceptor_flush(interceptor);
    hooked_count = 0;
    return removed;
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
