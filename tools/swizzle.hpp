#pragma once

#include <cstdint>
#include <cstddef>
#include <objc/runtime.h>

// Objective-C swizzling, table driven.
//
// Same shape as tools::hook: declare the targets, install them all at once,
// keep the original IMPs for call-through, roll back with uninstall().

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

    // Returns how many targets were armed. `originals[i]` is the previous IMP.
    int install(const Target *targets, int count, void **originals);

    // Convenience single target. Returns the previous IMP or nullptr.
    void *install(const char *cls, const char *sel, void *replacement, bool classMethod = false);

    int uninstall();

    bool armed(const char *cls, const char *sel);

    const Entry *entry_at(int index);
}
