#ifndef PSYZ_H
#define PSYZ_H

/**
 * @file psyz.h
 * @brief Main header to include when re-compiling a PS1 application targeting
 *        foreign platforms. It contains all publicly exposed APIs from PsyZ.
 *
 * All public API of PsyZ use the prefix `Psyz_`, while type use prefix `Psyz`.
 *
 * Functions are guaranteed to be platform-agnostic and exist for the sole
 * purpose of emulating certain PlayStation hardware characteristics. Typically
 * this is a lightweight layer that decodes hardware registers, manages state,
 * and ingests commands. Events are then dispatched to the relevant subsystem
 * backend. Examples: `Psyz_Cd`, `Psyz_Spu`, etc.
 *
 * APIs that implement the backend of a specific subsystem for the target
 * platform use the prefix `Psyz_{subsystem}`. These are the lowest layer of
 * abstraction of PsyZ, and they generally communicate with the platform
 * library or the hardware directly. When targeting a new platform, all the
 * subsystem endpoints are required to be implemented to guarantee full
 * functionality. All micro-optimizations and the use of hardware-specific
 * quirks live here. Examples: `Psyz_Audio`, `Psyz_Video`, etc.
 *
 * A typical game running on PsyZ is structured as follows:
 *    Game ->
 *        PSY-Q decompiled APIs ->
 *            PsyZ platform agnostic PS1 hardware emulation ->
 *                PsyZ platform-specific subsystems ->
 *                    Hardware, Driver or Operating System
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <psyz/platform.h>
#include <psyz/types.h>
#include <psyz/assert.h>
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

/**
 * @brief Set the title of the game window
 *
 * @param str Window title string
 */
void Psyz_SetTitle(const char* str);

struct PsyzDiskRead {
    unsigned int sector;  /**< disk sector, can be used as a unique file id */
    unsigned int size;    /**< byte count required to be read */
    void* buffer;         /**< buffer to read into, max index is buffer[size-1] */
};

/**
 * @brief Callback invoked when a disk read is triggered
 *
 * @param read Read request describing sector, size and destination buffer
 * @return Bytes read; -1 if not found, unsuccessful or not implemented
 */
typedef int (*PsyzDiskReadCB)(struct PsyzDiskRead* read);

/**
 * PS1 SPU constants (the SPU emulator is platform-agnostic; SDL or any other
 * audio backend pulls 44100 Hz stereo short frames from Psyz_SpuPullSamples).
 */
#define PSYZ_SPU_RAM_SIZE (512 * 1024) /**< must be a power of two */
#define PSYZ_SPU_NUM_VOICES 24         /**< match PS1 voice count */
#define PSYZ_SPU_SAMPLE_RATE 44100     /**< match fixed PS1 sample rate */

/**
 * @brief Advance the reserved SEQ playback timer
 *
 * Reserved. Used by the audio thread to emulate the timer for SEQ playback in
 * absence of an rcnt interrupt. Always assumes the Audio driver plays at
 * 44100Hz.
 *
 * @param n Number of ticks to advance
 */
void Psyz_RcntAdd(int n);

/**
 * @brief Initialize SPU emulation state
 *
 * Idempotent; safe to call multiple times.
 */
void Psyz_SpuInit(void);

/**
 * @brief Reset SPU state
 *
 * @param hot When non-zero, RAM contents are preserved across the reset
 *            (mirrors the PSX-Q "hot init" semantics for libspu).
 */
void Psyz_SpuReset(int hot);

/**
 * @brief Write one 16-bit value into the SPU register file
 *
 * Certain registers can trigger a side-effect. Please refer to psxspx docs for
 * SPU reference.
 *
 * @param reg_offset Offset relative to 0x1F801C00; valid range 0x000-0x1FF
 * @param value 16-bit value to write
 */
void Psyz_SpuWrite(unsigned int reg_offset, unsigned short value);

