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
