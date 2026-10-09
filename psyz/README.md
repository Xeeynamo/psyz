# PSY-Z

## Running Tests

Automated tests to ensure no regressions are introduced between changes.

### Linux

```bash
# run tests + GTE tests using hardware acceleration
make test

# run Windows tests using mingw32 and wine
make test-wine

# consoles and Android, emulated or on real hardware
make test-psp-emu test-psp-hw
make test-ps1-emu test-ps1-hw
make test-android
```

`test-ps1-hw` needs `nops.exe` (run through `mono`) and `upx` in `PATH`.

The tests are written with [ztest](../ztest/README.md). Pass options straight to the runner, for example
`python3 ../ztest/zrunner.py native --exe tests/build/sdl3-gpu-hw/psyz_tests --workdir tests --filter='gpu::*'`.
When an image comparison fails, the frame is written to `tests/expected/<name>.<target>.actual.png`.

### Windows

```batch
test.bat
```

Runs tests using Visual Studio as the CMake generator.