/**
 * @brief Read back one 16-bit value from the SPU register file
 *
 * Internally maps to SPU_RXX.
 *
 * @param reg_offset Offset relative to 0x1F801C00; valid range 0x000-0x1FF
 * @return 16-bit register value
 */
unsigned short Psyz_SpuRead(unsigned int reg_offset);

/**
 * @brief Read back the current SPU transfer address
 *
 * Maps to the xfer_addr register.
 *
 * @return Current transfer address
 */
unsigned int Psyz_SpuGetTransferAddr(void);

/**
 * @brief Set the SPU RAM transfer address
 *
 * The destination offset into the 512KB RAM. Maps to the PSX SPU register
 * 0x1F801DA6 (xfer_addr) divided by 8.
 *
 * @param addr Transfer address
 */
void Psyz_SpuSetTransferAddr(unsigned int addr);

/**
 * @brief Push one 16-bit word into the SPU transfer FIFO
 *
 * Maps to a write of SPU register 0x1F801DA8 (xfer_fifo). Each call deposits
 * the word at the current transfer address in SPU RAM and bumps the transfer
 * address by 2.
 *
 * @param word 16-bit word to push
 */
void Psyz_SpuFifoWrite(unsigned short word);

/**
 * @brief Faster bulk version of Psyz_SpuFifoWrite
 *
 * Bypasses individual writes. Uses xfer_addr as the destination address and
 * updates it at the end of the call.
 *
 * @param src Source buffer
 * @param size Number of bytes to transfer
 */
void Psyz_SpuFifoWriteBulk(const unsigned char* src, unsigned int size);

/**
 * @brief Read bytes from SPU RAM
 *
 * Wraps at 512 KB. Does not affect xfer_addr. Useful for debugging.
 *
 * @param offset Byte offset into SPU RAM
 * @param dst Destination buffer
 * @param size Number of bytes to read
 */
void Psyz_SpuMemRead(unsigned int offset, void* dst, unsigned int size);

/**
 * @brief Write bytes into SPU RAM
 *
 * Wraps at 512 KB. Does not affect xfer_addr. Useful for debugging.
 *
 * @param offset Byte offset into SPU RAM
 * @param src Source buffer
 * @param size Number of bytes to write
 */
void Psyz_SpuMemWrite(unsigned int offset, const void* src, unsigned int size);

/**
 * @brief Direct pointer to the 512 KB SPU RAM
 *
 * For tests and offline rendering.
 *
 * @return Pointer to SPU RAM
 */
unsigned char* Psyz_SpuGetRam(void);

/**
 * @brief Generate stereo 16-bit LE PCM frames into out (interleaved L, R)
 *
 * Internally used by the PsyZ Audio subsystem, it can also be used for offline
 * rendering and unit tests.
 *
 * @param out Output buffer
 * @param num_frames Number of stereo frames to generate
 */
void Psyz_SpuPullSamples(short* out, int num_frames);

/**
 * @brief Initialize the audio subsystem
 *
 * Opens the host audio device and the SPU mixer. Idempotent.
 *
 * @return 0 on success, -1 on failure
 */
int Psyz_AudioInit(void);

/**
 * @brief Shut down the audio subsystem and release its resources
 *
 * Typically used to undo the audio subsystem init for audio offline rendering.
 */
void Psyz_AudioDestroy(void);

/**
 * @brief Pause the host audio device so it stops pulling samples from the SPU
 *
 * Psyz_AudioUnpause must be called to resume.
 */
void Psyz_AudioPause(void);

/**
 * @brief Resume audio playback after Psyz_AudioPause
 */
void Psyz_AudioUnpause(void);

/**
 * @brief Acquire the audio mutex
 *
 * Intended for tests and offline rendering that need to suspend the SDL audio
 * callback while pulling samples directly. Must be paired with
 * Psyz_AudioUnlock.
 */
void Psyz_AudioLock(void);

/**
 * @brief Release the audio mutex acquired with Psyz_AudioLock
 */
void Psyz_AudioUnlock(void);

