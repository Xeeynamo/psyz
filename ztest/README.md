# ztest

Minimal C11 test framework designed to run on modern hardware and retro consoles. The framework requires `ztest.c` + `ztest.h`, plus the two bundled stb headers for image comparison.

```c
#include "ztest.h"

ZTEST_SETUP(math) {
  // runs once before every 'math' test group
}

ZTEST(math, add) {
    zassert_s32_eq(4, 2 + 2);
    zexpect_str_eq("abc", some_string());
}

ZTEST_MAIN
```

```
🟢 math::add
```

## Assertions

```c
z{assert|expect}_{type}_{cond}(exp_ected, act_tual)
```

- `zassert_*` fails and leaves the test; `zexpect_*` fails and keeps going. Both return 1 on success.
- types: `s32 u32 s16 u16 s8 u8 char str ptr uintptr u8array image`
- conds: `eq ne gt ge lt le`, read as `act <cond> exp`: `zexpect_s32_gt(5, x)` passes when `x > 5`.
- `u8array` takes a third length argument; `image` only has `eq` and `ne`.
- Both operands are cast to the named type first.

Controls and helpers, where `...` is used as an optional printf-style string literal:

- `zfail(...)` fails and keeps going.
- `zabort(...)` fails immediately.
- `zskip(...)` skip the test.
- `zskip_targets("psp;ps1")` skip the test on given targets.
- `zterminate(...)` immediately abandon the test suite.
- `zprintf(...)` per-test log printed after the result line on test fail or `--verbose`.
- `zerrorf(...)` same as above, but prints in red.

## Images

```c
zassert_image_eq("draw_ft4", zimage_frontbuffer(), zimage_r5g5b5_exact);
```

- The expected image is `expected/{NAME}.png`.
- The optional image `expected/{NAME}.{TARGET}.png` takes precedence.
- On failure the actual image is written to `expected/{NAME}.{TARGET}.actual.png`.
- The third argument picks the comparison: `zimage_r5g5b5_exact`, `zimage_r5g5b5_with_tolerance(tol)`,
  `zimage_r5g5b5_with_precision(prec)`, `zimage_r5g5b5_with_tol_prec(tol, prec)`,
  `zimage_r5g5b5_region(x, y, w, h, tol, prec)`. Tolerance is per 5-bit channel, precision is the fraction of
  matching channels.
- `zassert_image_eq` consumes the actual image. Every image is released after the test's teardown.
- `zimage_frontbuffer()` is a weak symbol: PSP, PS1, NDS and N64 capture the real display; elsewhere it returns an
  empty image until the program defines its own. On PS1 the display start cannot be read back from the GPU, so
  call `zimage_set_display_origin(x, y)` or override the function.

Included image comparisons are:
- `zimage_r5g5b5_exact` convert exp and act to 5-bit per channel, then compare byte per byte.
- `zimage_r5g5b5_with_tolerance(tol)` match on channels `(exp - act) <= tol`, where `tol=0` is same as `exact`.
- `zimage_r5g5b5_with_precision(prec)` match on delta `(pixel_total - pixel_different) / pixel_total >= prec`, where `prec=1.0` is same as `exact`.
- `zimage_r5g5b5_with_tol_prec(tol, prec)` combines both tolerance and precision when comparing


## Output

The mode is picked from the terminal, or with `--output=emoji|color|plain` / `ZTEST_OUTPUT`:

| mode  | line while running | result                                  | Use-case                    |
|-------|--------------------|-----------------------------------------|-----------------------------|
| emoji | `⏳ group::name`   | 🟢, ❌, or 🟡 when test ends.          | Modern terminals            |
| color | `RUN  group::name` | green `PASS`, red `FAIL`, yellow `SKIP` | Standard TTYs               |
| plain | `group::name`      | ` PASS`, ` FAIL` or ` SKIP` appended    | Pipe to file, GH Actions CI |

The last line is always `ztest: N passed, N failed, N skipped`.

Options:
- `--verbose` to always print `zprintf` and not suppress stdout/stderr
- `--list` list of all tests in plain `group::test` format
- `--filter=grp,grp::test,-excluded` includes and excludes specific or group of tests. Examples:
  - `--filter=gpu::*` runs the `gpu` group only.
  - `--filter=gpu::*,gte::rot*` runs the `gpu` group plus the `gte` tests starting with `rot`.
  - `--filter=-libcd::*,-gpu::draw_lines` runs everything except the `libcd` group and one test.
