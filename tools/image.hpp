#pragma once

#include <cstdint>
#include <cstddef>

namespace tools::image
{
    struct Segment
    {
        const char *name;
        uintptr_t start;
        uintptr_t end;
        int initprot;
        int maxprot;
    };

    using SegmentVisitor = bool (*)(const Segment &segment, void *user);

    uintptr_t base_exact(const char *name);
    uintptr_t base_for(uintptr_t address);

    bool enumerate_segments(uintptr_t base, SegmentVisitor visitor, void *user);
}