/**
 * @brief Set path to CUE file, simulating a CD loaded
 *
 * Passing a NULL will unset a previously set disk path.
 *
 * @param diskPath Path to the CUE file, or NULL to unset
 * @return 0 on success, otherwise CUE parsing failed
 */
int Psyz_SetDiskPath(const char* diskPath);

/**
 * @brief Simulate CD-ROM drive shell (lid) opening or closing
 *
 * @param is_open Non-zero to open the shell, zero to close it
 */
void Psyz_CdShellOpen(int is_open);

/**
 * @brief Pull PCM audio samples from the CD as interleaved stereo frames
 *
 * In XA mode, ADPCM sectors are decoded before being returned. CdMix
 * attenuation has already been applied to the output. Called internally from
 * libcd, and typically not used by the user.
 *
 * @param out Output buffer
 * @param num_frames Number of stereo frames requested
 * @return Number of frames read
 */
size_t Psyz_CdPullSamples(short* out, size_t num_frames);

/**
 * @brief Set callback invoked when a disk read is triggered
 *
 * If cb is NULL or returns a negative value, PSY-Z falls back to CD emulation.
 *
 * @param cb Disk read callback, or NULL to clear
 */
void Psyz_SetDiskReadCB(PsyzDiskReadCB cb);

typedef enum {
    /**
     * Auto-detect use of driver VSync (default)
     * - Tests monitor refresh rate on initialization (adds ~30ms startup time)
     * - Uses driver VSync if monitor is detected between 57Hz-63Hz
     *   (±5% tolerance). This covers both 59.94Hz (NTSC) and 60Hz
     * - Uses manual limiter otherwise (144Hz, 165Hz, etc.)
     * - Balances performance and accuracy automatically
     */
    PSYZ_VSYNC_AUTO,

    /**
     * Always use driver VSync
     * Pros: Zero CPU overhead and no frame pacing on matching refresh rates
     * Cons: Game will run faster on high refresh monitors
     * Cons: Framerate always matches monitor exactly.
     *   On 60Hz monitor: runs at 60fps instead of NTSC 59.94fps
     *   causing ~1 second drift every ~16.7 minutes of gameplay
     * Real NTSC PSX hardware runs at 59.94Hz (60/1.001)
     *   PAL PSX hardware runs at exactly 50Hz
     * Best when combined with driver control panel frame rate limit
     * IMPORTANT: PAL games (50fps) will run at 60fps on 60Hz monitors without
     *   driver-level limiting, which is incorrect
     */
    PSYZ_VSYNC_ON,

    /**
     * Always use internal manual frame limiter
     * Pros: Precise 59.94fps (NTSC) / 50fps (PAL) matching real hardware
     * Pros: Safe for VRR displays
     * Pros: Consistent timing across all monitor refresh rates
     * Cons: ~6% CPU usage on one core (1ms busy-wait per frame for precision)
     * Cons: May have minor frame pacing variance on non-VRR displays
     */
    PSYZ_VSYNC_OFF,
} PsyzVsyncMode;

typedef struct {
    double last_frame_time_us;       /**< duration of last frame */
    double last_draw_time_us;        /**< render time excluding vsync wait */
    double target_frame_time_us;     /**< target frame time */
    unsigned long long total_frames; /**< total frames rendered */
    int using_driver_vsync;          /**< 1 for VSync, 0 for limiter */
} PsyzGpuStats;

/**
 * @brief Set VSync mode (default: AUTO)
 *
 * @param mode VSync mode to set
 * @return 0 on success, -1 if invalid mode
 */
int Psyz_SetVsyncMode(PsyzVsyncMode mode);

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

/**
 * @brief DMA channel identifiers
 */
typedef enum {
    DMA_CHANNEL_MDEC_IN,  /**< MDEC input */
    DMA_CHANNEL_MDEC_OUT, /**< MDEC output */
    DMA_CHANNEL_GPU,      /**< GPU */
    DMA_CHANNEL_CD,       /**< CD-ROM */
    DMA_CHANNEL_SPU,      /**< SPU */
    DMA_CHANNEL_PIO,      /**< PIO (expansion port) */
    DMA_CHANNEL_OTC,      /**< Ordering table clear */
} PsyzDmaChannel;

