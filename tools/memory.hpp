#pragma once

#include <cstdint>
#include <cstddef>

namespace tools::memory
{
    struct Region
    {
        uintptr_t start;
        uintptr_t end;
        int prot;
        int maxprot;
    };

    bool region_of(uintptr_t address, Region &out);

    bool readable(uintptr_t address, size_t size);
    bool writable(uintptr_t address, size_t size);

    bool read(uintptr_t address, void *out, size_t size);
    bool write(uintptr_t address, const void *data, size_t size);

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

    bool plausible(uintptr_t pointer, size_t size = 8);
}
