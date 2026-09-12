#!/usr/bin/env python3
"""psp_link_module.py - link a set of object files into a loadable PSP PRX
module, including the imported symbols from HOST.

Usage:
    psp_link_module.py --cc <psp-gcc> --pspdev <PSPDEV root>
                        --out-elf <path> --out-prx <path>
                        [--nids <nids.json> --library <name>]
                        -- <object files...>
"""
import argparse
import os
import subprocess
import sys

from psp_module_abi import write_empty_table

HERE = os.path.dirname(os.path.abspath(__file__))
MODULE_ABI = os.path.join(HERE, "psp_module_abi.py")


def run(cmd):
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit(
            f"command failed ({proc.returncode}): {' '.join(cmd)}\n"
            f"--- stdout ---\n{proc.stdout}\n--- stderr ---\n{proc.stderr}")
    return proc.stdout


# Explicitly export PRX symbols required from the kernel module loader
MODULE_SYSLIB_EXP = """PSP_BEGIN_EXPORTS

PSP_EXPORT_START(syslib, 0, 0x8000)
PSP_EXPORT_FUNC_HASH(module_start)
PSP_EXPORT_FUNC_HASH(module_stop)
PSP_EXPORT_VAR_HASH(module_info)
PSP_EXPORT_END

PSP_END_EXPORTS
"""


def build_syslib_exports(cc, pspdev, out_elf):
    """Compile the module's syslib export table, returning its object path."""
    exp_path = out_elf + ".syslib.exp"
    c_path = out_elf + ".syslib.c"
    o_path = out_elf + "_syslib.o"
    with open(exp_path, "w") as f:
        f.write(MODULE_SYSLIB_EXP)
    exports_c = run([os.path.join(pspdev, "bin", "psp-build-exports"),
                     "-b", exp_path])
    with open(c_path, "w") as f:
        f.write(exports_c)
    run([cc, "-nostdlib", "-G0", "-I",
         os.path.join(pspdev, "psp", "sdk", "include"),
         "-c", c_path, "-o", o_path])
    return o_path


def link(cc, pspdev, objects, extra_objects, out_elf):
    specs = os.path.join(pspdev, "psp", "sdk", "lib", "prxspecs")
    libdir = os.path.join(pspdev, "psp", "sdk", "lib")
    linkfile = os.path.join(libdir, "linkfile.prx")
    cmd = [cc, "-nostdlib", "-nostartfiles", "-G0",
           f"-specs={specs}", f"-Wl,-q,-T{linkfile}",
           "-Wl,-zmax-page-size=128"]
    cmd += objects + extra_objects
    # -lpspuser: the module glue's variable-import fixup calls
    # sceKernelDcacheWritebackRange/sceKernelIcacheInvalidateRange. These are
    # UtilsForUser stubs, resolved by the loader against a system library, so
    # they stay compatible with the -nostdlib link.
    cmd += ["-L" + libdir, "-lpspmodinfo", "-lpspuser", "-o", out_elf]
    run(cmd)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cc", required=True)
    ap.add_argument("--pspdev", required=True)
    ap.add_argument("--out-elf", required=True)
    ap.add_argument("--out-prx", required=True)
    ap.add_argument("--nids")
    ap.add_argument("--library")
    ap.add_argument("--max-passes", type=int, default=6)
    ap.add_argument("objects", nargs="+")
    args = ap.parse_args()
    if args.objects and args.objects[0] == "--":
        args.objects = args.objects[1:]

    out_dir = os.path.dirname(os.path.abspath(args.out_elf))
    os.makedirs(out_dir, exist_ok=True)

    syslib_o = build_syslib_exports(args.cc, args.pspdev, args.out_elf)

    if not args.nids:
        link(args.cc, args.pspdev, args.objects, [syslib_o], args.out_elf)
    else:
        varfixup_s = args.out_elf + ".varfixup.S"
        varfixup_o = args.out_elf + "_varfixup.o"
        # Pass 0: an empty table, so the module has one to iterate on.
        write_empty_table(varfixup_s)
        run([args.cc, "-G0", "-c", varfixup_s, "-o", varfixup_o])

        converged = False
        for pass_n in range(1, args.max_passes + 1):
            with open(varfixup_s, "rb") as f:
                previous = f.read()
            link(args.cc, args.pspdev, args.objects,
                 [varfixup_o, syslib_o], args.out_elf)
            run([sys.executable, MODULE_ABI, "fixup-vars", args.out_elf,
                 "--nids", args.nids, "--library", args.library])
            with open(varfixup_s, "rb") as f:
                current = f.read()
            if current == previous:
                print(f"psp_link_module.py: variable-import fixup converged "
                      f"at pass {pass_n}")
                converged = True
                break
            run([args.cc, "-G0", "-c", varfixup_s, "-o", varfixup_o])
        if not converged:
            sys.exit(f"psp_link_module.py: variable-import fixup did not "
                      f"converge within {args.max_passes} passes")
        # The unchanged table is already linked into this ELF.

    fixup_imports = os.path.join(args.pspdev, "bin", "psp-fixup-imports")
    prxgen = os.path.join(args.pspdev, "bin", "psp-prxgen")
    run([fixup_imports, args.out_elf])
    run([prxgen, args.out_elf, args.out_prx])


if __name__ == "__main__":
    main()
