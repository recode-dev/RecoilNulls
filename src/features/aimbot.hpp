#pragma once

#include <cstdint>

namespace aimbot
{
    void setupAimbot(uintptr_t baseAddress);
    void updateAimbot(bool enabled);
    void resetAimbot();
    bool hasTarget();
}
