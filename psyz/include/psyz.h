#ifndef PSYZ_H
#define PSYZ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <psyz/platform.h>
#include <psyz/types.h>
#include <psyz/log.h>

#ifdef __psyz // exclude when targeting the PSX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
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

// Set the title of the game window
void Psyz_SetTitle(const char* str);

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
    double last_frame_time_us;       // duration of last frame
    double last_draw_time_us;        // render time excluding vsync wait
    double target_frame_time_us;     // target frame time
    unsigned long long total_frames; // total frames rendered
    int using_driver_vsync;          // 1 for VSync, 0 for limiter
} Psyz_GpuStats;

// Set VSync mode (default: AUTO)
// Returns: 0 on success, -1 if invalid mode
int Psyz_SetVsyncMode(Psyz_VsyncMode mode);

// Get frame timing statistics
// Returns: 0 on success, -1 if stats is NULL or platform not initialized
int Psyz_GetGpuStats(Psyz_GpuStats* stats);

// Get frame output as a byte array. This function is very slow.
// Returns: NULL on failure, otherwise ptr to be destroyed with free(ptr)
unsigned char* Psyz_AllocAndCaptureFrame(int* w, int* h);

// Adjust a PlayStation 1 path to the host filesystem
// Handles memory card paths (bu00:, bu10:, etc.) and other special cases.
// If a custom callback is set via Psyz_AdjustPathCB and returns >= 0,
// the callback result is used; otherwise internal adjustment is applied.
// The function is used internally when opening and creating files, or
// when enumerating the list of files in the specified directory.
//
// IMPORTANT: The filename portion (after the last path separator) is
// automatically truncated to 19 characters to match PS1 DIRENTRY.name[20]
// which requires null-termination for compatibility with SDK string functions.
// This truncation is applied regardless of whether callback or internal
// adjustment is used.
//
// Parameters:
//   dst: Destination buffer for adjusted path (must be valid)
//   src: Source path to adjust (PlayStation 1 format)
//   maxlen: Maximum length of destination buffer
void Psyz_AdjustPath(char* dst, const char* src, int maxlen);

// Set custom path adjustment callback
// The callback is invoked before internal path adjustment
// Return value from callback:
//   < 0: No adjustment done, fall back to PSY-Z internal adjustment
//   >= 0: Number of bytes written to dst (adjustment successful)
// Parameters:
//   dst: Destination buffer for adjusted path
//   src: Source path to adjust
//   maxlen: Maximum length of destination buffer
void Psyz_AdjustPathCB(int (*callback)(char* dst, const char* src, int maxlen));

// Join two path components with the platform's path separator
// Automatically handles path separators to avoid double separators
// Parameters:
//   left: Left path component (modified in-place)
//   right: Right path component to append
//   maxlen: Maximum length of left buffer
// Returns: Pointer to left on success, NULL if buffer too small
char* Psyz_JoinPath(char* left, const char* right, int maxlen);

#ifdef __cplusplus
}
#endif

#endif
