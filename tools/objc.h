#ifndef OBJC_H
#define OBJC_H
#include <objc/runtime.h>
#include <cstdint>
#ifdef __cplusplus
extern "C"
{
#endif
    using objc_hook_fn = void (*)();
    int objc_arm(uintptr_t imageBase,
                 const char* clsName,
                 const char* selName,
                 objc_hook_fn body);
#ifdef __cplusplus
}
#endif
#endif
