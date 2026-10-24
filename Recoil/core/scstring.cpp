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

    bool readInt32(uintptr_t address, int32_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }

    bool readPointer(uintptr_t address, uintptr_t &out)
    {
        return readBytes(address, &out, sizeof(out));
    }
}

std::optional<std::string> readScString(uintptr_t value, size_t maxLength)
{
    if (value == 0) return std::nullopt;

    int32_t length = 0;
    if (!readInt32(value + offsets::ScString_length, length)) return std::nullopt;
    if (length <= 0 || (size_t)length > maxLength) return std::nullopt;

    uintptr_t data = value + offsets::ScString_data;
    if (length > INLINE_MAX_LENGTH && !readPointer(value + offsets::ScString_data, data)) return std::nullopt;
    if (data == 0) return std::nullopt;

    std::string out((size_t)length, '\0');
    if (!readBytes(data, out.data(), (size_t)length)) return std::nullopt;

    return out;
}
