#ifndef PSYZ_BOOTLOGO_H
#define PSYZ_BOOTLOGO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    short width;                 /**< 256 to 640, 0 selects 320 */
    short height;                /**< 240 (default), or 480 for interlaced */
    unsigned char jingle_off;    /**< non-zero mutes the jingle */
    unsigned char no_easter_egg; /**< to disable the logo interaction */
    unsigned char frame_step;    /**< non-zero for debugging frame-by-frame */
    unsigned char RESERVED_1;    /**< RESERVED */
    int time_max;                /**< bootlogo duration, defaults to 4500ms */
    int (*skip_cb)(int time);    /**< overrides skip logic; return 1 to skip */
} PsyzBootlogoConfig;

/**
 * @brief Run the boot logo until it ends or is skipped.
 *
 * Without a skip_cb, Start and Select skip, and so do Cross, Circle, Triangle
 * and Square unless frame_step is set. The D-Pad, L1, R1, L2 and R2 never skip.
 * With a skip_cb, only the callback skips.
 *
 * @param config Configure bootlogo behaviour.
 * @return 1 when skipped, 0 when it ends naturally
 */
int Psyz_Bootlogo(PsyzBootlogoConfig config);

#ifdef __cplusplus
}
#endif

#endif
