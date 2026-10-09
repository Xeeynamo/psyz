#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
TOOLS = ROOT / "build/bunny-tools"
PINS = {
    "psxavenc": ("https://github.com/WonderfulToolchain/psxavenc.git",
                  "82f3871c5fe5e82e71016a6636aba25ddddf2ca8"),
    "mkpsxiso": ("https://github.com/Lameguy64/mkpsxiso.git",
                  "a6b11ea86e67c189137ac50a4066044b3fdb0525"),
}
COMMANDS = []
PSXAVENC_PATCHES = [HERE / name for name in
                   ("psxavenc-flush.patch", "psxavenc-av-free.patch")]


def run(command, cwd=ROOT, log=None, env=None):
    command = list(map(str, command))
    COMMANDS.append({"argv": command, "cwd": str(cwd)})
    print("+", " ".join(command), flush=True)
    if log:
        try:
            with Path(log).open("w") as output:
                subprocess.run(command, cwd=cwd, stdout=output,
                               stderr=subprocess.STDOUT, check=True, env=env)
        except subprocess.CalledProcessError:
            print(Path(log).read_text(), file=sys.stderr)
            raise
    else:
        subprocess.run(command, cwd=cwd, check=True, env=env)


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def prepare_tools(jobs):
    TOOLS.mkdir(parents=True, exist_ok=True)
    for name, (url, revision) in PINS.items():
        directory = TOOLS / name
        if not directory.exists():
            run(["git", "clone", url, directory])
        current = subprocess.check_output(
            ["git", "-C", str(directory), "rev-parse", "HEAD"], text=True).strip()
        if current != revision:
            run(["git", "-C", directory, "checkout", "--detach", revision])
        if name == "mkpsxiso":
            run(["git", "-C", directory, "submodule", "update", "--init", "--recursive"])
            run(["cmake", "-S", directory, "-B", directory / "build",
                 "-DCMAKE_BUILD_TYPE=Release"])
            run(["cmake", "--build", directory / "build", "-j", jobs])
        else:
            for patch in PSXAVENC_PATCHES:
                applied = subprocess.run(
                    ["git", "apply", "--unidiff-zero", "--reverse", "--check", str(patch)],
                    cwd=directory, capture_output=True).returncode == 0
                if not applied:
                    run(["git", "apply", "--unidiff-zero", "--check", patch], cwd=directory)
                    run(["git", "apply", "--unidiff-zero", patch], cwd=directory)
            if not (directory / "build/build.ninja").exists():
                run(["meson", "setup", directory / "build", directory,
                     "--buildtype=release"])
            run(["meson", "compile", "-C", directory / "build", "-j", jobs])


def check_stream(path):
    data = path.read_bytes()
    if len(data) % 2352:
        raise ValueError("strcd must contain whole 2352-byte sectors")
    frames, audio_sectors, video_sectors = {}, 0, 0
    for offset in range(0, len(data), 2352):
        sector = data[offset:offset + 2352]
        if sector[16:20] != sector[20:24]:
            raise ValueError("mismatched XA subheaders")
        if sector[18] & 0x44 == 0x44:
            if sector[16:18] != b"\x01\x01" or sector[19] != 1:
                raise ValueError("expected XA file/channel 1, stereo 37.8kHz 4-bit")
            audio_sectors += 1
            continue
        magic, kind, chunk, count, frame, size, width, height = struct.unpack_from(
            "<HHHHIIHH", sector, 24)
        if (magic, kind, width, height) != (0x160, 0x8001, 320, 192):
            raise ValueError("unexpected STR video header")
        if count >= 32 or size > count * 2016 or chunk >= count:
            raise ValueError("frame exceeds player ring capacity")
        entry = frames.setdefault(frame, {"chunks": [], "sectors": count, "bytes": size})
        if (entry["sectors"], entry["bytes"]) != (count, size):
            raise ValueError("inconsistent frame headers")
        entry["chunks"].append(chunk)
        video_sectors += 1
    if sorted(frames) != list(range(1, 61)) or not audio_sectors:
        raise ValueError("expected 60 video frames with XA audio")
    for entry in frames.values():
        if entry["chunks"] != list(range(entry["sectors"])):
            raise ValueError("missing, repeated or out-of-order video chunk")
    return {"frames": 60, "sectors": len(data) // 2352,
            "xa_sectors": audio_sectors, "video_sectors": video_sectors,
            "raw_bytes": len(data), "max_frame_bytes": max(f["bytes"] for f in frames.values())}


