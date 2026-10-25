#pragma once

#include <objc/runtime.h>

namespace tools::objc
{
    Class find_class(const char *name);

    IMP swap(Class cls, const char *selector, IMP replacement, bool classMethod = false);

    bool restore(Class cls, const char *selector, IMP original, bool classMethod = false);
}
