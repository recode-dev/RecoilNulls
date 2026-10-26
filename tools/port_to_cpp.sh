#!/usr/bin/env bash
#
# port_to_cpp.sh - converts the Recoil tweak sources from Objective-C++ (.mm)
# to plain C++ (.cpp).
#
# The tree only ever used three Objective-C constructs:
#
#   1. #import <Foundation/Foundation.h>        -> dropped, C headers cover it
#   2. NSSearchPathForDirectoriesInDomains()    -> getenv("HOME")/Documents
#   3. __unsafe_unretained Class                -> plain Class (no ARC in C++)
#
# Everything else (Mach VM, dyld, dispatch, block literals, the ObjC runtime
# C API used by tools/objc.mm) is plain C or a Clang extension and compiles
# unchanged as C++ - the Makefile adds -fblocks so the dispatch blocks stay.
#
# Usage:
#   tools/port_to_cpp.sh --dry-run   # report what would change
#   tools/port_to_cpp.sh             # perform the port
#
# The script is idempotent: re-running it on an already ported tree is a no-op.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

DRY_RUN=0
for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=1 ;;
        -h|--help) sed -n '2,19p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown argument: $arg" >&2; exit 2 ;;
    esac
done

echo "== Recoil: Objective-C++ -> C++ port =="
echo "root: $ROOT"

# ---------------------------------------------------------------------------
# 1. Rename every .mm to .cpp (src/ and tools/).
# ---------------------------------------------------------------------------
renames=0
while IFS= read -r -d '' src; do
    dst="${src%.mm}.cpp"
    if [ -e "$dst" ]; then
        echo "  ! both $src and $dst exist, skipping rename" >&2
        continue
    fi
    echo "  rename $src -> $dst"
    renames=$((renames + 1))
    if [ "$DRY_RUN" -eq 0 ]; then
        if git rev-parse --git-dir >/dev/null 2>&1 && git ls-files --error-unmatch "$src" >/dev/null 2>&1; then
            git mv "$src" "$dst"
        else
            mv "$src" "$dst"
        fi
    fi
done < <(find src tools -name '*.mm' -print0 2>/dev/null | sort -z)

# ---------------------------------------------------------------------------
# 2. Rewrite sources: #import -> #include, drop Foundation, replace the two
#    Objective-C call sites, and add the dispatch time-unit fallbacks.
# ---------------------------------------------------------------------------
PORT_DRY_RUN="$DRY_RUN" python3 - <<'PYEOF'
import os
import pathlib
import sys

root = pathlib.Path(".").resolve()
dry = os.environ.get("PORT_DRY_RUN") == "1"

NEW_DOC_FN = """const char *document_path(void)
{
    static char resolved[1024];
    const char *home = getenv("HOME");

    resolved[0] = 0;
    if (home != nullptr)
    {
        snprintf(resolved, sizeof(resolved), "%s/Documents", home);
    }
    if (resolved[0] == 0)
    {
        snprintf(resolved, sizeof(resolved), "%s", "/tmp");
    }

    return resolved;
}"""

NSEC_GUARD = """
/* Dispatch time units are provided by mach/clock_types.h in ObjC builds;
   define them defensively now that Foundation is no longer included. */
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000ull
#endif
#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC 1000000ull
#endif
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC 1000ull
#endif
"""


def rewrite_imports(text):
    out = []
    for line in text.splitlines(True):
        stripped = line.strip()
        if stripped.startswith("#import "):
            line = line.replace("#import ", "#include ", 1)
            stripped = line.strip()
        if stripped in ('#include <Foundation/Foundation.h>', '#include "Foundation/Foundation.h"'):
            continue
        out.append(line)
    return "".join(out)


def rewrite_unsafe(text):
    return text.replace("__unsafe_unretained ", "")


def rewrite_logger(text):
    if "NSSearchPathForDirectoriesInDomains" not in text:
        return text
    marker = "const char *document_path(void)"
    start = text.find(marker)
    if start < 0:
        print("  ! document_path() not found in logger", file=sys.stderr)
        return text
    tail = text.find("return resolved;", start)
    if tail < 0:
        print("  ! return resolved; not found in logger", file=sys.stderr)
        return text
    end = text.find("}", tail)
    if end < 0:
        print("  ! closing brace not found in logger", file=sys.stderr)
        return text
    line_start = text.rfind("\n", 0, start) + 1
    indent = text[line_start:start]
    body = NEW_DOC_FN.split("\n")
    body = [body[0]] + [indent + line if line else line for line in body[1:]]
    return text[:start] + "\n".join(body) + text[end + 1:]


