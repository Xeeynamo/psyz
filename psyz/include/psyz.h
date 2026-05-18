#ifndef PSYZ_H
#define PSYZ_H

// Main header to include when re-compiling a PS1 application targeting foreign
// platforms. It contains all publicly exposed APIs from PsyZ.
//
// All public API of PsyZ use the prefix `Psyz_`.
//
// APIs that re-implement PSY-Q-specific behavior use the prefix `Psyz_{lib}`.
// These are guaranteed to be platform-agnostic and exist for the sole purpose
// of emulating certain PlayStation hardware characteristics. Typically this is
// a lightweight layer that decodes hardware registers, manages state, and
// ingests commands. Events are then dispatched to the relevant subsystem
// backend. Examples: `Psyz_Cd`, `Psyz_Spu`, etc.
//
// APIs that implement the backend of a specific subsystem for the target
// platform use the prefix `Psyz_{subsystem}`. These are the lowest layer of
// abstraction of PsyZ, and they generally communicate with the platform
// library or the hardware directly. When targeting a new platform, all the
// subsystem endpoints are required to be implemented to guarantee full
// functionality. All micro-optimizations and the use of hardware-specific
// quirks live here. Examples: `Psyz_Audio`, `Psyz_Draw`, etc.
//
// A typical game running on PsyZ is structured as follows:
//    Game ->
//        PSY-Q decompiled APIs ->
//            PsyZ platform agnostic PS1 hardware emulation ->
//                PsyZ platform-specific subsystems ->
//                    Hardware, Driver or Operating System

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

// PS1 SPU constants (the SPU emulator is platform-agnostic; SDL or any other
// audio backend pulls 44100 Hz stereo s16 frames from Psyz_SpuPullSamples).
#define PSYZ_SPU_RAM_SIZE (512 * 1024)
#define PSYZ_SPU_NUM_VOICES 24
#define PSYZ_SPU_SAMPLE_RATE 44100

// Initialize SPU emulation state. Idempotent; safe to call multiple times.
void Psyz_SpuInit(void);

// Reset SPU state. When `hot` is non-zero, RAM contents are preserved across
// the reset (mirrors the PSX-Q "hot init" semantics for libspu).
void Psyz_SpuReset(int hot);

// Set the SPU RAM transfer address (byte address into the 512 KB RAM).
void Psyz_SpuSetTransferAddr(unsigned int addr);

// Write `size` raw bytes to SPU RAM starting at the current transfer address.
// The address auto-advances and wraps at 512 KB.
void Psyz_SpuWriteToRam(const unsigned char* src, unsigned int size);

// Direct pointer to the 512 KB SPU RAM (for tests and offline rendering).
unsigned char* Psyz_SpuGetRam(void);

// Generate num_frames stereo 16-bit LE PCM frames into out (interleaved L, R).
// Internally used by the PsyZ Audio subsystem, it can also be used for offline
// rendering and unit tests.
void Psyz_SpuPullSamples(short* out, int num_frames);

// Initialize the audio subsystem: open the host audio device and the SPU
// mixer. Idempotent. Returns 0 on success, -1 on failure.
int Psyz_AudioInit(void);

// Shut down the audio subsystem and release its resources.
// Typically used to undo the audio subsystem init for audio offline rendering.
void Psyz_AudioDestroy(void);

// Pause the host audio device so it stops pulling samples from the SPU.
// Psyz_AudioUnpause must be called to resume.
void Psyz_AudioPause(void);

// Resume audio playback after Psyz_AudioPause.
void Psyz_AudioUnpause(void);

// Acquire the audio mutex. Intended for tests and offline rendering that need
// to suspend the SDL audio callback while pulling samples directly. Must be
// paired with Psyz_AudioUnlock.
void Psyz_AudioLock(void);

// Release the audio mutex acquired with Psyz_AudioLock.
void Psyz_AudioUnlock(void);

// Set path to CUE file, simulating a CD loaded.
// Passing a NULL will unset a previously set disk path.
// Returns: 0 on success, otherwise CUE parsing failed
int Psyz_SetDiskPath(const char* diskPath);

// Simulate CD-ROM drive shell (lid) opening or closing.
void Psyz_CdShellOpen(int is_open);

// Pull PCM audio samples from the CD into the output buffer as interleaved
// stereo frames. In XA mode, ADPCM sectors are decoded before being returned.
// CdMix attenuation has already been applied to the output. Called internally
// from libcd, and typically not used by the user. Returns the number of
// frames read.
size_t Psyz_CdPullSamples(short* out, size_t num_frames);

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

/**
 * @brief Synchronize with vertical blank
 *
 * Synchronize with the refresh rate mode set in Psyz_SetVsyncMode.
 * The interface is very similar to libetc VSync.
 *
 * @param mode Synchronization mode:
 *             - 0: Wait for next vertical blank
 *             - Negative: Return immediately (non-blocking)
 *             - Positive: Wait for specified number of vertical blanks
 * @return Number of vertical blanks since last call
 */
int Psyz_VSync(int mode);

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

/**
 * @brief Write a GP0 word to the internal GPU queue
 *
 * GP0 (1F801810h) is the PS1 rendering port responsible for drawing primitives
 * and configuring GPU state. On real hardware GP0 words are consumed
 * asynchronously by the GPU FIFO. Here they are buffered in an internal queue
 * and may only flushed when Psyz_GpuExeque is called depending on backend.
 * https://problemkaputt.de/psxspx-graphics-processing-unit-gpu.htm
 *
 * @param word GP0 command word
 */
void Psyz_GpuWriteGP0(unsigned int word);

/**
 * @brief Flush the internal GP0 queue and render to screen
 *
 * Processes all GP0 words accumulated via Psyz_GpuWriteGP0 and submits them
 * to the GPU backend. Depending on the backend, this call may block until all
 * queued packed are consumed, or return immediately. When synchronization is
 * desired, poll until the queue is fully drained:
 * `while ((ret = Psyz_GpuExeque()) > 0);`
 *
 * @return 0: all commands rendered.
 *         Positive: number of commands still pending.
 *         Negative: operation failed.
 */
int Psyz_GpuExeque(void);

/**
 * @brief Write a GP1 word to control the display
 *
 * GP1 (1F801814h) is the PS1 display control port responsible for configuring
 * the display output. GP1 words sent via this endpoint take effect immediately.
 * Depending on the backend, certain commands may be silently ignored if the
 * target does not support the requested property, such as specific resolutions
 * or interlaced modes.
 * https://problemkaputt.de/psxspx-gpu-display-control-commands-gp1.htm
 *
 * @param word GP1 command word
 */
void Psyz_GpuDisplayCommand(unsigned int word);

#ifdef __cplusplus
}
#endif

#endif
