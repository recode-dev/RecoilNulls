#pragma once

#include <cstdint>
#include <cstddef>

namespace tools::hook
{
    void *install(uintptr_t imageBase, uintptr_t rva, void *replacement);
    int uninstall();

    void set_error(const char *format, ...);
    const char *last_error();
}
