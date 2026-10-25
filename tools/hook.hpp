#pragma once

#include <cstdint>
#include <cstddef>

namespace tools::hook
{
    constexpr int MAX_HOOKS = 32;
    constexpr int MAX_SLOTS_PER_HOOK = 64;

    struct Spec
    {
        uintptr_t rva;
        void *replacement;
        uint32_t siteHash;
        int control;
    };

    struct Entry
    {
        uintptr_t target;
        uintptr_t replacement;
        uintptr_t slots[MAX_SLOTS_PER_HOOK];
        int count;
        int control;
        bool used;
    };

    int install(uintptr_t imageBase, const Spec *specs, int count, void **originals);

    void *install(uintptr_t imageBase, uintptr_t rva, void *replacement);

    int uninstall();

    void *original(uintptr_t target);

    bool prologue_ok(uintptr_t address);

    uint32_t site_hash(uintptr_t address);

    int probe(uintptr_t target);

    void set_verbose(bool enabled);
    void set_error(const char *format, ...);
    const char *last_error();
}
