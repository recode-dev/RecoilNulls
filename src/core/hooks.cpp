#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern "C"
{
#include "hook.h"
#include "../core/gum_boot.h"
#include <mach-o/dyld.h>
}

static jmp_buf gum_jump;
static volatile int gum_in_guard = 0;
static volatile int gum_booted = 0;
static volatile int gum_broken = 0;
static char gum_error_text[384];

const char* gum_last_error(void)
{
    return gum_error_text;
}

static void gum_log_handler(const gchar* domain,
                            GLogLevelFlags level,
                            const gchar* message,
                            gpointer userData)
{
    (void)userData;
    if ((level & (G_LOG_LEVEL_ERROR | G_LOG_FLAG_FATAL)) == 0)
    {
        return;
    }
    snprintf(gum_error_text,
             sizeof(gum_error_text),
             "%s: %s",
             domain ? domain : "log",
             message ? message : "?");
    gum_broken = 1;
    if (gum_in_guard)
    {
        longjmp(gum_jump, 1);
    }
}

static int gum_ready(void)
{
    if (gum_broken)
    {
        return 0;
    }
    if (gum_booted)
    {
        return 1;
    }
    gum_in_guard = 1;
    if (setjmp(gum_jump) == 0)
    {
        g_log_set_default_handler(gum_log_handler, NULL);
        gum_init_embedded();
        gum_booted = 1;
    }
    gum_in_guard = 0;
    if (!gum_booted)
    {
        gum_broken = 1;
    }
    return gum_booted;
}

int gum_enumerate_modules(GumFoundModuleFunc callback, gpointer userData)
{
    int ok = 0;
    if (!callback || !gum_ready())
    {
        return 0;
    }
    gum_in_guard = 1;
    if (setjmp(gum_jump) == 0)
    {
        gum_process_enumerate_modules(callback, userData);
        ok = 1;
    }
    gum_in_guard = 0;
    return ok;
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
