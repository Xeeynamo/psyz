# PSY-Z

## Running Tests

Automated tests to ensure no regressions are introduced between changes.

### Linux

```bash
# run tests + GTE tests using hardware acceleration
make test

# run Windows tests using mingw32 and wine
make test-wine
```

### Windows

```batch
test.bat
```

Runs tests using Visual Studio as the CMake generator.
