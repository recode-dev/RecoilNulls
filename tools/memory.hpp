#pragma once

#include <cstdint>
#include <cstddef>

// Guarded in-process memory access.
//
// Every read validates the mapping first, so a bad offset degrades into a
// `false` return instead of a SIGSEGV. Writes temporarily lift page protection
// (vm_protect up to maxprot) and drop it back with close_windows().

namespace tools::memory
{
    struct Region
    {
        uintptr_t start;
        uintptr_t end;
        int prot;      // current protection
        int maxprot;   // maximum protection the mapping allows
    };

    bool region_of(uintptr_t address, Region &out);

    bool readable(uintptr_t address, size_t size);
    bool writable(uintptr_t address, size_t size);

    bool read(uintptr_t address, void *out, size_t size);
    bool write(uintptr_t address, const void *data, size_t size);

    // Drops every page temporarily opened by write().
    void close_windows();

    template <typename T>
    bool get(uintptr_t address, T &out)
    {
        return read(address, &out, sizeof(T));
    }

    template <typename T>
    bool put(uintptr_t address, const T &value)
    {
        return write(address, &value, sizeof(T));
    }

    // A pointer that could plausibly be dereferenced on arm64 iOS.
    bool plausible(uintptr_t pointer, size_t size = 8);
}