def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--source", type=Path)
    mode.add_argument("--prepare-tools", action="store_true",
                      help="build the pinned encoders/disc tools without building a disc")
    parser.add_argument("--psyq", type=Path, default=ROOT / "nugget/psyq")
    parser.add_argument("--prefix", default="mipsel-none-elf")
    parser.add_argument("--format", default="elf32-littlemips",
                        help="linker output format (nugget FORMAT)")
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if args.prepare_tools:
        prepare_tools(args.jobs)
        return
    args.source = args.source.resolve(); args.psyq = args.psyq.resolve()
    generated = HERE / "generated"; generated.mkdir(exist_ok=True)
    if not args.source.is_file() or not (args.psyq / "lib/libpress.a").is_file():
        parser.error("source MP4 and converted PSYQ include/lib directories are required")
    prepare_tools(args.jobs)
    if not (ROOT / "nugget/common.mk").is_file():
        run(["git", "submodule", "update", "--init", "nugget"])
    clip = generated / "clip.mkv"
    run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-ss", "19",
         "-i", args.source, "-t", "2", "-map", "0:v:0", "-map", "0:a:0",
         "-vf", "fps=30,scale=320:180:flags=lanczos,setsar=1,pad=320:192:0:6:black",
         "-c:v", "ffv1", "-level", "3", "-threads", "1", "-c:a", "pcm_s16le",
         "-ar", "37800", "-ac", "2", "-map_metadata", "-1", "-fflags", "+bitexact", clip])
    raw = generated / "bunny.strcd"
    run([TOOLS / "psxavenc/build/psxavenc", "-t", "strcd", "-v", "v2",
         "-f", "37800", "-b", "4", "-c", "2", "-s", "320x192", "-r", "30",
         "-x", "2", "-F", "1", "-C", "1", clip, raw], log=generated / "encode.log")
    stream = check_stream(raw)
    data = raw.read_bytes()
    (HERE / "disc/bunny.str").write_bytes(b"".join(
        data[i + 16:i + 2352] for i in range(0, len(data), 2352)))
    run(["make", "clean"], cwd=HERE)
    run(["make", f"PREFIX={args.prefix}", f"FORMAT={args.format}",
         f"PSYQ={args.psyq}"], cwd=HERE,
        log=generated / "ps1-build.log")
    (generated / "system-area.dat").write_bytes(bytes(12 * 2336))
    run([TOOLS / "mkpsxiso/build/mkpsxiso", "-y", "-lba", "bunny.lba", "disc.xml"],
        cwd=HERE / "disc", log=generated / "disc-build.log", env=dict(os.environ, TZ="UTC"))
    paths = [args.source, clip, raw, HERE / "disc/bunny.str", HERE / "movie.ps-exe",
             HERE / "disc/bunny.bin", HERE / "disc/bunny.cue"]
    sdk_inputs = {str(p.relative_to(args.psyq)): sha(p)
                  for directory in ("include", "lib")
                  for p in sorted((args.psyq / directory).rglob("*")) if p.is_file()}
    player_inputs = [HERE / name for name in
                     ("movie.c", "Makefile", "CREDITS.txt", "disc/disc.xml", "disc/SYSTEM.CNF")]
    manifest = {"sdk_inputs_sha256": sdk_inputs,
                "player_inputs_sha256": {str(p.relative_to(HERE)): sha(p) for p in player_inputs},
                "submodules": {name: subprocess.check_output(
                    ["git", "-C", str(ROOT / name), "rev-parse", "HEAD"], text=True).strip()
                    for name in ("nugget",)},
                "source_interval_seconds": [19, 21], "content": [320, 180],
                "encoded": [320, 192], "fps": 30, "xa": "37800 Hz stereo 4-bit file=1 channel=1",
                "tools": {k: v[1] for k, v in PINS.items()}, "stream": stream,
                "psxavenc_flush_patch_sha256": sha(HERE / "psxavenc-flush.patch"),
                "psxavenc_patches_sha256": {p.name: sha(p) for p in PSXAVENC_PATCHES},
                "ffmpeg": subprocess.check_output(["ffmpeg", "-version"], text=True).splitlines()[0],
                "compiler": subprocess.check_output([args.prefix + "-gcc", "--version"], text=True).splitlines()[0],
                "artifacts": {str(p): {"bytes": p.stat().st_size, "sha256": sha(p)} for p in paths},
                "commands": COMMANDS}
    (generated / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(stream, indent=2))
    print("Disc:", HERE / "disc/bunny.cue")


if __name__ == "__main__":
    main()
