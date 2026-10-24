#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>
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

std::optional<std::string> readScString(uintptr_t value, size_t maxLength)
{
    if (value == 0) return std::nullopt;

    uint8_t head[HEADER_SIZE];
    if (!readBytes(value + offsets::ScString_length, head, HEADER_SIZE)) return std::nullopt;

    const int32_t length = (int32_t)((uint32_t)head[0] |
        ((uint32_t)head[1] << 8) |
        ((uint32_t)head[2] << 16) |
        ((uint32_t)head[3] << 24));
    if (length <= 0 || (size_t)length > maxLength) return std::nullopt;

    if (length <= INLINE_MAX_LENGTH) return std::string((const char *)head + 4, (size_t)length);

    uintptr_t data = 0;
    for (size_t i = 0; i < 8; i++) data |= (uintptr_t)head[4 + i] << (8 * i);
    if (data == 0) return std::nullopt;

    std::string out((size_t)length, '\0');
    if (!readBytes(data, out.data(), (size_t)length)) return std::nullopt;
    return out;
}
