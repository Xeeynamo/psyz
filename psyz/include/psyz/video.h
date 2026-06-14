#ifndef PSYZ_VIDEO_H
#define PSYZ_VIDEO_H

/**
 * @file video.h
 * @brief Window, display timing and frame presentation endpoints.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the title of the game window
 *
 * @param str Window title string
 */
void Psyz_SetTitle(const char* str);

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
} PsyzVideoStats;

/**
 * @brief Set VSync mode (default: AUTO)
 *
 * @param mode VSync mode to set
 * @return 0 on success, -1 if invalid mode
 */
int Psyz_VideoSetVsyncMode(PsyzVsyncMode mode);

/**
 * @brief Synchronize with vertical blank
 *
 * Synchronize with the refresh rate mode set in Psyz_VideoSetVsyncMode.
 * The interface is very similar to libetc VSync.
 *
 * @param mode Synchronization mode:
 *             - 0: Wait for next vertical blank
 *             - Negative: Return immediately (non-blocking)
 *             - Positive: Wait for specified number of vertical blanks
 * @return Number of vertical blanks since last call
 */
int Psyz_VideoVSync(int mode);

/**
 * @brief Get frame timing statistics
 *
 * @param stats Output structure to fill
 * @return 0 on success, -1 if stats is NULL or platform not initialized
 */
int Psyz_VideoStats(PsyzVideoStats* stats);

/**
 * @brief Get frame output as a byte array
 *
 * This function is very slow.
 *
 * @param w Output frame width
 * @param h Output frame height
 * @return NULL on failure, otherwise ptr to be destroyed with free(ptr)
 */
unsigned char* Psyz_VideoAllocCapturedFrame(int* w, int* h);

#ifdef __cplusplus
}
#endif

#endif
