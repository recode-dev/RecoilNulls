#pragma once

#include <cstdint>

namespace autoaim
{
    void setupAutoaim(uintptr_t baseAddress);
    void updateAutoaim(bool enabled);
    void resetAutoaim();
    bool hasTarget();
}
