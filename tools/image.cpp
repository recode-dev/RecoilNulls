#include "image.hpp"

#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <string.h>

namespace
{
    constexpr uint32_t MAX_IMAGES = 8192;

    const struct mach_header_64 *header_of(uintptr_t base)
    {
        if (base == 0) return nullptr;

        const struct mach_header_64 *header = (const struct mach_header_64 *)base;
        if (header->magic != MH_MAGIC_64) return nullptr;
        if (header->ncmds == 0 || header->ncmds > 4096) return nullptr;
        if (header->sizeofcmds == 0 || header->sizeofcmds > (4u * 1024u * 1024u)) return nullptr;

        return header;
    }

    // Walks LC_SEGMENT_64 commands. `visit` gets in-memory start/end plus the
    // raw segment, and may stop the walk by returning false.
    template <typename Fn>
    bool walk_segments(uintptr_t base, Fn visit)
    {
        const struct mach_header_64 *header = header_of(base);
        if (header == nullptr) return false;

        const uint8_t *cursor = (const uint8_t *)(header + 1);
        const uint8_t *limit = cursor + header->sizeofcmds;

        // Prefer the dyld-reported base over manual slide math: it is correct
        // for both the main executable and every loaded dylib.
        uintptr_t textVmaddr = 0;
        bool haveText = false;

        for (uint32_t c = 0; c < header->ncmds; c++)
        {
            if (cursor + sizeof(struct load_command) > limit) break;

            const struct load_command *cmd = (const struct load_command *)cursor;
            if (cmd->cmdsize < sizeof(struct load_command)) break;
            if (cursor + cmd->cmdsize > limit) break;

            if (cmd->cmd == LC_SEGMENT_64 && cmd->cmdsize >= sizeof(struct segment_command_64))
            {
                const struct segment_command_64 *seg = (const struct segment_command_64 *)cmd;
                if (strcmp(seg->segname, "__TEXT") == 0)
                {
                    textVmaddr = (uintptr_t)seg->vmaddr;
                    haveText = true;
                    break;
                }
            }

            cursor += cmd->cmdsize;
        }

        if (!haveText) return false;

        const uintptr_t slideAddress = base - textVmaddr;

        cursor = (const uint8_t *)(header + 1);
        for (uint32_t c = 0; c < header->ncmds; c++)
        {
            if (cursor + sizeof(struct load_command) > limit) break;

            const struct load_command *cmd = (const struct load_command *)cursor;
            if (cmd->cmdsize < sizeof(struct load_command)) break;
            if (cursor + cmd->cmdsize > limit) break;

            if (cmd->cmd == LC_SEGMENT_64 && cmd->cmdsize >= sizeof(struct segment_command_64))
            {
                const struct segment_command_64 *seg = (const struct segment_command_64 *)cmd;
                const uintptr_t start = (uintptr_t)seg->vmaddr + slideAddress;
                const uintptr_t end = start + (uintptr_t)seg->vmsize;

                if (!visit(seg, start, end)) return true;
            }

            cursor += cmd->cmdsize;
        }

        return true;
    }

    bool contains_nocase(const char *haystack, const char *needle)
    {
        if (haystack == nullptr || needle == nullptr || *needle == 0) return false;

        for (const char *p = haystack; *p != 0; p++)
        {
            const char *a = p;
            const char *b = needle;

            while (*a != 0 && *b != 0)
            {
                char ca = *a;
                char cb = *b;
                if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
                if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
                if (ca != cb) break;
                a++;
                b++;
            }

            if (*b == 0) return true;
        }

        return false;
    }
}

namespace tools::image
{
    void enumerate(Visitor visitor, void *user)
    {
        if (visitor == nullptr) return;

        uint32_t count = _dyld_image_count();
        if (count > MAX_IMAGES) count = MAX_IMAGES;

        for (uint32_t i = 0; i < count; i++)
        {
            const struct mach_header_64 *header = _dyld_get_image_header(i);
            if (header == nullptr) continue;

            const char *path = _dyld_get_image_name(i);

            Module module;
            module.base = (uintptr_t)header;
            module.size = 0;
            module.path = path != nullptr ? path : "";

            uintptr_t start = 0;
            uintptr_t end = 0;
            if (tools::image::range_for(module.base, start, end)) module.size = end - start;

            if (!visitor(module, user)) return;
        }
    }

    bool enumerate_segments(uintptr_t base, SegmentVisitor visitor, void *user)
    {
        if (visitor == nullptr) return false;

        return walk_segments(base, [&](const struct segment_command_64 *seg, uintptr_t start, uintptr_t end) -> bool
        {
            Segment out;
            out.name = seg->segname;
            out.start = start;
            out.end = end;
            out.initprot = (int)seg->initprot;
            out.maxprot = (int)seg->maxprot;

            return visitor(out, user);
        });
    }

    uintptr_t base_named(const char *needle)
    {
        if (needle == nullptr || *needle == 0) return 0;

        uint32_t count = _dyld_image_count();
        if (count > MAX_IMAGES) count = MAX_IMAGES;

        for (uint32_t i = 0; i < count; i++)
        {
            const char *path = _dyld_get_image_name(i);
            if (path == nullptr) continue;
            if (!contains_nocase(path, needle)) continue;

            const struct mach_header_64 *header = _dyld_get_image_header(i);
            if (header == nullptr) continue;

            return (uintptr_t)header;
        }

        return 0;
    }

    uintptr_t base_for(uintptr_t address)
    {
        if (address == 0) return 0;

        uint32_t count = _dyld_image_count();
        if (count > MAX_IMAGES) count = MAX_IMAGES;

        for (uint32_t i = 0; i < count; i++)
        {
            const struct mach_header_64 *header = _dyld_get_image_header(i);
            if (header == nullptr) continue;

            const uintptr_t base = (uintptr_t)header;
            if (tools::image::owns(base, address)) return base;
        }

        return 0;
    }

    uintptr_t slide_for(uintptr_t base)
    {
        const struct mach_header_64 *header = header_of(base);
        if (header == nullptr) return 0;

        uintptr_t textVmaddr = 0;
        bool haveText = false;

        walk_segments(base, [&](const struct segment_command_64 *seg, uintptr_t, uintptr_t)
        {
            if (strcmp(seg->segname, "__TEXT") != 0) return true;
            textVmaddr = (uintptr_t)seg->vmaddr;
            haveText = true;
            return false;
        });

        if (!haveText) return 0;
        return base - textVmaddr;
    }

    bool range_for(uintptr_t base, uintptr_t &start, uintptr_t &end)
    {
        uintptr_t low = 0;
        uintptr_t high = 0;

        const bool walked = walk_segments(base, [&](const struct segment_command_64 *, uintptr_t s, uintptr_t e)
        {
            if (e <= s) return true;
            if (low == 0 || s < low) low = s;
            if (e > high) high = e;
            return true;
        });

        if (!walked || low == 0 || high <= low) return false;

        start = low;
        end = high;
        return true;
    }

    bool owns(uintptr_t base, uintptr_t address)
    {
        if (base == 0 || address == 0) return false;

        bool inside = false;

        walk_segments(base, [&](const struct segment_command_64 *, uintptr_t s, uintptr_t e)
        {
            if (address >= s && address < e)
            {
                inside = true;
                return false;
            }
            return true;
        });

        return inside;
    }

    bool in_image(uintptr_t address)
    {
        return tools::image::base_for(address) != 0;
    }
}
