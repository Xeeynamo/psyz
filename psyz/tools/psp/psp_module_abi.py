#!/usr/bin/env python3
"""psp_module_abi.py - generate the PSP ABI for a PsyZ module.

Usage:
  derive-exports --module-objs <obj...> --host-objs <obj|archive...>
                  --modules NAME --library NAME --out-dir DIR [--syslib]
      Emits, under DIR:
        <library>.exp       - psp-build-exports source (host/export side)
        <module>_imports.S  - import assembly for each module
        <library>_exports.c - host export table
        <library>_resolve.c - host variable resolver
        <module>_nids.json  - NIDs and symbol types for fixup-vars

  fixup-vars <module.elf> --nids <module_nids.json> [--library NAME]
      Post-link pass to relink unreferenced nids
"""
import argparse
import json
import os
import re
import subprocess
import sys

R_MIPS_32 = 2
R_MIPS_HI16 = 5
R_MIPS_LO16 = 6
RELOC_TYPE_CODE = {"R_MIPS_32": R_MIPS_32, "R_MIPS_HI16": R_MIPS_HI16,
                   "R_MIPS_LO16": R_MIPS_LO16}


def pspdev_root():
    root = os.environ.get("PSPDEV")
    if root:
        return root
    home_candidate = os.path.join(os.path.expanduser("~"), "pspdev")
    if os.path.isdir(home_candidate):
        return home_candidate
    return "/usr/local/pspdev"


def tool(name):
    return os.path.join(pspdev_root(), "bin", name)


def run(*args, **kw):
    kw.setdefault("check", True)
    kw.setdefault("capture_output", True)
    kw.setdefault("text", True)
    return subprocess.run(list(args), **kw)


# nm type letters:
#   T text (function)
#   W weak function
#   D initialised data
#   B bss
#   R rodata
#   G/S small-data
#   V weak object
NM_FUNC_TYPES = set("TW")
NM_VAR_TYPES = set("DBRGSVC")

# Undefined symbols that are never part of a host ABI
#   _gp/_ftext/__executable_start  linker-provided
#   __lib_ent_*/__lib_stub_*       PRX export/import from the linker script
#   sce*                           PSP SDK stubs, resolved by the loader
#   g_sceVarFixupTable             emitted by the fixed-point relink itself
LINKER_PROVIDED = {
    "_gp", "_ftext", "__executable_start",
    "__lib_ent_top", "__lib_ent_bottom",
    "__lib_stub_top", "__lib_stub_bottom",
    "g_sceVarFixupTable",
}

# Glue stuff; these are symbols that the HOST must not resolve
MODULE_INTERNAL = {
    "Psyz_ModuleStart", "Psyz_ModuleStop",
    "host_resolve_var",
}


def _nm_symbols(nm, paths, flag):
    found = {}
    for path in paths:
        proc = run(nm, "--extern-only", flag, path, check=False)
        if proc.returncode != 0:
            sys.exit(f"{os.path.basename(nm)} failed on {path}:\n"
                     f"{proc.stderr.strip()}")
        for line in proc.stdout.splitlines():
            parts = line.split()
            # "        U name"  or  "0000000c T name"; archive member headers
            # and blank lines have no type letter and are skipped.
            if len(parts) == 2:
                letter, name = parts[0], parts[1]
            elif len(parts) == 3:
                letter, name = parts[1], parts[2]
            else:
                continue
            if len(letter) != 1:
                continue
            found.setdefault(name, letter)
    return found


def undefined_symbols(nm, objs):
    # 'U' is a real undefined reference
    # 'w'/'v' are weak undefined
    undef = _nm_symbols(nm, objs, "--undefined-only")
    needed = {n for n, letter in undef.items() if letter == "U"}
    return needed - set(_nm_symbols(nm, objs, "--defined-only"))


def host_symbols(nm, paths):
    return _nm_symbols(nm, paths, "--defined-only")


