#!/usr/bin/env python3
"""Port the hand-rolled runtime primitives to the frida-gum 17.x API.

Idempotent: running it twice changes nothing.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def read(path):
    return (ROOT / path).read_text()


def write(path, text):
    (ROOT / path).write_text(text)


def sub(text, old, new, label):
    if new in text:
        return text
    if old not in text:
        raise SystemExit("port: anchor not found -> %s" % label)
    return text.replace(old, new, 1)


def replace_func(text, signature, body, label):
    start = text.find(signature)
    if start < 0:
        raise SystemExit("port: function not found -> %s" % label)
    end = text.find("\n}\n", start)
    if end < 0:
        raise SystemExit("port: function end not found -> %s" % label)
    end += 3
    return text[:start] + body + text[end:]


def port_scan_h():
    text = read("src/core/scan.h")
    if "#include <frida-gum.h>" not in text:
        text = text.replace('#include "offsets.h"',
                            '#include <frida-gum.h>\n#include "offsets.h"', 1)
    if "#include <frida-gum.h>" not in text:
        text = "#include <frida-gum.h>\n" + text
    body = """static inline BOOL addr_readable(uintptr_t address, size_t length)
{
    if (!address || !length)
    {
        return NO;
    }
    return gum_memory_is_readable((gconstpointer)address, (gsize)length) ? YES
                                                                        : NO;
}
"""
    text = replace_func(text, "static inline BOOL addr_readable(uintptr_t address, size_t length)",
                        body, "scan.h addr_readable")
    write("src/core/scan.h", text)


def port_scan_cpp():
    text = read("src/core/scan.cpp")
    if "#include <frida-gum.h>" not in text:
        text = "#include <frida-gum.h>\n" + text

    read_body = """BOOL read_bytes(uintptr_t address, void* out, size_t length)
{
    if (!out || !length)
    {
        return NO;
    }
    if (!address)
    {
        return NO;
    }
    if (!gum_memory_is_readable((gconstpointer)address, (gsize)length))
    {
        return NO;
    }
    memcpy(out, (const void*)address, length);
    return YES;
}
"""
    text = replace_func(text, "BOOL read_bytes(uintptr_t address, void* out, size_t length)",
                        read_body, "read_bytes")

    copy_body = """BOOL copy(uintptr_t source, void* destination, size_t length)
{
    if (!source || !destination || !length)
    {
        return NO;
    }
    if (!gum_memory_is_readable((gconstpointer)source, (gsize)length))
    {
        return NO;
    }
    memcpy(destination, (const void*)source, length);
    return YES;
}
"""
    text = replace_func(text, "BOOL copy(uintptr_t source, void* destination, size_t length)",
                        copy_body, "copy")

    finder = """static gboolean port_module_cb(GumModule* module, gpointer userData)
{
    uintptr_t* out = (uintptr_t*)userData;
    const char* path = NULL;
    const GumMemoryRange* range = NULL;
    if (!module || !out)
    {
        return TRUE;
    }
    path = gum_module_get_name(module);
    range = gum_module_get_range(module);
    if (!path || !range)
    {
        return TRUE;
    }
    if (!strstr(path, ".app/"))
    {
        return TRUE;
    }
    if (strstr(path, "/System/") || strstr(path, "/usr/lib/") ||
        strstr(path, ".framework/") || strstr(path, ".dylib"))
    {
        return TRUE;
    }
    if (!valid_header((uintptr_t)range->base_address))
    {
        return TRUE;
    }
    *out = (uintptr_t)range->base_address;
    return FALSE;
}
BOOL find_game_image(uintptr_t* out_base)
{
    uintptr_t found = 0;
    if (!out_base)
    {
        return NO;
    }
    gum_process_enumerate_modules(port_module_cb, &found);
    if (!found)
    {
        return NO;
    }
    *out_base = found;
    return YES;
}
"""
    text = replace_func(text, "BOOL find_game_image(uintptr_t* out_base)",
                        finder, "find_game_image")
    write("src/core/scan.cpp", text)


def port_index():
    text = read("src/index.cpp")
    if "#include <frida-gum.h>" not in text:
        text = sub(text, '#include "./core/offsets.h"',
                   '#include <frida-gum.h>\n#include "./core/offsets.h"', "index include")
    if "gum_init_embedded();" not in text:
        text = sub(text,
                   "__attribute__((constructor)) void start(void)\n{\n",
                   "__attribute__((constructor)) void start(void)\n{\n    gum_init_embedded();\n",
                   "gum init")
    write("src/index.cpp", text)


def main():
    port_scan_h()
    port_scan_cpp()
    port_index()
    print("port: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
