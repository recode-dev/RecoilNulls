#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>
#include <cstring>
#include <mach/mach.h>

#include "offsets.hpp"
#include "scstring.hpp"

namespace
{
    constexpr int32_t INLINE_MAX_LENGTH = 7;
    constexpr size_t HEADER_SIZE = 16;

    bool readBytes(uintptr_t address, void *out, size_t size)
    {
        if (address == 0 || out == nullptr || size == 0) return false;
        vm_size_t copied = 0;
        kern_return_t result = vm_read_overwrite(mach_task_self(),
                                                (vm_address_t)address,
                                                (vm_size_t)size,
                                                (vm_address_t)out,
                                                &copied);
        return result == KERN_SUCCESS && copied == (vm_size_t)size;
    }
}

size_t readScStringInto(uintptr_t value, char *out, size_t capacity)
{
    if (value == 0 || out == nullptr || capacity < 2) return 0;

    uint8_t head[HEADER_SIZE];
    if (!readBytes(value + offsets::ScString_length, head, HEADER_SIZE)) return 0;

    const int32_t length = (int32_t)((uint32_t)head[0] |
        ((uint32_t)head[1] << 8) |
        ((uint32_t)head[2] << 16) |
        ((uint32_t)head[3] << 24));
    if (length <= 0) return 0;

    const size_t limit = capacity - 1;
    const size_t want = (size_t)length < limit ? (size_t)length : limit;

    if (length <= INLINE_MAX_LENGTH)
    {
        memcpy(out, head + 4, want);
        out[want] = 0;
        return want;
    }

    uintptr_t data = 0;
    for (size_t i = 0; i < 8; i++) data |= (uintptr_t)head[4 + i] << (8 * i);
    if (data == 0) return 0;

    if (!readBytes(data, out, want)) return 0;
    out[want] = 0;
    return want;
}

std::optional<std::string> readScString(uintptr_t value, size_t maxLength)
{
    if (maxLength == 0) return std::nullopt;

    char buffer[257];
    const size_t capacity = maxLength < 256 ? maxLength + 1 : sizeof(buffer);
    const size_t length = readScStringInto(value, buffer, capacity);
    if (length == 0) return std::nullopt;
    return std::string(buffer, length);
}
