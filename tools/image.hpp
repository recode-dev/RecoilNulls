#pragma once

#include <cstdint>
#include <cstddef>

// Mach-O image (dylib / executable) helpers.
//
// Everything here is read-only inspection of the dyld image list: no patching,
// no allocation, safe to call from any thread once dyld has loaded the target.

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

    // Iterates LC_SEGMENT_64 entries of one image with resolved addresses.
    bool enumerate_segments(uintptr_t base, SegmentVisitor visitor, void *user);

    // Iterates every loaded image. Return false from the visitor to stop early.
    void enumerate(Visitor visitor, void *user);

    // Case-insensitive substring match on the image path ("nulls", "Brawl", ...).
    uintptr_t base_named(const char *needle);

    // Base address of the image that owns `address` (0 when unmapped).
    uintptr_t base_for(uintptr_t address);

    // Slide of a loaded image (base - __TEXT.vmaddr).
    uintptr_t slide_for(uintptr_t base);

    // Real in-memory start/end of the image, computed from its segments.
    bool range_for(uintptr_t base, uintptr_t &start, uintptr_t &end);

    // True when `address` falls inside the image `base`.
    bool owns(uintptr_t base, uintptr_t address);

    // True when the address belongs to any loaded image.
    bool in_image(uintptr_t address);
}