- `--output=emoji|color|plain` forces the output mode and takes precedence over `ZTEST_OUTPUT`. Without either,
  the mode is detected: `plain` when stdout is not a terminal, `NO_COLOR` is set or `TERM` is unset or `dumb`; `color` when
  `TERM` is `linux` or the locale is not UTF-8; `emoji` otherwise. On Windows `emoji` needs Windows Terminal
  (`WT_SESSION` or `TERM_PROGRAM`). Consoles default to `plain`. An unknown value falls back to detection.

Targets without a command line read cmdline args from `ztest.args`.

## Targets

`ZTEST_TARGET` is detected at compile time (`linux windows macos ios android psp ps1 nds n64`) and can be
overridden with `-DZTEST_TARGET=`. `ztest_add_tag("name")` adds names matched by `zskip_targets` and
`ztest_is_target`, for example `ppsspp` or `pcsx-redux`.

| target  | output                         | files                                |
|---------|--------------------------------|--------------------------------------|
| desktop, Android | stdout                | stdio                                |
| PSP     | `sceIoWrite`                   | `host0:/` through `sceIo`            |
| PS1     | BIOS TTY, or `ztest.log` over PCDrv on hardware | PCDrv (pcsx-redux break or nops serial) |
| NDS     | gdb breakpoint hooks           | NitroFS reads, gdb dumps writes      |
| N64     | gdb breakpoint hook, ISViewer  | DragonFS reads, base64 blocks writes |

Freestanding builds can define `ZTEST_HEAP_SIZE` for a built-in allocator, or `ZTEST_MALLOC`/`ZTEST_REALLOC`/`ZTEST_FREE`.
Without a libc, define `ZTEST_NEED_<NAME>` (for example `ZTEST_NEED_MEMCPY`) for each missing function and
`ztest.c` provides it. The CMake build detects them on its own.

## Running

Wrapper to run tests headless on any supported target.

```
zrunner.py <runner> --exe <binary> --workdir <dir with expected/> [--verbose] [--filter ...] [-- args]
```

Supported runners are:

- `native`: runs on host.
- `wine`: runs Windows tests on Linux or macOS.
- `psp-emu` requires `PPSSPPHeadless`, found in most PPSSPP distributions.
- `psp-hw` requires a PSP connected via USB, running PSPLINK.
- `ps1-emu` requires `pcsx-redux`.
- `ps1-hw` requires a PS1 connected via UART, running UniROM.
- `nds-emu` requires `melonDS`
- `n64-emu` requires `ares`
- `android` runs on any device reachable via `adb shell`, phone or emulator: `--serial`, else `ANDROID_SERIAL`,
  else the first device listed. With none online it boots the `--avd` headless (default: the first AVD) and
  shuts it down afterwards.

Emulators `PATH`, and can be overridden via `--emulator-path=<file>`. For PlayStation 1, `nops.exe` runs through `mono`. The
debugger for `nds-emu` and `n64-emu` is the first `gdb-multiarch` or `gdb` in `PATH` that knows ARM and MIPS.

Other options:

- `--serial=<id>` picks the adb device, or the PS1 serial port (default `/dev/ttyUSB0`).
- `--bios=<image>` gives pcsx-redux a BIOS when it has none configured.
- `--gdb-port=<port>` for the emulator's GDB stub (melonDS 3333, ares 9123 by default).
- `--headless` runs ares inside `gamescope --backend headless`, which happens anyway without a display.
- `--avd=<name>` is the Android virtual device to boot, by default the first one listed.
- `--timeout=<seconds>` for the whole run (default 300). On timeout the running test is named.

## Self-test

`selftest/` tests the whole framework, including tests that fail on purpose. `make test-<target>` builds it
for a target and compares the transcript with `selftest/selftest.expected.txt` byte for byte:

```
make test-native test-psp-emu test-psp-hw test-ps1-emu test-ps1-hw test-nds-emu test-n64-emu
make test-android test-wine
```

N64 builds run in docker: `make libdragon-image` builds libdragon once into a local image.