/**
 * @brief Emulate DMA
 *
 * Emulates registers between 0x1F801080 to 0x1F8010EC. The MMIO pointer is
 * calculated with 0x1F801080 + ch * 0x10 + offset * 4
 *
 * @param ch Channel to write to
 * @param offset can be either 0, 1 or 2
 * @param value to write to; can be a raw pointer
 */
void Psyz_DmaWrite(PsyzDmaChannel ch, unsigned offset, u_long value);

/**
 * @brief Get frame timing statistics
 *
 * @param stats Output structure to fill
 * @return 0 on success, -1 if stats is NULL or platform not initialized
 */
int Psyz_GetGpuStats(PsyzGpuStats* stats);

/**
 * @brief Get frame output as a byte array
 *
 * This function is very slow.
 *
 * @param w Output frame width
 * @param h Output frame height
 * @return NULL on failure, otherwise ptr to be destroyed with free(ptr)
 */
unsigned char* Psyz_AllocAndCaptureFrame(int* w, int* h);

/**
 * @brief Adjust a PlayStation 1 path to the host filesystem
 *
 * Handles memory card paths (bu00:, bu10:, etc.) and other special cases.
 * If a custom callback is set via Psyz_AdjustPathCB and returns >= 0, the
 * callback result is used; otherwise internal adjustment is applied. The
 * function is used internally when opening and creating files, or when
 * enumerating the list of files in the specified directory.
 *
 * @note The filename portion (after the last path separator) is automatically
 *       truncated to 19 characters to match PS1 DIRENTRY.name[20] which
 *       requires null-termination for compatibility with SDK string functions.
 *       This truncation is applied regardless of whether callback or internal
 *       adjustment is used.
 *
 * @param dst Destination buffer for adjusted path (must be valid)
 * @param src Source path to adjust (PlayStation 1 format)
 * @param maxlen Maximum length of destination buffer
 */
void Psyz_AdjustPath(char* dst, const char* src, int maxlen);

/**
 * @brief Set custom path adjustment callback
 *
 * The callback is invoked before internal path adjustment. Its return value:
 * - < 0: No adjustment done, fall back to PSY-Z internal adjustment
 * - >= 0: Number of bytes written to dst (adjustment successful)
 *
 * The callback receives:
 * - dst: Destination buffer for adjusted path
 * - src: Source path to adjust
 * - maxlen: Maximum length of destination buffer
 *
 * @param callback Path adjustment callback
 */
void Psyz_AdjustPathCB(int (*callback)(char* dst, const char* src, int maxlen));

/**
 * @brief Join two path components with the platform's path separator
 *
 * Automatically handles path separators to avoid double separators.
 *
 * @param left Left path component (modified in-place)
 * @param right Right path component to append
 * @param maxlen Maximum length of left buffer
 * @return Pointer to left on success, NULL if buffer too small
 */
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

/**
 * @brief Read a GTE data register (COP2 data)
 *
 * The PS1 GTE (COP2) exposes 32 data registers holding input vectors,
 * accumulators, screen XY/Z FIFOs, and MAC values. Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @return Register value packed as a 32-bit word
 */
unsigned int Psyz_GteDataRead(unsigned reg);

/**
 * @brief Write a GTE data register (COP2 data)
 *
 * Writes a 32-bit word into the GTE data register file. Used by an external
 * emulator to forward MTC2/LWC2 instructions into PsyZ's GTE state.
 * Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @param value Value to write
 */
void Psyz_GteDataWrite(unsigned reg, unsigned int value);

/**
 * @brief Read a GTE control register (COP2 control)
 *
 * The GTE control registers hold transformation matrices, translation vectors,
 * projection parameters, and the FLAG register. Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @return Register value packed as a 32-bit word
 */
unsigned int Psyz_GteCtrlRead(unsigned reg);

