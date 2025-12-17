#ifndef PSYZ_H
#define PSYZ_H

#include <types.h>
#include <log.h>
#ifdef __psyz // exclude when targeting the PSX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#endif

// Define PLATFORM_64BIT for 64-bit platforms
// Works with GCC, Clang, and MSVC
#if defined(__LP64__) || defined(_LP64) || defined(_WIN64) ||                  \
    defined(__x86_64__) || defined(__amd64__) || defined(__aarch64__) ||       \
    defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM64) ||               \
    defined(__ia64__) || defined(__ppc64__) || defined(__powerpc64__)
#define PLATFORM_64BIT
#endif

#define open my_open
#define close my_close
#define lseek my_lseek
#define read my_read
#define write my_write
#define ioctl my_ioctl

#ifndef _WIN32
int my_open(const char* devname, int flag);
int my_close(int fd);
long my_lseek(long fd, long offset, long flag);
long my_read(long fd, void* buf, long n);
long my_write(long fd, void* buf, long n);
long my_ioctl(long fd, long com, long arg);
#endif

#ifdef _MSC_VER
#define __builtin_memcpy memcpy
#endif

#define DISP_WIDTH 256
#define DISP_HEIGHT 256
#define SCREEN_SCALE 2
#define VRAM_W 1024
#define VRAM_H 512
#define VRAM_STRIDE 2048

#define NOP
#define CLAMP(x, min, max) x < min ? min : (x > max ? max : x)

#ifndef LEN
#define LEN(x) ((s32)(sizeof(x) / sizeof(*(x))))
#endif

#ifndef LENU
#define LENU(x) ((u32)(sizeof(x) / sizeof(*(x))))
#endif

#endif
