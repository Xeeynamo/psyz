#ifndef PSYZ_PLATFORM_H
#define PSYZ_PLATFORM_H

// Define PLATFORM_64BIT for 64-bit platforms
// Works with GCC, Clang, and MSVC
#if defined(__LP64__) || defined(_LP64) || defined(_WIN64) ||                  \
    defined(__x86_64__) || defined(__amd64__) || defined(__aarch64__) ||       \
    defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM64) ||               \
    defined(__ia64__) || defined(__ppc64__) || defined(__powerpc64__)
#define PLATFORM_64BIT
#endif

#ifdef __psyz
#ifndef PLATFORM_PSYZ
#define PLATFORM_PSYZ
#endif
#endif

#endif
