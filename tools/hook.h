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
    uintptr_t hook_image_base(uintptr_t address);
#ifdef __cplusplus
}
#endif
#endif