def derive_entries(nm, objects, provided, library):
    needed = undefined_symbols(nm, objects)
    entries = []
    unresolved = []
    for name in sorted(needed):
        letter = provided.get(name)
        if letter is None:
            if name in LINKER_PROVIDED or name in MODULE_INTERNAL:
                continue
            if name.startswith("sce") or name.startswith("__lib_"):
                continue
            unresolved.append(name)
            continue
        if letter in NM_FUNC_TYPES:
            entries.append(("func", name))
        elif letter in NM_VAR_TYPES:
            entries.append(("var", name))
        else:
            unresolved.append(f"{name} (unclassifiable nm type '{letter}')")

    if unresolved:
        sys.exit(
            f"psp_module_abi.py: module imports {len(unresolved)} symbol(s) "
            f"not provided by host library '{library}':\n" +
            "".join(f"    {n}\n" for n in unresolved) +
            "\nModules link -nostdlib, so these cannot come from libc. "
            "Either define them in the host (they are exported "
            "automatically) or remove the dependency.")

    entries.sort(key=lambda e: (e[0] != "func", e[1]))
    return entries


def write_exp_file(entries, library, out_path, syslib=False):
    lines = ["PSP_BEGIN_EXPORTS", ""]
    if syslib:
        lines += ["PSP_EXPORT_START(syslib, 0, 0x8000)",
                  "PSP_EXPORT_FUNC_HASH(module_start)",
                  "PSP_EXPORT_VAR_HASH(module_info)",
                  "PSP_EXPORT_END", ""]
    lines += [f"PSP_EXPORT_START({library}, 0, 0x0001)"]
    for kind, name in entries:
        lines.append(f"PSP_EXPORT_FUNC_HASH({name})")
    lines += ["PSP_EXPORT_END", "", "PSP_END_EXPORTS", ""]
    with open(out_path, "w") as f:
        f.write("\n".join(lines))


def nids_from_exports_c(exports_c_text, library):
    # Matches e.g.:
    #   extern int host_log;
    #   ...
    #   static const unsigned int __HostLib_exports[..] ... = {
    #       0x47CD3253,
    #       0xFB362626,
    #       (unsigned int) &host_log,
    #       (unsigned int) &g_hostCounter,
    #   };
    m = re.search(
        r"__%s_exports\[\d+\][^=]*=\s*\{([^}]*)\}" % re.escape(library),
        exports_c_text, re.S)
    if not m:
        sys.exit(f"psp-build-exports output has no {library} export table; "
                  f"is the library name correct?")
    body = m.group(1)
    nid_values = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", body)]
    name_refs = re.findall(r"&(\w+)", body)
    if len(nid_values) != len(name_refs):
        sys.exit("could not pair NIDs with export names in psp-build-exports "
                  "output (unexpected format)")
    return dict(zip(name_refs, nid_values))


def cmd_derive_exports(args):
    nm = args.nm or tool("psp-nm")
    groups = []
    if args.module_objs:
        groups.append(list(args.module_objs))
    if args.module_objs_file:
        current = []
        with open(args.module_objs_file) as f:
            for raw in f:
                line = raw.strip()
                if line:
                    current.append(line)
                elif current:
                    groups.append(current)
                    current = []
        if current:
            groups.append(current)
    if not groups:
        sys.exit("derive-exports: no module objects given")
    if len(args.modules) != len(groups):
        sys.exit("derive-exports: expected one module name per object group")
    provided = host_symbols(nm, args.host_objs)
    module_entries = [derive_entries(nm, group, provided, args.library)
                      for group in groups]
    for entries in module_entries:
        entries.append(("func", "host_resolve_var"))
    entries = sorted(set(entry for group in module_entries for entry in group),
                     key=lambda e: (e[0] != "func", e[1]))

    print(f"psp_module_abi.py: {args.library} ABI: "
          f"{sum(1 for k, _ in entries if k == 'func')} function(s), "
          f"{sum(1 for k, _ in entries if k == 'var')} variable(s)")
    os.makedirs(args.out_dir, exist_ok=True)

    exp_path = os.path.join(args.out_dir, f"{args.library}.exp")
    write_exp_file(entries, args.library, exp_path, args.syslib)

    exports_c = run(tool("psp-build-exports"), "-b", exp_path).stdout
    with open(os.path.join(args.out_dir, f"{args.library}_exports.c"), "w") as f:
        f.write(exports_c)
    nid_of = nids_from_exports_c(exports_c, args.library)

    nids = {name: {"nid": nid_of[name], "kind": kind}
            for kind, name in entries}
    for module, imports in zip(args.modules, module_entries):
        write_imports_s(imports, args.library, nids,
                        os.path.join(args.out_dir, f"{module}_imports.S"))
        with open(os.path.join(args.out_dir, f"{module}_nids.json"), "w") as f:
            json.dump({name: nids[name] for _, name in imports}, f,
                      indent=2, sort_keys=True)

    resolve_c = os.path.join(args.out_dir, f"{args.library}_resolve.c")
    write_resolve_c(entries, args.library, nids, resolve_c)

    print(f"wrote {exp_path}")
    print(f"wrote {resolve_c}")


