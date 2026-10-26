#ifndef HOOK_H
#define HOOK_H
#include <mach/mach.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif
#define HOOK_SLOTS_MAX 32
    typedef struct
    {
        uintptr_t rva;
        void* replacement;
        int control;
    } hook_t;
    int hooks_install(uintptr_t imageBase,
                      const hook_t* hooks,
                      int count,
                      void** originals);
    int hooks_uninstall(void);
    uintptr_t hook_image_base(uintptr_t address);
    void hook_set_error(const char* format, ...);
    const char* hook_last_error(void);
#ifdef __cplusplus
}
#endif
#endif
