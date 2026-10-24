#pragma once

#include <cstdint>

namespace module
{
    bool install();
    bool isInstalled();
    uintptr_t baseAddress();

    namespace state
    {
        inline bool autododge = true;
        inline bool aimbot = true;
    }
}
