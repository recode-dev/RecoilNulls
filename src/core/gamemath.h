#ifndef CORE_GAMEMATH_H
#define CORE_GAMEMATH_H

#include <cmath>

// Thin bridges into LogicMath. The engine works in integer logic units and
// integer degrees with a 1024 fixed-point scale for sin/cos, so everything here
// takes and returns integer units and only converts at the call site.
int lm_sqrt(int value);
int lm_sin(int degrees);
int lm_cos(int degrees);
int lm_get_angle(int x, int y);
int lm_distance_square_to_line(
    int ax, int ay, int bx, int by, int px, int py);
float lm_distance_to_line(
    float ax, float ay, float bx, float by, float px, float py);

// sqrt of a squared length in logic units. Keeps the float plumbing of the
// callers but takes the root from the engine instead of libm.
inline float lm_hypot(float x, float y)
{
    float sq = x * x + y * y;
    if (sq < 1.0f)
    {
        sq = 1.0f;
    }
    return static_cast<float>(lm_sqrt(static_cast<int>(sq + 0.5f)));
}

inline float lm_sqrtf(float value)
{
    if (value < 1.0f)
    {
        value = 1.0f;
    }
    return static_cast<float>(lm_sqrt(static_cast<int>(value + 0.5f)));
}

// sin/cos for callers that keep their angles in radians. The engine only knows
// whole degrees, so the angle is rounded to the nearest degree.
inline float lm_sinf(float radians)
{
    int deg = static_cast<int>(radians * 57.29577951f +
                               (radians < 0.0f ? -0.5f : 0.5f));
    return static_cast<float>(lm_sin(deg)) * (1.0f / 1024.0f);
}

inline float lm_cosf(float radians)
{
    int deg = static_cast<int>(radians * 57.29577951f +
                               (radians < 0.0f ? -0.5f : 0.5f));
    return static_cast<float>(lm_cos(deg)) * (1.0f / 1024.0f);
}

#endif
