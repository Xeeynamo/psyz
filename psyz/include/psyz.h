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

typedef enum {
    // Auto-detect use of driver VSync (default)
    // - Tests monitor refresh rate on initialization (adds ~30ms startup time)
    // - Uses driver VSync if monitor is detected between 57Hz-63Hz
    // . (±5% tolerance). This covers both 59.94Hz (NTSC) and 60Hz
    // - Uses manual limiter otherwise (144Hz, 165Hz, etc.)
    // - Balances performance and accuracy automatically
    PSYZ_VSYNC_AUTO,

    // Always use driver VSync
    // Pros: Zero CPU overhead and no frame pacing on matching refresh rates
    // Cons: Game will run faster on high refresh monitors
    // Cons: Framerate always matches monitor exactly.
    // . On 60Hz monitor: runs at 60fps instead of NTSC 59.94fps
    // . causing ~1 second drift every ~16.7 minutes of gameplay
    // Real NTSC PSX hardware runs at 59.94Hz (60/1.001)
    // . PAL PSX hardware runs at exactly 50Hz
    // Best when combined with driver control panel frame rate limit
    // IMPORTANT: PAL games (50fps) will run at 60fps on 60Hz monitors without
    // . driver-level limiting, which is incorrect
    PSYZ_VSYNC_ON,

    // Always use internal manual frame limiter
    // Pros: Precise 59.94fps (NTSC) / 50fps (PAL) matching real hardware
    // Pros: Safe for VRR displays
    // Pros: Consistent timing across all monitor refresh rates
    // Cons: ~6% CPU usage on one core (1ms busy-wait per frame for precision)
    // Cons: May have minor frame pacing variance on non-VRR displays
    PSYZ_VSYNC_OFF,
} Psyz_VsyncMode;

typedef struct {
    // Frame timing stats (from vsync system)
    double last_frame_time_us;       // duration of last frame
    double target_frame_time_us;     // target frame time
    unsigned long long total_frames; // total frames rendered
    int using_driver_vsync;          // 1 for VSync, 0 for limiter
    // Buffer stats (from GPU buffer system)
    int vertex_buffer_capacity;      // Current allocated vertex capacity
    int vertex_buffer_queued;        // Currently queued vertices waiting for flush
    int vertex_buffer_peak;          // Peak vertices used since initialization
    int vertex_buffer_growth_count;  // Number of times buffer grew
    int flush_per_frame;             // lower value means higher performance
} Psyz_GpuStats;

// Set VSync mode (default: AUTO)
// Returns: 0 on success, -1 if invalid mode
int Psyz_SetVsyncMode(Psyz_VsyncMode mode);

// Configure GPU knobs to tweak performance
// min_vertices: Initial allocation size (must be >= 4, multiple of 4)
// max_vertices: Maximum allowed size (must be <= 65536, >= min_vertices)
// Returns: 0 on success, -1 on invalid parameters
// Note: Does not shrink existing buffers, only affects future allocations
// Must be called as early as possible
int Psyz_SetGpuLimits(int min_vertices, int max_vertices);

// Query GPU statistics for profiling
// stats: Pointer to Psyz_GpuStats struct to fill (must not be NULL)
// Returns: 0 on success, -1 if system not initialized or stats is NULL
int Psyz_GetGpuStats(Psyz_GpuStats* stats);
