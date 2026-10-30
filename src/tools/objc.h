#ifndef OBJC_H
#define OBJC_H
#include <objc/runtime.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
    using objc_body_t = void (*)(void);
    int objc_arm(uintptr_t   imageBase,
                 const char* clsName,
                 const char* selName,
                 objc_body_t body);
#ifdef __cplusplus
}
#endif
#endif
