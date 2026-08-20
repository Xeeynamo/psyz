#!/usr/bin/env python3
# Generates compile_commands.json for clangd.
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

LIBS_400 = (
    "libapi libc2 libcd libcard libcomb libds libetc libgpu libgs libgte "
    "libgun libmath libmcrd libpress libsio libsnd libspu libtap"
).split()

FLAGS = [
    "clang", "-target", "mipsel-unknown-linux-gnu",
    "-Iinclude", "-Iinclude/og",
    "-Wno-unknown-warning-option", "-Wno-builtin-declaration-mismatch",
    "-Dmips", "-D__OPTIMIZE__", "-D__mips__", "-Dpsx", "-D__psx__", "-D__psx", "-D_PSYQ",
    "-D__EXTENSIONS__", "-D_MIPSEL", "-D_LANGUAGE_C", "-DLANGUAGE_C",
    "-fsyntax-only",
]


def main():
    srcs = []
    for lib in LIBS_400:
        d = os.path.join(ROOT, "src", lib)
        if os.path.isdir(d):
            for f in sorted(os.listdir(d)):
                if f.endswith(".c"):
                    srcs.append(os.path.join("src", lib, f))

    entries = [
        {"directory": ROOT, "file": s, "arguments": FLAGS + [s]}
        for s in srcs
    ]

    out_path = os.path.join(ROOT, "compile_commands.json")
    with open(out_path, "w") as f:
        json.dump(entries, f, indent=2)


if __name__ == "__main__":
    main()
