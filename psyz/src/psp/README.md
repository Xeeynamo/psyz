# PlayStation Portable target

This folder isolates everything related to the PSP implementation.

## Port a Psy-Z title on PSP

The README serves as a barebone guide on how to port your game on PSP

### Requirements

* [pspdev SDK](https://pspdev.github.io/installation.html) to create PSP builds.
* [PSPLINK](https://pspdev.github.io/debugging.html) for hardware debugging and profiling.
* [PPSSPP](https://www.ppsspp.org/download/) to run tests without the real hardware.

The toolchain expects `PSPDEV` in the environment. Alternatively, `~/pspdev`
or `/usr/local/pspdev` must be provided.

### Configure

* `psyz_title(<target> "TITLE")` will set the game name shown in the VSH, as well as the window title on PC.
* Pre-defined macros `__PSP__` or `PLATFORM_PSP` for PSP-specific code.

### Build

The port builds with the [pspdev SDK](https://pspdev.github.io/) through the toolchain shim in this directory:

```sh
cmake -B build/psyz_psp -DCMAKE_TOOLCHAIN_FILE=path_to_psyz/psyz/src/psp/psp.cmake
cmake --build build/psyz_psp
```

Copy the generated `build/psyz_psp/EBOOT.PBP` to `ms0:/PSP/GAME/YOUR_GAME/EBOOT.PBP`.

### Debugging

```sh
cmake -B build/psyz_psp -DCMAKE_TOOLCHAIN_FILE=path_to_psyz/psyz/src/psp/psp.cmake -DBUILD_PRX=1
cmake --build build/psyz_psp
usbhostfs_pc & # run from the directory that should map to host0:/
pspsh          # at the prompt, run: ldstart host0:/build/psyz_psp/yourgame.prx
```

Use an interactive `pspsh` rather than `pspsh -e "..."` if you want to see the
game's `stdout`/`stderr`: the `-e` form sets `notty`, so it never connects the
TTY output channel and prints nothing the game logs.

## Working on the Psy-Z PSP backend

### Files

| File            | Role                                               |
| --------------- | ---------------------------------------------------|
| `psp.cmake`     | `psyz_psp_*` helpers, pspdev cmake shim            |
| `psp_startup.c` | module info, HOME exit callback                    |
| `psp_gpu.c`     | `sceGu` hardware accelerated renderer              |
| `psp_input.c`   | `sceCtrl` digital + analog pad                     |
| `psp_audio.c`   | software SPU renderer, PCM sent to `sceAudio`      |
| `psp_file.c`    | native `sceIo*` file I/O and directory enumeration |
| `psp_log.c`     | log to stderr for psplink/PPSSPP                   |
| `psp_misc.c`    | no-op stubs, misc stuff                            |

### Testing

```sh
# needs PPSSPPHeadless executable in environment PATH
make test-psp-emu

# needs a real console with PSPLINK running
make test-psp-hw
```

The main difference between the two is `test-psp-emu` creates a proper ELF,
while `test-psp-hw` creates a PRX module needed by PSPLINK.

Because PPSSPP is more lenient than the real GE (texture alignment, blend fixups,
the dither matrix), testing on real hardware before any code submission is
strictly required.

## Next steps

* `Psyz_PspExitRequested` must become a cross-platform API, wired with SDL3 `QuitPlatform`
* Add `sceUtilitySavedata` support for game save and psy-z user configuration.
* Add `sceSas` for audio mixing to offload complexity and computation to the Media Engine.
    * Or, find a way to offload SPU emulation to the Media Engine.
* Add `psyz_psp_icon0` to specify the 144x80 PNG icon (optional)
* Add `psyz_psp_pic1` to specify the 480x272 PNG background (optional)
* Add `psyz_psp_snd0` to specify the Atrac3 sound playing in the background (optional)
* Add `psyz_psp_icon1` to specify the 144x80 PMF video (optional)

## Platform notes

### Aspect ratio

The PS1 supports the different set of resolutions 256x240, 320x240, 384x240, 512x240, and interlaced with 480 of vertical resolution. Meanwhile the PSP supports a fixed resolution of 480x272 (TODO confirm resolution through video out). Therefore modifications are required to take full advantage of the PSP screen.

`PSYZ_ASPECT_DISPLAY` is the default behavior, which scales up or down any resolution to 320x240 using a simple linear interpolation. Linear interpolation is mainly used to avoid 1px wide lines disappearing when scaling down from 384x240. Querying `Psyz_VideoGetDisplaySize()` will return a resolution of 362x272, which must be respected to fill the entire screen while respecting the original aspect ratio.

`PSYZ_ASPECT_SQUARE` presents pixels 1:1 to the screen, centered on all sides. Aspect ratio of resolutions other than 320x240 will not be corrected, but output will appear crisp and clear. Querying `Psyz_VideoGetDisplaySize()` will return the full 480x272 space.

If the ported game wants to use the full 480x272 screen, the decision of using either `PSYZ_ASPECT_DISPLAY` or `PSYZ_ASPECT_SQUARE` must be made upfront to correctly decide how 2D sprites and HUD must scale. `Psyz_VideoSetDrawArea(rect)` can be used to instruct Psy-Z to which coordinates and size the output will be displayed to. Black bars will automatically be added based on the intended draw area.

### `StoreImage` and `MoveImage` caveats

On PS1 and PC, `StoreImage` downloads the content from the GPU VRAM to get feedback of content written to front buffer. This is used to achieve special effects.

Primitives rasterize into the framebuffer, which the shadowed VRAM never sees, so reading `g_vram` alone would return whatever was last uploaded and miss every rendered pixel. `StoreImage` and `MoveImage` therefore call `VramReadbackRect` first: when something has been drawn this frame, it syncs the GE and copies the framebuffer rows covering the requested rect back into the shadow. Only the region the framebuffer currently maps can be recovered this way, which is where rendered pixels live.

The readback stalls the GE, and that stall is expensive: measured on real hardware at `-O3`, a `MoveImage` of an on-screen 64x64 rect costs about 2.2ms against 0.12ms for the same call off-screen, roughly 12% of a 60fps frame. The 8KB copy is only a few percent of that; the rest is `sceGuFinish`/`sceGuSync` draining the GE and losing the cross-frame overlap the deferred present relies on. A bounding box of what has been drawn (`fb_drawn_*`) lets transfers that miss it skip the stall entirely, so off-screen VRAM shuffling stays cheap.

Games that read back drawn pixels every frame will feel this. Tracking dirty rects per tile, so a readback only syncs when the requested rect really intersects pending GE work, would cut it further and is not implemented.

### SPU emulation

This entirely runs on software. Samples are synthesized via `Psyz_SpuPullSamples` and sent straight to `sceAudio` as PCM 44100Hz 2ch samples. This current implementation mirrors the PC backend.
s
The audio backend does not yet support `sceSas`, which passes the complexity and computation cost to the PSP Media Engine.

### File I/O

The Sony I/O manager is used, using the EBOOT folder as working directory. Use `Psyz_AdjustPathCB` for absolute `ms0:/` layouts.

`sceUtilitySavedata` for saving game data and Psy-Z specific configuration is not yet supported.


### PSPLINK: Don't use fopen

PSPLINK patches `sceIoOpen` to work accessing files on `host0:/`.
This seems to be not working with `fopen`, which will always return `NULL`.

### PSPLINK: Don't use sceIoChdir on constructors

Calling `sceIoChdir` in a  `__attribute__((constructor))` functions will not
work due to a PSPLINK design limitation. Any constructor functions will be
executed when PSPLINK loads the PRX. But then it sets it own chdir before
running the PRX entrypoint.
