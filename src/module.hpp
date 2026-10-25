#pragma once

#include <cstdint>

namespace module
{
    // Resolves the game image, binds offsets and arms every hook.
    bool install(uintptr_t base);

    bool is_installed();
    uintptr_t base_address();

    // One frame of work. Safe to call from several drivers: it filters by the
    // in-battle state and rate-limits itself.
    void pump(uintptr_t client);
    void pump();

    namespace state
    {
        inline bool autododge = true;
        inline bool autoaim = true;
    }
}
