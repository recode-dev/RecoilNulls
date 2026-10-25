#pragma once

#include <cstdint>

namespace module
{
    bool install(uintptr_t base);

    namespace state
    {
        inline bool autododge = true;
        inline bool autoaim = true;
    }
}
