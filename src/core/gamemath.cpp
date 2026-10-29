#include "gamemath.h"

#include "commands.h"
#include "offsets.h"

int lm_sqrt(int value)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICMATH__SQRT);
    }
    if (!slot)
    {
        return 0;
    }
    return reinterpret_cast<int (*)(int)>(slot)(value);
}

int lm_sin(int degrees)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICMATH__SIN);
    }
    if (!slot)
    {
        return 0;
    }
    return reinterpret_cast<int (*)(int)>(slot)(degrees);
}

int lm_cos(int degrees)
{
    // No standalone cos() survives in v69: the engine defines it as sin(a+90),
    // which is exactly what the v36 body does before the table lookup.
    return lm_sin(degrees + 90);
}

int lm_get_angle(int x, int y)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICMATH__GETANGLE);
    }
    if (!slot)
    {
        return 0;
    }
    return reinterpret_cast<int (*)(int, int)>(slot)(x, y);
}

int lm_distance_square_to_line(int ax, int ay, int bx, int by, int px, int py)
{
    static uintptr_t slot = 0;
    if (!slot)
    {
        slot = callable_at_rva(LOGICMATH__GETDISTANCESQUARETOLINE);
    }
    if (!slot)
    {
        return 0;
    }
    int a[2] = {ax, ay};
    int b[2] = {bx, by};
    int p[2] = {px, py};
    return reinterpret_cast<int (*)(const int*, const int*, const int*)>(slot)(
        a, b, p);
}

float lm_distance_to_line(
    float ax, float ay, float bx, float by, float px, float py)
{
    int sq = lm_distance_square_to_line(static_cast<int>(ax + 0.5f),
                                        static_cast<int>(ay + 0.5f),
                                        static_cast<int>(bx + 0.5f),
                                        static_cast<int>(by + 0.5f),
                                        static_cast<int>(px + 0.5f),
                                        static_cast<int>(py + 0.5f));
    if (sq < 1)
    {
        sq = 1;
    }
    return static_cast<float>(lm_sqrt(sq));
}
