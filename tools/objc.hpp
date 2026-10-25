#pragma once

#include <cstdint>
#include <cstddef>
#include <objc/runtime.h>

namespace tools::objc
{
    Class find_class(const char *name);

    Method find_method(Class cls, const char *selector, bool classMethod = false);

    IMP implementation(Class cls, const char *selector, bool classMethod = false);

    IMP swap(Class cls, const char *selector, IMP replacement, bool classMethod = false);

    bool restore(Class cls, const char *selector, IMP original, bool classMethod = false);

    const char *type_encoding(Class cls, const char *selector, bool classMethod = false);
}
