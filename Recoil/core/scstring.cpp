#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>
#include <mach/mach.h>

#include "offsets.cpp"

namespace
{
    constexpr int32_t INLINE_MAX_LENGTH = 7;
    constexpr size_t DEFAULT_MAX_LENGTH = 256;

    bool readRaw(uintptr_t address, void *out, size_t size)
    {
        if (address == 0 || out == nullptr || size == 0) return false;
        vm_size_t read = 0;
        kern_return_t status = vm_read_overwrite(mach_task_self(), (vm_address_t)address, (vm_size_t)size, (vm_address_t)out, &read);
        return status == KERN_SUCCESS && read == size;
    }

    bool readS32(uintptr_t address, int32_t &out)
    {
        return readRaw(address, &out, sizeof(out));
    }

    bool readPointer(uintptr_t address, uintptr_t &out)
    {
        return readRaw(address, &out, sizeof(out));
    }
}

std::optional<std::string> readScString(uintptr_t value, size_t maxLength = DEFAULT_MAX_LENGTH)
{
    int32_t length = 0;
    if (!readS32(value + offsets::ScString_length, length)) return std::nullopt;
    if (length <= 0 || (size_t)length > maxLength) return std::nullopt;

    uintptr_t data = 0;
    if ((int32_t)length <= INLINE_MAX_LENGTH)
    {
        data = value + offsets::ScString_data;
    }
    else if (!readPointer(value + offsets::ScString_data, data))
    {
        return std::nullopt;
    }

    if (data == 0) return std::nullopt;

    std::string text((size_t)length, '\0');
    if (!readRaw(data, &text[0], (size_t)length)) return std::nullopt;

    return text;
}