def rewrite_nsec(text):
    if "RECOIL_SCAN_SCAN_H" not in text or "NSEC_PER_SEC" in text:
        return text
    anchor = "#include <stdint.h>"
    pos = text.find(anchor)
    if pos < 0:
        return text
    end = pos + len(anchor)
    return text[:end] + "\n" + NSEC_GUARD + text[end:]


def rewrite_cfdate(text):
    if "RECOIL_SCAN_SCAN_H" not in text or "CFDate.h" in text:
        return text
    anchor = "#include <objc/runtime.h>"
    pos = text.find(anchor)
    if pos < 0:
        return text
    end = pos + len(anchor)
    return text[:end] + "\n\n/* CFAbsoluteTimeGetCurrent lives in CoreFoundation, a C API. */\n#include <CoreFoundation/CFDate.h>" + text[end:]


TRANSFORMS = (rewrite_imports, rewrite_unsafe, rewrite_logger, rewrite_nsec, rewrite_cfdate)

targets = []
for base in ("src", "tools"):
    for pattern in ("*.cpp", "*.mm", "*.h", "*.c", "*.hpp"):
        targets.extend(sorted((root / base).rglob(pattern)))

changed = []
for path in targets:
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        continue
    new = text
    for transform in TRANSFORMS:
        new = transform(new)
    if new != text:
        changed.append(path)
        if not dry:
            path.write_text(new, encoding="utf-8")

for path in changed:
    print("  patch  " + str(path.relative_to(root)))

stale = sorted(list((root / "src").rglob("*.mm")) + list((root / "tools").rglob("*.mm")))
print("  files patched: %d, remaining .mm: %d" % (len(changed), len(stale)))

# ---------------------------------------------------------------------------
# Verify that no Objective-C source is left behind.
# ---------------------------------------------------------------------------
FORBIDDEN = [
    "#import ",
    "NSSearchPathForDirectoriesInDomains",
    "NSArray",
    "NSMutableArray",
    "NSString",
    "NSObject",
    "NSLog",
    "__unsafe_unretained",
    "@selector(",
    "@autoreleasepool",
    "@interface",
    "@implementation",
]
violations = []
for base in ("src", "tools"):
    for path in sorted((root / base).rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h", ".hpp"):
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for needle in FORBIDDEN:
            index = text.find(needle)
            while index >= 0:
                line = text.count("\n", 0, index) + 1
                violations.append(str(path.relative_to(root)) + ":" + str(line) + ": " + needle.strip())
                index = text.find(needle, index + 1)

if violations:
    if dry:
        print("  would still contain Objective-C (fix pending):", file=sys.stderr)
    else:
        print("  ! objective-c leftovers:", file=sys.stderr)
    for item in violations:
        print("    " + item, file=sys.stderr)
    if not dry:
        sys.exit(1)
else:
    print("  objective-c syntax: none")

if not dry and stale:
    print("  ! .mm files survived the rename", file=sys.stderr)
    sys.exit(1)
PYEOF

# ---------------------------------------------------------------------------
# 3. Makefile: build .cpp instead of .mm, keep blocks enabled.
# ---------------------------------------------------------------------------
if [ "$DRY_RUN" -eq 0 ]; then
    cat > Makefile <<'MAKEFILE'
TARGET := iphone:clang:latest:15.0
ARCHS := arm64

include $(THEOS)/makefiles/common.mk

TWEAK_NAME := Recoil

Recoil_FILES := $(shell find src -name "*.cpp" | sort)
Recoil_FILES += $(shell find tools \( -name "*.c" -o -name "*.cpp" \) | sort)

RECOIL_INCLUDE_FLAGS := -Isrc -Itools

Recoil_CFLAGS := -Wall -Wextra $(RECOIL_INCLUDE_FLAGS)
# -fblocks keeps the dispatch block literals compiling in plain C++.
Recoil_CCFLAGS := -std=c++17 -fblocks -Wall -Wextra $(RECOIL_INCLUDE_FLAGS)

# Foundation stays linked only so libobjc (used by tools/objc.cpp) resolves;
# no Objective-C source remains in the tweak.
Recoil_FRAMEWORKS := Foundation CoreFoundation

include $(THEOS_MAKE_PATH)/tweak.mk
MAKEFILE
    echo "  patch  Makefile"
fi

echo
if [ "$DRY_RUN" -eq 0 ]; then
    echo "== port complete: $(find src tools -name '*.cpp' | wc -l | tr -d ' ') C++ files, $(find src tools -name '*.mm' | wc -l | tr -d ' ') Objective-C++ files =="
else
    echo "== dry run: $renames rename(s) pending =="
fi
