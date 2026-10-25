#pragma once

#include <cstdint>
#include <cstddef>

namespace tools::image
{
    struct Module
    {
        uintptr_t base;
        uintptr_t size;
        const char *path;
    };

    struct Segment
    {
        const char *name;
        uintptr_t start;
        uintptr_t end;
        int initprot;
        int maxprot;
    };

    using Visitor = bool (*)(const Module &module, void *user);
    using SegmentVisitor = bool (*)(const Segment &segment, void *user);

    bool enumerate_segments(uintptr_t base, SegmentVisitor visitor, void *user);

    void enumerate(Visitor visitor, void *user);

    uintptr_t base_exact(const char *name);

    uintptr_t base_for(uintptr_t address);

    uintptr_t slide_for(uintptr_t base);

    bool range_for(uintptr_t base, uintptr_t &start, uintptr_t &end);

    bool owns(uintptr_t base, uintptr_t address);

    bool in_image(uintptr_t address);
}
