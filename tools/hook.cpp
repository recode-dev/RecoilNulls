#include "hook.hpp"
#include "image.hpp"
#include "memory.hpp"

#include <mach/vm_map.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace
{
    constexpr uintptr_t VA_LIMIT = 0x0000FFFFFFFFFFFFULL;
    constexpr size_t SCAN_CHUNK = 4096;
    constexpr int MAX_HOOKS = 32;
    constexpr int MAX_SLOTS = 64;

    struct Entry
    {
        uintptr_t target;
        uintptr_t replacement;
        uintptr_t slots[MAX_SLOTS];
        int count;
        bool used;
    };

    Entry g_hooks[MAX_HOOKS];
    uintptr_t g_scanBuffer[SCAN_CHUNK / sizeof(uintptr_t)];
    char g_error[256];

    Entry *entry_alloc(uintptr_t target)
    {
        for (int i = 0; i < MAX_HOOKS; i++)
        {
            if (g_hooks[i].used && g_hooks[i].target == target) return &g_hooks[i];
        }

        for (int i = 0; i < MAX_HOOKS; i++)
        {
            if (g_hooks[i].used) continue;

            memset(&g_hooks[i], 0, sizeof(g_hooks[i]));
            g_hooks[i].used = true;
            g_hooks[i].target = target;

            return &g_hooks[i];
        }

        return nullptr;
    }

    bool slot_matches(uintptr_t value, uintptr_t needle)
    {
        if (needle == 0) return false;
        if (value == needle) return true;

        return (value & VA_LIMIT) == needle;
    }

    bool prologue_ok(uintptr_t address)
    {
        uint32_t word = 0;
        if (!tools::memory::read(address, &word, sizeof(word))) return false;

        if ((word & 0xFFC003FFu) == 0xD10003FFu) return true;
        if ((word & 0xFF4003E0u) == 0xA90003E0u) return true;
        if ((word & 0xFF4003E0u) == 0xA80003E0u) return true;
        if (word == 0xD503237Fu) return true;
        if (word == 0xD503245Fu) return true;
        if (word == 0xD65F03C0u) return true;
        if (word == 0x910003FDu) return true;
        if ((word & 0xFF000000u) == 0x14000000u) return true;
        if ((word & 0x9F000000u) == 0x10000000u) return true;

        return false;
    }

    struct ScanState
    {
        uintptr_t needle;
        uintptr_t replacement;
        bool dryRun;
        int hits;
        uintptr_t slots[MAX_SLOTS];
    };

    bool scan_segment(const tools::image::Segment &segment, void *user)
    {
        ScanState *state = (ScanState *)user;
        if (state == nullptr) return true;

        if (segment.initprot & VM_PROT_EXECUTE) return true;
        if (((segment.initprot | segment.maxprot) & VM_PROT_WRITE) == 0) return true;

        uintptr_t cursor = segment.start;

        while ((cursor + sizeof(uintptr_t)) <= segment.end)
        {
            size_t want = SCAN_CHUNK;
            if ((cursor + want) > segment.end) want = (size_t)(segment.end - cursor);
            if (want < sizeof(uintptr_t)) break;

            if (!tools::memory::read(cursor, g_scanBuffer, want))
            {
                cursor += 0x1000;
                continue;
            }

            for (size_t offset = 0; offset + sizeof(uintptr_t) <= want; offset += sizeof(uintptr_t))
            {
                const uintptr_t value = g_scanBuffer[offset / sizeof(uintptr_t)];
                if (!slot_matches(value, state->needle)) continue;

                const uintptr_t slot = cursor + offset;

                if (state->dryRun)
                {
                    if (state->hits < MAX_SLOTS) state->slots[state->hits] = slot;
                    state->hits++;
                    continue;
                }

                if (state->hits >= MAX_SLOTS) continue;

                if (!tools::memory::write(slot, &state->replacement, sizeof(state->replacement))) continue;

                uintptr_t check = 0;
                if (!tools::memory::read(slot, &check, sizeof(check))) continue;
                if (check != state->replacement) continue;

                state->slots[state->hits] = slot;
                state->hits++;
            }

            cursor += want;
        }

        return true;
    }

    int scan_image(uintptr_t imageBase, uintptr_t needle, uintptr_t replacement, bool dryRun,
                   uintptr_t *slots, int capacity)
    {
        ScanState state;
        memset(&state, 0, sizeof(state));
        state.needle = needle;
        state.replacement = replacement;
        state.dryRun = dryRun;

        tools::image::enumerate_segments(imageBase, scan_segment, &state);

        int stored = state.hits < capacity ? state.hits : capacity;
        for (int i = 0; i < stored; i++) slots[i] = state.slots[i];

        return state.hits;
    }
}

namespace tools::hook
{
    void set_error(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        vsnprintf(g_error, sizeof(g_error), format, args);
        va_end(args);
    }

    const char *last_error()
    {
        return g_error;
    }

    void *install(uintptr_t imageBase, uintptr_t rva, void *replacement)
    {
        if (imageBase == 0 || rva == 0 || replacement == nullptr) return nullptr;

        const uintptr_t target = imageBase + rva;

        uintptr_t found[MAX_SLOTS];
        const int available = scan_image(imageBase, target, 0, true, found, MAX_SLOTS);

        if (available <= 0)
        {
            set_error("no pointer slot references %p", (void *)target);
            return nullptr;
        }

        if (available > MAX_SLOTS)
        {
            set_error("%d slots reference %p, refusing", available, (void *)target);
            return nullptr;
        }

        if (!prologue_ok(target))
        {
            set_error("%p does not look like a function entry", (void *)target);
            return nullptr;
        }

        Entry *entry = entry_alloc(target);
        if (entry == nullptr)
        {
            set_error("hook table exhausted");
            return nullptr;
        }

        const int written = scan_image(imageBase, target, (uintptr_t)replacement, false, found, MAX_SLOTS);
        if (written <= 0)
        {
            set_error("no slot accepted the replacement for %p", (void *)target);
            return nullptr;
        }

        entry->replacement = (uintptr_t)replacement;

        for (int i = 0; i < written && i < MAX_SLOTS && entry->count < MAX_SLOTS; i++)
        {
            entry->slots[entry->count] = found[i];
            entry->count++;
        }

        tools::memory::close_windows();

        return (void *)target;
    }

    int uninstall()
    {
        int removed = 0;

        for (int i = 0; i < MAX_HOOKS; i++)
        {
            if (!g_hooks[i].used) continue;

            const uintptr_t restore = g_hooks[i].target;

            for (int s = 0; s < g_hooks[i].count; s++)
            {
                tools::memory::write(g_hooks[i].slots[s], &restore, sizeof(restore));
            }

            memset(&g_hooks[i], 0, sizeof(g_hooks[i]));
            removed++;
        }

        tools::memory::close_windows();

        return removed;
    }
}
