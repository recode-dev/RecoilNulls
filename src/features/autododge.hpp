#pragma once

#include <cstdint>
#include <cstddef>

namespace autododge
{
    struct Options
    {
        float reactionSpeed = 100.0f;
        int32_t directionPrecision = 48;
        int32_t safetyMargin = 25;
    };

    void setup();
    void reset();
    void update(uint32_t nowMs);
}
