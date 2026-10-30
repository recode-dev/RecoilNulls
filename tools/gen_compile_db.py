#!/usr/bin/env python3
"""Write compile_commands.json so clang-tidy can parse every TU."""

import json
import os
import subprocess
import sys


def main():
    if len(sys.argv) != 3:
        print("usage: gen_compile_db.py <sdk> <gum-include>", file=sys.stderr)
        return 2
    sdk, gum = sys.argv[1], sys.argv[2]
    files = [
        f
        for f in subprocess.check_output(["git", "ls-files", "src/*.cpp"]).decode().split()
        if f.endswith(".cpp")
    ]
    entries = []
    for name in files:
        command = (
            "clang++ -std=c++17 -fblocks -fobjc-arc -x objective-c++ "
            f"-isysroot {sdk} -target arm64-apple-ios14.0 "
            f"-I{gum} -Isrc -Isrc/core -Isrc/features -Isrc/utils -c {name}"
        )
        entries.append({"directory": os.getcwd(), "file": name, "command": command})
    with open("compile_commands.json", "w") as handle:
        json.dump(entries, handle, indent=1)
    print("tus:", len(entries))
    return 0


if __name__ == "__main__":
    sys.exit(main())
