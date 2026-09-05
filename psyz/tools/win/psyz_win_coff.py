#!/usr/bin/env python3
"""Windows COFF helper for PsyZ loadable modules.

A module (.dll) that reads or writes a global variable owned by its host (.exe)
cannot reference it directly: MSVC exports data through the import address
table only, so the import library provides `__imp_X` and no plain `X`, and the
reference fails to link with LNK2001. The usual fix is `__declspec(dllimport)`
on every shared declaration, which is far too invasive for a ported game.

Instead this tool derives, at build time, which host globals a module actually
uses, and emits a header that rewrites each one into an IAT indirection:

    #define g_SampleCounter (*__imp_g_SampleCounter)

The header is force-included into the module's translation units, so the
developer's own `extern int g_SampleCounter;` becomes a declaration of the IAT
pointer, typed by that very declaration. This tool therefore never needs to
know the type of anything it rewrites.

Only the standard library is used: dumpbin and llvm-nm are not on PATH outside
a Visual Studio developer prompt.
"""

import argparse
import re
import struct
import sys

IMAGE_SYM_UNDEFINED = 0
IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_FILE_MACHINE_I386 = 0x014C

ARCHIVE_MAGIC = b"!<arch>\n"
BIGOBJ_CLSID = bytes.fromhex("c7a1bad1eeeba94baf20faf66aa4dcb8")

# Identifiers that must never be rewritten even if the host happens to export
# them: the generated header is force-included ahead of every system header, so
# a macro on one of these would rewrite declarations owned by libc.
DEFAULT_EXCLUDES = (
    r"^_",
    r"^(errno|environ|stdin|stdout|stderr|timezone|daylight|tzname)$",
    r"^(optarg|optind|opterr|optopt)$",
)


class CoffError(Exception):
    pass


def iter_archive_members(blob, path):
    """Yield (name, body) for every member of a COFF archive."""
    if not blob.startswith(ARCHIVE_MAGIC):
        raise CoffError("%s: not a COFF archive" % path)
    off = len(ARCHIVE_MAGIC)
    while off + 60 <= len(blob):
        header = blob[off:off + 60]
        name = header[0:16].decode("ascii", "replace").rstrip()
        try:
            size = int(header[48:58].decode("ascii").strip())
        except ValueError:
            raise CoffError(
                "%s: malformed member header at offset %d" % (path, off))
        body = blob[off + 60:off + 60 + size]
        yield name, body
        off += 60 + size + (size & 1)


def read_archive_symbol_index(path):
    """Return the set of symbol names defined by an import/static library.

    Only the archive's first linker member is needed. Its layout is a
    big-endian symbol count, that many big-endian member offsets, then the same
    number of NUL-terminated names.
    """
    with open(path, "rb") as handle:
        blob = handle.read()
    for name, body in iter_archive_members(blob, path):
        if name not in ("/", ""):
            raise CoffError(
                "%s: first member is '%s', not the linker member" % (path, name))
        if len(body) < 4:
            raise CoffError("%s: truncated linker member" % path)
        count = struct.unpack(">I", body[:4])[0]
        names = body[4 + 4 * count:].split(b"\x00")
        symbols = {n.decode("ascii", "replace") for n in names if n}
        if len(symbols) < count // 2:
            raise CoffError(
                "%s: linker member declares %d symbols, parsed %d"
                % (path, count, len(symbols)))
        return symbols
    raise CoffError("%s: archive has no members" % path)


def data_exports(import_lib):
    """Names the host exports as data.

    The linker turns an `X DATA` export into an import record that defines
    `__imp_X` alone, while a function export also defines the plain name for
    its call thunk. So a symbol is data exactly when the import library has
    `__imp_X` but no `X`.
    """
    index = read_archive_symbol_index(import_lib)
    imported = {name[len("__imp_"):]
                for name in index if name.startswith("__imp_")}
    return imported - index


def parse_coff_object(blob, path):
    """Return (machine, {undefined external symbol names}) for one COFF object."""
    if len(blob) < 20:
        raise CoffError("%s: truncated object" % path)

    # A bigobj header collides with the short-format import record on its first
    # four bytes; the class id settles it.
    is_bigobj = (blob[0:4] == b"\x00\x00\xff\xff" and len(blob) >= 56 and
                 blob[8:24] == BIGOBJ_CLSID)
    if is_bigobj:
        machine = struct.unpack("<H", blob[6:8])[0]
        sym_ptr, sym_count = struct.unpack("<II", blob[48:56])
        record_size = 20
        section_fmt = "<i"
    else:
        machine, _, _, sym_ptr, sym_count = struct.unpack("<HHIII", blob[:16])
        record_size = 18
        section_fmt = "<h"
    section_size = struct.calcsize(section_fmt)

    if sym_ptr == 0 or sym_count == 0:
        return machine, set()

    strtab = blob[sym_ptr + sym_count * record_size:]
    undefined = set()
    index = 0
    while index < sym_count:
        base = sym_ptr + index * record_size
        record = blob[base:base + record_size]
        if len(record) < record_size:
            raise CoffError("%s: truncated symbol table" % path)
        raw_name = record[0:8]
        if raw_name[0:4] == b"\x00\x00\x00\x00":
            offset = struct.unpack("<I", raw_name[4:8])[0]
            end = strtab.find(b"\x00", offset)
            name = strtab[offset:end if end >= 0 else len(strtab)]
        else:
            name = raw_name.split(b"\x00")[0]
        value = struct.unpack("<I", record[8:12])[0]
        section = struct.unpack(section_fmt, record[12:12 + section_size])[0]
        storage = record[record_size - 2]
        aux = record[record_size - 1]
        # A section number of zero with a non-zero value is a common symbol (a
        # tentative definition), which is defined here rather than imported.
        if (section == IMAGE_SYM_UNDEFINED and value == 0 and
                storage == IMAGE_SYM_CLASS_EXTERNAL):
            undefined.add(name.decode("ascii", "replace"))
        index += 1 + aux
    return machine, undefined


