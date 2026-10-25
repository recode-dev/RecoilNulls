#pragma once

#include <cstdint>
#include <cstddef>

// Pointer-slot hooking.
//
// Instead of patching instructions, this rewrites every data pointer inside the
// target image that points at the function we want to intercept (vtables,
// function-pointer tables, dispatch slots) so it points at our replacement.
//
// Why: instruction patching on arm64 needs prologue relocation, cache flushes
// and correct unwind info; a single mistake there kills the process before any
// of our code runs. Swapping a pointer is a plain aligned 8-byte store into a
// writable segment, verifiable immediately, and reversible.
//
// The replacement keeps the original signature and calls the original through
// the pointer handed back in `originals`.

namespace tools::hook
{
    constexpr int MAX_HOOKS = 32;
    constexpr int MAX_SLOTS_PER_HOOK = 64;

    struct Spec
    {
        uintptr_t rva;         // offset from the image base
        void *replacement;     // our function
        uint32_t siteHash;     // optional FNV-1a of the first 16 bytes, 0 disables
        int control;           // free-form tag for the caller
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

    // Installs every spec that passes the probes. `originals[i]` receives the
    // call-through pointer for spec i (nullptr when that spec was skipped).
    int install(uintptr_t imageBase, const Spec *specs, int count, void **originals);

    // Single hook. Returns the call-through pointer, or nullptr on failure.
    void *install(uintptr_t imageBase, uintptr_t rva, void *replacement);

    // Restores every pointer written by install(). Returns how many hooks were
    // rolled back.
    int uninstall();

    // Call-through for a hook installed earlier (nullptr when unknown).
    void *original(uintptr_t target);

    // Sanity check: does this address look like an arm64 function entry?
    bool prologue_ok(uintptr_t address);

    // FNV-1a over the first 16 bytes — cheap guard against offset drift.
    uint32_t site_hash(uintptr_t address);

    // How many pointer slots currently reference `target`.
    int probe(uintptr_t target);

    void set_verbose(bool enabled);
    void set_error(const char *format, ...);
    const char *last_error();
}