/**
 * @brief Write a GTE control register (COP2 control)
 *
 * Writes a 32-bit word into the GTE control register file. Used by an external
 * emulator to forward CTC2 instructions into PsyZ's GTE state.
 * Register indices follow
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param reg Register index (0-31)
 * @param value Value to write
 */
void Psyz_GteCtrlWrite(unsigned reg, unsigned int value);

/**
 * @brief Execute a GTE command (COP2 instruction)
 *
 * Dispatches a 25-bit GTE command to the corresponding operation. Bits 0-5
 * select the operation (e.g. RTPS, RTPT, NCLIP, AVSZ3, AVSZ4). Used by an
 * external emulator to forward COP2 instructions into PsyZ's GTE state.
 * https://psx-spx.consoledev.net/geometrytransformationenginegte/
 *
 * @param cmd 25-bit GTE command word
 */
void Psyz_GteCommand(unsigned int cmd);

/**
 * @brief Controller identifiers to emulate multiple PS1 controller kinds
 *
 * https://problemkaputt.de/psxspx-controllers-communication-sequence.htm
 */
typedef enum {
    PSYZ_CTRL_ERROR = 0x00,         /**< Failed to set controller param */
    PSYZ_CTRL_QUERY_KIND = 0x00,    /**< Query current kind without changing it */
    PSYZ_CTRL_MOUSE = 0x12,         /**< TODO document buffer format */
    PSYZ_CTRL_DIGITAL_PAD = 0x41,   /**< TODO document buffer format */
    PSYZ_CTRL_ANALOG_STICK = 0x53,  /**< TODO document buffer format */
    PSYZ_CTRL_ANALOG_PAD = 0x73,    /**< TODO document buffer format */
    PSYZ_CTRL_KEYBOARD = 0x96,      /**< TODO document buffer format */
    PSYZ_CTRL_DISCONNECTED = 0xFF,  /**< TODO */
} PsyzControllerKind;

/**
 * @brief Select which controller kind a port emulates
 *
 * Sets the controller kind reported on @p port and @p channel.
 * Defaults to PSYZ_CTRL_DIGITAL_PAD for better compatibility with early games.
 * When the reserved PSYZ_CTRL_QUERY_KIND value is passed, fetch the current
 * controller kind for the specified @p port and @p channel as returned value.
 *
 * @param port Controller port (0 or 1)
 * @param channel Multitap channel (0-3). Currently reserved and unused.
 * @param kind Controller kind to emulate, or PSYZ_CTRL_QUERY_KIND
 * @return Previously selected kind, or PSYZ_CTRL_ERROR on invalid arguments
 */
PsyzControllerKind Psyz_SetController(
    int port, int channel, PsyzControllerKind kind);

/**
 * Size of the controller receive buffer the BIOS fills on real hardware:
 * byte 0 = status, byte 1 = controller kind, then the per-kind payload.
 * https://problemkaputt.de/psxspx-controllers-and-memory-cards.htm
 */
#define PSYZ_PAD_BUF_LEN 34

/**
 * @brief Retrieve the internal 34-byte controller frame for a port
 *
 * Copies the latest hardware-faithful SIO frame for @p port into @p dst.
 * Direct low-level entrypoint for libetc/libapi. Depending on the backend
 * implementation, triggers an OS poll of gamepad or mapped keyboard.
 *
 * @param port Controller port (0 or 1)
 * @param dst Buffer to write into
 * @param len Buffer length in bytes (at most PSYZ_PAD_BUF_LEN is copied)
 */
void Psyz_PadsGet(int port, char* dst, int len);

/**
 * @brief Set the internal 34-byte controller frame.
 *
 * Called by the platform input backend to store the same frame retrieved with
 * Psyz_PadsGet. It can be used to emulate input as part of automated tests.
 *
 * @param port Controller port (0 or 1)
 * @param src Buffer to write from
 * @param len Buffer length in bytes
 */
void Psyz_PadsSet(int port, const char* src, int len);

#ifdef __cplusplus
}
#endif

#endif
