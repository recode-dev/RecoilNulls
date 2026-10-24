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
    void configure(const Options &options);
    void update(uint32_t nowMs);
    bool dodgeDir(float &x, float &y);
    bool sendBattleMove(uintptr_t logic, float tx, float ty);
    bool clampTarget(float tx, float ty, float &outX, float &outY);
}
