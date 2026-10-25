#pragma once

#include <cstdint>
#include <cstddef>
#include <objc/runtime.h>

namespace tools::swizzle
{
    constexpr int MAX_TARGETS = 32;

    struct Target
    {
        const char *cls;
        const char *sel;
        bool classMethod;
        void *replacement;
        int control;
    };

    struct Entry
    {
        Class cls;
        SEL sel;
        IMP original;
        IMP replacement;
        bool classMethod;
        int control;
        bool used;
    };

    int install(const Target *targets, int count, void **originals);

    void *install(const char *cls, const char *sel, void *replacement, bool classMethod = false);

    int uninstall();

    bool armed(const char *cls, const char *sel);

    const Entry *entry_at(int index);
}