def write_imports_s(entries, library, nids, out_path):
    lines = [
        "/* Generated by psp_module_abi.py derive-exports -- do not edit. */",
        '#include "pspimport.s"',
        '#include "psyz_import_var.s"',
        "",
        # attribute<<16 | version
        # attribute=9 is required by sceKernelStartModule
        f"\tIMPORT_START {library}, 0x00090000",
    ]
    for kind, name in entries:
        nid = nids[name]["nid"]
        if kind == "func":
            lines.append(f"\tIMPORT_FUNC  {library}, {nid:#010x}, {name}")
        else:
            lines.append(f"\tIMPORT_VAR   {library}, {nid:#010x}, {name}")
    lines.append("")
    with open(out_path, "w") as f:
        f.write("\n".join(lines))


def write_resolve_c(entries, library, nids, out_path):
    lines = [
        "/* Generated by psp_module_abi.py derive-exports -- do not edit. */",
        "#include <pspkernel.h>",
        "",
    ]
    var_entries = [(kind, name) for kind, name in entries if kind == "var"]
    for _kind, name in var_entries:
        lines.append(f"extern char {name}[];")
    lines += ["", "unsigned int host_resolve_var(unsigned int nid) {",
              "    switch (nid) {"]
    for _kind, name in var_entries:
        nid = nids[name]["nid"]
        lines.append(f"    case {nid:#010x}u: return (unsigned int){name};")
    lines += ["    default: return 0;", "    }", "}", ""]
    with open(out_path, "w") as f:
        f.write("\n".join(lines))


def section_index_map(elf):
    out = run(tool("psp-readelf"), "-SW", elf).stdout
    idx = {}
    for line in out.splitlines():
        m = re.match(
            r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+"
            r"([0-9a-f]+)\s+([0-9a-f]+)\s+(\S*)",
            line)
        if m:
            n, name, _typ, addr, off, size, _es, flags = m.groups()
            idx[name] = {"index": int(n), "addr": int(addr, 16),
                         "off": int(off, 16), "size": int(size, 16),
                         "alloc": "A" in flags, "nobits": _typ == "NOBITS"}
    return idx


def relocations(elf):
    out = run(tool("psp-readelf"), "-rW", elf).stdout
    cur_section = None
    results = []
    for line in out.splitlines():
        m = re.match(r"Relocation section '(\.\S+)'", line)
        if m:
            cur_section = m.group(1)
            continue
        m = re.match(
            r"^([0-9a-f]{8})\s+[0-9a-f]{8}\s+(R_MIPS_\S+)\s+([0-9a-f]+)\s+(\S+)",
            line)
        if m and cur_section:
            off, typ, symval, sym = m.groups()
            results.append((cur_section, int(off, 16), typ, sym, int(symval, 16)))
    return results


def read_word(elf, secs, offset):
    best = None
    for name, s in secs.items():
        if s["size"] == 0 or s.get("nobits", False):
            continue
        if not s["alloc"]:
            # Debug/.comment/.reginfo/etc sections report addr=0 too, just
            # like a real allocated section based at 0 would; only SHF_ALLOC
            # sections have runtime addresses that "offset" can mean.
            continue
        if s["addr"] <= offset and offset + 4 <= s["addr"] + s["size"]:
            if best is None or s["size"] < best[1]["size"]:
                best = (name, s)
    if best is None:
        return None
    _name, s = best
    fileoff = s["off"] + (offset - s["addr"])
    with open(elf, "rb") as f:
        f.seek(fileoff)
        data = f.read(4)
    if len(data) != 4:
        return None
    return int.from_bytes(data, "little")


