# PSY-Z Porting Guide

This guide provides comprehensive instructions for porting PlayStation 1 games and applications to PSY-Z, enabling them to run natively on modern platforms while maintaining compatibility with the original PlayStation 1 hardware.

## Code Modifications

When adapting your PlayStation 1 code to work with PSY-Z, several key modifications are required to ensure proper functionality across different architectures and platforms while sharing the same PlayStation 1 code base.

### Ordered Tables

The PSY-Q SDK uses something called _ordered table_, often abbreviated as `ot`. The type of this construct is `u_long*`. Due to how PSY-Q handles memory, the ordered table will not work on other platforms without modification. You need to replace all instances of `u_long*` used for ordered tables with the PSY-Z `OT_TYPE*`.

**Example:**

```c
// Before (PSY-Q only)
u_long ot[OTSIZE];

// After (PSY-Q and PSY-Z compatible)
OT_TYPE ot[OTSIZE];
```

### Custom GPU Primitives

In cases where your application or game recreates or reuses GPU primitives defined in `libgpu.h`, be sure to replace the struct field at the beginning of your custom structs from `u_long tag` to `O_TAG`. You can refer to `struct POLY_F3` or similar structures in `libgpu.h` for examples of how the original structures were adjusted.

**Example:**

```c
// Before (PSY-Q only)
typedef struct {
    u_long tag;
    // ... other fields
} MyCustomPrimitive;

// After (PSY-Q and PSY-Z compatible)
typedef struct {
    O_TAG tag;
    // ... other fields
} MyCustomPrimitive;
```

### Pointer Storage and Data Types

Use `u_long` types for data that is expected to hold pointers, as the type changes size to 8 bytes for 64-bit builds. **Ensure pointers are never stored in `s32` or `u32` types**, as addresses will be truncated on 64-bit builds, leading to crashes or undefined behavior. Consider using either pointer types (e.g., `s32*`) or `u_long` when you need to store addresses.

**Example:**

```c
// Before (unsafe for 64-bit)
u32 myPointer = (u32)&someData;

// After (safe for 32-bit and 64-bit)
u_long myPointer = (u_long)&someData;
// or better yet:
s32* myPointer = &someData;
```

If your software uses `u_long` to store 32-bit values that are not pointers, you will need to change the type to `unsigned int`, otherwise data will not be stored as expected on 64-bit builds.

### Header Includes

Your project should import PlayStation 1 headers directly using angle brackets, such as `<libgpu.h>`, `<libgte.h>`, etc. This makes it easier to swap between the PSY-Q SDK and PSY-Z by simply changing the include path. Include the header `psyz.h` at the beginning of your source files, along with the modified `lib*.h` headers.

**Example:**

```c
#include <psyz.h> // always first
#include <libgpu.h>
#include <libgte.h>
#include <libetc.h>
```

To include the necessary headers, add `-Ipsyz/include` to your GCC or Clang compiler flags.

## Common Pitfalls to Avoid

### Symbol Overlaps

Ensure the ported application does not have symbols that overlap in memory, which will lead to bugs or crashes. For example, declaring `s32 D_800A1230[2]` and `s32 D_800A1234` will produce a correct PlayStation 1 decompilation build, but it will produce bugs with PSY-Z or any porting library because the symbols are expected to overlap.

**Example of problematic code:**

```c
// These overlap in memory - will cause bugs!
s32 D_800A1230[2];  // occupies 0x800A1230-0x800A1237
s32 D_800A1234;     // overlaps at 0x800A1234
```

**Corrected code:**

```c
s32 D_800A1230[2];  // access D_800A1230[1] instead of creating a separate symbol
```

### Hardcoded Memory Addresses

Ensure no hardcoded memory addresses are used in your code. A statement like `*(s32*)0x800A1234 = value;` is guaranteed to crash at runtime on modern platforms, as these memory addresses are specific to PlayStation 1 hardware and are invalid on different architectures.

## Build System Integration

PSY-Z supports integration with both CMake and Make-based build systems, allowing you to target multiple platforms from a single codebase.

### CMake Integration

To integrate PSY-Z into your project:

1. Add the PSY-Z directory as a subdirectory or git submodule
2. Link your executable against the `psyz` library
3. Set the `__psyz` compile definition

```cmake
cmake_minimum_required(VERSION 3.10..3.31)

project(mygame)

set(CMAKE_C_STANDARD 11)

add_subdirectory(path/to/psyz build)
add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE psyz)
target_include_directories(psyz PRIVATE path/to/psyz/include)
target_compile_definitions(psyz PRIVATE __psyz)
```

### Makefile Integration

For Makefile-based projects targeting both PlayStation 1 and PSY-Z, you can use the nugget build system. See the `samples/` directory for complete examples.

**Example Makefile:**

```makefile
TARGET = mygame
TYPE = ps-exe
CFLAGS = -std=gnu11

SRCS = main.c

include path/to/psyz/psx.mk
```

You can look into the `samples/` directory for complete examples on how to set up a project to target both PlayStation 1 hardware and PSY-Z on PC.

### Unix-like mix-ups

The PlayStation 1 SDK shares some functions and declarations that look similar to their Unix counterparts. These include `open`, `close`, `lseek`, `read`, `write`, and defines such as `O_RDONLY` or `O_CREAT`. While they might look similar, they are **not compatible** with the POSIX standard:

* **Flag values differ**: For example, `O_CREAT` on PS1 is `0x200`, but on Linux it's `0100` (or `0x40`).
* **Path formats differ**: PlayStation accepts paths like `bu00:savegame` (memory card paths) that modern platforms don't understand.
* **Function signatures differ**: Some parameters and behavior differ between PS1 SDK and POSIX standards.

**PSY-Z Approach:**

To handle these incompatibilities, PSY-Z provides platform abstraction:

1. **Never include `<fcntl.h>` in your code**: This prevents conflicts between PS1 and modern platform definitions.
2. **Use PS1 flags from `<romio.h>`**: Instead of `O_WRONLY`, use `FWRITE`. Instead of `O_CREAT`, use `FCREAT`, etc.
3. **Use PSY-Z platform wrappers**: Functions like `open` are automatically renamed as `psyz_open` (see `psyz.h`), which will internally translate the calls to what the targeted platform understands.

This ensures your code works similarly to a PlayStation 1 and modern platforms with little modification.

## Platform-Specific Considerations

### Windows with MSVC

Windows with MSVC does not favor GNU C extensions, which may present some challenges when porting code originally written for GCC-based toolchains:

* **Function parameters must have a name**: Anonymous parameters like `void foo(int)` are not allowed; use `void foo(int value)` instead.
* **Zero-length arrays are not supported**: Arrays must have a size of at least 1. Use flexible array members or dynamic allocation where appropriate.
* **`long` type is 32-bit on 64-bit targets**: Unlike GCC on Unix-like systems where `long` is 64-bit on 64-bit platforms, MSVC keeps `long` as 32-bit even on 64-bit Windows. The custom `u_long` type in PSY-Z is still 64-bit on 64-bit builds.

If these limitations represent a significant obstacle for your project, consider using **clang-cl** instead, which provides better GNU C extension support while still using the MSVC ABI and Windows SDK.

## Contributing to PSY-Z

PSY-Z is not perfect or complete. Bugs or missing implementations are bound to happen. If any of those issues are spotted, the best way forward is to open an issue on the project repository or reach out to the maintainers. Ensuring PSY-Z works on your project will enable future ports more easily and help improve the SDK for the entire community.

When reporting issues, please include:

* The platform and compiler you're using
* A minimal reproducible example if possible
* The expected behavior versus actual behavior
* Any relevant error messages or logs
