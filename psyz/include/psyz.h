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

#define open psyz_open
#define close psyz_close
#define lseek psyz_lseek
#define read psyz_read
#define write psyz_write
#define ioctl psyz_ioctl
#define undelete psyz_undelete
#define _get_errno psyz_get_errno

#ifdef _WIN32
int psyz_open(const char* devname, int flag, ...);
int psyz_close(int fd);
long psyz_lseek(int fd, long offset, int flag);
int psyz_read(int fd, void* buf, unsigned int n);
int psyz_write(int fd, const void* buf, unsigned int n);
long psyz_ioctl(long fd, long com, long arg);
#else
int psyz_open(const char* devname, int flag);
int psyz_close(int fd);
long psyz_lseek(long fd, long offset, long flag);
long psyz_read(long fd, void* buf, long n);
long psyz_write(long fd, void* buf, long n);
long psyz_ioctl(long fd, long com, long arg);
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

struct PsyzDiskRead {
    // disk sector, can be used as a unique file identifier
    unsigned int sector;

    // byte count required to be read
    unsigned int size;

    // buffer to read the data into, max index is buffer[size-1]
    void* buffer;
};

// returns byte read, -1 is not found, unsuccessful or not implemented
typedef int (*DiskReadCB)(struct PsyzDiskRead* read);

// Set path to CUE file, simulating a CD loaded
// negative value means unsuccessful, zero means ok
int Psyz_SetDiskPath(const char* diskPath);

// Set callback when a disk read is triggered
// if cb is NULL or returns a negative value, PSY-Z falls back to CD emulation
void Psyz_SetDiskReadCB(DiskReadCB cb);
