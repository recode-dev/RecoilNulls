#pragma once

#include <cstdint>
#include <cstddef>

namespace tools::memory
{
    constexpr uintptr_t MIN_ADDRESS = 0x100000000ULL;

    bool readable(uintptr_t address, size_t size);
    bool read(uintptr_t address, void *out, size_t size);
    bool write(uintptr_t address, const void *data, size_t size);
    void close_windows();

    bool plausible(uintptr_t pointer);
}
