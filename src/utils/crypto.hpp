#pragma once

#include <cstdint>
#include <cstddef>

namespace crypto
{
    void sha256(const uint8_t *data, size_t size, uint8_t out[32]);
}
