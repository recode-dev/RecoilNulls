#pragma once

#include <cstdint>
#include <cstddef>

namespace tools::swizzle
{
    constexpr int MAX_TARGETS = 32;

    struct Target
    {
        const char *cls;
        const char *sel;
        bool classMethod;
        void *replacement;
    };

    int install(const Target *targets, int count, void **originals);

    void *install(const char *cls, const char *sel, void *replacement, bool classMethod = false);

    int uninstall();
}