def pair_relocs(elf, secs, entries_raw):
    out = []
    by_sym = {}
    for (nid, sym, symval, target, off, typ) in entries_raw:
        by_sym.setdefault((nid, sym, symval, target), []).append((off, typ))

    for (nid, sym, symval, target), sites in by_sym.items():
        pending_hi = []
        for (off, typ) in sites:
            word = None
            if target in secs and off % 4 == 0:
                word = read_word(elf, {target: secs[target]}, off)
            if word is None:
                sys.exit(f"fixup-vars: invalid site {sym} {target}+{off:#x}")
            imm = word & 0xFFFF
            if typ == "R_MIPS_HI16":
                pending_hi.append((off, imm))
            elif typ == "R_MIPS_LO16":
                lo = imm - 0x10000 if imm & 0x8000 else imm
                addend = lo - symval
                if pending_hi:
                    for (hoff, himm) in pending_hi:
                        addend = ((himm << 16) + lo) - symval
                        out.append((nid, hoff, "R_MIPS_HI16", addend))
                    pending_hi = []
                out.append((nid, off, "R_MIPS_LO16", addend))
            elif typ == "R_MIPS_32":
                addend = word - symval
                out.append((nid, off, "R_MIPS_32", addend))
            else:
                sys.exit(f"fixup-vars: unsupported relocation {typ} for {sym}")
        if pending_hi:
            sys.exit(f"fixup-vars: unmatched HI16 for {sym} in {target}")
    out.sort(key=lambda e: e[1])
    return out


def cmd_fixup_vars(args):
    with open(args.nids) as f:
        nids = json.load(f)
    varmap = {name: info["nid"] for name, info in nids.items()
              if info["kind"] == "var"}
    if not varmap:
        write_empty_table(args.elf + ".varfixup.S")
        return

    secs = section_index_map(args.elf)
    relocs = relocations(args.elf)

    entries_raw = []
    for relsec, off, typ, sym, symval in relocs:
        if sym not in varmap:
            continue
        target = relsec[len(".rel"):]
        if target not in secs:
            sys.exit(f"fixup-vars: missing relocation target {target}")
        if not secs[target]["alloc"]:
            continue
        if typ not in RELOC_TYPE_CODE:
            sys.exit(
                f"fixup-vars: unsupported relocation {typ} against variable "
                f"import '{sym}' at {relsec}+{off:#x}.")
        entries_raw.append((varmap[sym], sym, symval, target, off, typ))

    paired = pair_relocs(args.elf, secs, entries_raw)
    entries = [(nid, off, RELOC_TYPE_CODE[typ], addend)
               for (nid, off, typ, addend) in paired]

    for nid, off, tc, addend in entries:
        tn = {v: k for k, v in RELOC_TYPE_CODE.items()}[tc]
        print(f"  nid={nid:#010x} offset={off:#06x} type={tn} addend={addend:#x}")

    out_s = args.elf + ".varfixup.S"
    lines = [
        "/* Generated by psp_module_abi.py fixup-vars -- do not edit. */",
        '\t.section .rodata.sceVarFixup, "a", @progbits',
        "\t.global g_sceVarFixupTable",
        "g_sceVarFixupTable:",
    ]
    for nid, off, tc, addend in entries:
        lines.append(
            f"\t.word 0x{nid:08x}, 0x{addend & 0xffffffff:08x}, 0x{off:08x}, {tc}")
    lines.append("\t.word 0, 0, 0, 0 /* terminator */")
    lines.append("")
    with open(out_s, "w") as f:
        f.write("\n".join(lines))
    print(f"wrote {len(entries)} entries to {out_s}")


def write_empty_table(out_s):
    with open(out_s, "w") as f:
        f.write(
            "/* Generated by psp_module_abi.py fixup-vars -- do not edit. */\n"
            '\t.section .rodata.sceVarFixup, "a", @progbits\n'
            "\t.global g_sceVarFixupTable\n"
            "g_sceVarFixupTable:\n"
            "\t.word 0, 0, 0, 0 /* terminator */\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    de = sub.add_parser("derive-exports")
    de.add_argument("--module-objs", nargs="+",
                    help="every object file the module is built from")
    de.add_argument("--module-objs-file",
                    help="file listing module objects, one per line "
                         "(combined with --module-objs)")
    de.add_argument("--modules", nargs="+", required=True,
                    help="module names in object-group order")
    de.add_argument("--host-objs", nargs="+", required=True,
                    help="the host's object files and static archives")
    de.add_argument("--nm", help="nm to use (default: $PSPDEV/bin/psp-nm)")
    de.add_argument("--library", required=True)
    de.add_argument("--out-dir", required=True)
    de.add_argument("--syslib", action="store_true",
                    help="host is built as a PRX; emit the syslib block")
    de.set_defaults(func=cmd_derive_exports)

    fv = sub.add_parser("fixup-vars")
    fv.add_argument("elf")
    fv.add_argument("--nids", required=True)
    fv.add_argument("--library")
    fv.set_defaults(func=cmd_fixup_vars)

    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