def scan_objects(paths):
    """Union of undefined externals across objects, plus the target machine."""
    machine = None
    undefined = set()
    for path in paths:
        with open(path, "rb") as handle:
            blob = handle.read()
        if blob.startswith(ARCHIVE_MAGIC):
            objects = [body for name, body in iter_archive_members(blob, path)
                       if name not in ("/", "//", "")]
        else:
            objects = [blob]
        for body in objects:
            obj_machine, names = parse_coff_object(body, path)
            if obj_machine and machine is None:
                machine = obj_machine
            undefined |= names
    return machine, undefined


def read_object_list(args):
    paths = list(args.module_obj)
    if args.module_objs_file:
        with open(args.module_objs_file, "r", encoding="utf-8") as handle:
            paths += [line.strip() for line in handle if line.strip()]
    return paths


def write_if_changed(path, text):
    """Avoid touching an unchanged header, which would recompile the module."""
    try:
        with open(path, "r", encoding="utf-8") as handle:
            if handle.read() == text:
                return False
    except OSError:
        pass
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    return True


BANNER = """\
/* Generated by psyz_win_coff.py for module '%s' -- do not edit.
 *
 * Every host global this module references is rewritten into an indirection
 * through the host's import address table, so a plain `extern int g_X;` in a
 * shared header declares the IAT pointer with the developer's own type, and no
 * __declspec(dllimport) is needed anywhere.
 *
 * Limitations:
 *  - `T *p = &g_HostVar;` at file scope does not compile: the address is only
 *    known at load time, so assign it at runtime instead. Explicit dllimport
 *    has the same restriction; other platforms are unaffected.
 *  - Shared declarations must say `extern`. A tentative definition (`int g_X;`)
 *    turns into a second definition of the IAT pointer.
 *  - C++ sources must declare host globals inside `extern "C"`.
 *  - A module-local name matching a rewritten global is rewritten too. Opt out
 *    of a single name with -DPSYZ_NO_AUTOIMPORT_<name>.
 */"""


def gen_autoimport(args):
    machine, undefined = scan_objects(read_object_list(args))
    exports = data_exports(args.import_lib)
    excludes = [re.compile(pattern) for pattern in
                (args.exclude if args.exclude else list(DEFAULT_EXCLUDES))]

    # On x86 every C identifier gains a leading underscore, so the assembly
    # names seen here are one underscore ahead of what the source must say.
    strip = machine == IMAGE_FILE_MACHINE_I386

    def to_c_identifier(asm_name):
        return asm_name[1:] if strip and asm_name.startswith("_") else asm_name

    rewritten = []
    for asm_name in sorted(undefined & exports):
        plain = to_c_identifier(asm_name)
        if any(pattern.search(plain) for pattern in excludes):
            continue
        rewritten.append((plain, to_c_identifier("__imp_" + asm_name)))

    guard = "PSYZ_AUTOIMPORT_%s_H" % re.sub(
        r"[^A-Za-z0-9]", "_", args.module_name).upper()
    lines = [BANNER % args.module_name,
             "#ifndef %s" % guard,
             "#define %s" % guard,
             ""]
    if rewritten:
        for plain, imp in rewritten:
            lines += ["#ifndef PSYZ_NO_AUTOIMPORT_%s" % plain,
                      "#define %s (*%s)" % (plain, imp),
                      "#endif"]
    else:
        lines.append("/* This module references no host globals. */")
    lines += ["", "#endif", ""]

    write_if_changed(args.output, "\n".join(lines))
    if args.verbose:
        print("psyz_win_coff: %s imports %d host global(s): %s"
              % (args.module_name, len(rewritten),
                 ", ".join(name for name, _ in rewritten) or "(none)"))
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    gen = sub.add_parser("gen-autoimport",
                         help="emit the forced-include header for one module")
    gen.add_argument("--module-name", required=True)
    gen.add_argument("--module-obj", action="append", default=[],
                     help="module object file (repeatable)")
    gen.add_argument("--module-objs-file",
                     help="file listing module object files, one per line")
    gen.add_argument("--import-lib", required=True, help="the host's import library")
    gen.add_argument("--output", required=True)
    gen.add_argument("--exclude", action="append",
                     help="regex of names never to rewrite; replaces the defaults")
    gen.add_argument("--verbose", action="store_true")
    gen.set_defaults(func=gen_autoimport)

    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except (CoffError, OSError) as error:
        sys.stderr.write("psyz_win_coff: %s\n" % error)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
