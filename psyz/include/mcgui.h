#ifndef MCGUI_H
#define MCGUI_H

#include <types.h>

/**
 * @brief BG data structure
 *
 * Part of the McGuiEnv structure.
 */
struct sMcGuiBg {
    short mode; /**< BG mode: 0=Scroll mode (64x64 tiled-texture), 1=Still-image
                   mode */
    signed char scrollDirect; /**< Scroll direction (valid when BG mode=0):
                                 0=up, 1=top left, 2=left, 3=bottom left,
                                 4=down, 5=bottom right, 6=right, 7=top right */
    signed char scrollSpeed;  /**< Scroll speed (valid when BG mode=0):
                                 0=Stopped, 1=1/60 sec, 2=1/30 sec, 3=1/20 sec */
    u_long*
        timadr; /**< Header address of TIM data for BG (valid when BG mode=1) */
};

/**
 * @brief Memory Card data structure
 *
 * This structure holds information used for loading and saving game data.
 * Part of the McGuiEnv structure.
 */
struct sMcGuiCards {
    char file[21];  /**< File name (ASCII only, except 0x00, 0x2a(*), 0x3f(?),
                       terminated by 0x00) */
    char title[65]; /**< Document name (full-size 32 characters of SJIS
                       non-kanji and level-1 kanji, terminated by 0x00) */
    char frame;     /**< Reserved area (unusable) */
    char block;     /**< Number of icon images (automatically animated): 1-3 */
    u_long* iconAddr; /**< TIM data header address for icon image */
    u_long* dataAddr; /**< Header Address of game data */
    long dataBytes; /**< Number of game data bytes (128-byte units, 1-15 blocks)
                     */
};

/**
 * @brief Controller data structure
 *
 * This structure holds controller information. It is part of the McGuiEnv
 * structure. Set the flag members of the supported controller types to 1, and
 * set the respective button codes in the BUTTON_OK/BUTTON_CANCEL fields. Set
 * the flag members of unsupported controller types to 0.
 */
struct sMcGuiController {
    volatile u_char* buf[2]; /**< Controller's receive data buffer */
    struct {
        int flag;             /**< Controller type enabled flag */
        u_long BUTTON_OK;     /**< OK button code */
        u_long BUTTON_CANCEL; /**< Cancel button code */
    } type1;                  /**< Default controller */
    struct {
        int flag;             /**< Controller type enabled flag */
        u_long BUTTON_OK;     /**< OK button code */
        u_long BUTTON_CANCEL; /**< Cancel button code */
    } type2;                  /**< Mouse */
    struct {
        int flag;             /**< Controller type enabled flag */
        u_long BUTTON_OK;     /**< OK button code */
        u_long BUTTON_CANCEL; /**< Cancel button code */
    } type3;                  /**< Analog joystick, DUAL SHOCK */
    struct {
        int flag;             /**< Controller type enabled flag */
        u_long BUTTON_OK;     /**< OK button code */
        u_long BUTTON_CANCEL; /**< Cancel button code */
    } type4;                  /**< NeGcon */
};

/**
 * @brief Cursor data structure
 *
 * This structure sets the color and shape of the menu cursor. It is part of the
 * McGuiEnv structure. Note: type is currently not supported.
 */
struct sMcGuiCursor {
    char type; /**< Cursor shape (not supported) */
    u_char r;  /**< Color code (0-255) */
    u_char g;  /**< Color code (0-255) */
    u_char b;  /**< Color code (0-255) */
};

/**
 * @brief BGM and sound effects data structure
 *
 * This structure contains information about the BGM that plays on the Memory
 * Card screen and the sound effect that is produced when the cursor is moved,
 * etc.
 */
struct sMcGuiSnd {
    int MVOL;        /**< Main volume (0-127) */
    int isReverb;    /**< 0: reverb OFF, 1: reverb ON */
    int reverbType;  /**< Reverb type */
    int reverbDepth; /**< Reverb depth (0-127) */
    struct {
        int isbgm;   /**< 0: no BGM, 1: BGM */
        u_long* seq; /**< Header address of SEQ data */
        u_long* vh;  /**< Header address of VH data for SEQ */
        u_long* vb;  /**< Header address of VB data for SEQ */
        int SVOL;    /**< SEQ volume (0-127) */
    } bgm;
    struct {
        int isse;        /**< 0: no sound effects, 1: sound effects */
        u_long* vh;      /**< Header address of VH data for SE */
        u_long* vb;      /**< Header address of VB data for SE */
        int vol;         /**< Sound effects volume */
        int prog;        /**< Sound effects program number */
        int TONE_OK;     /**< Tone number of confirmation/execution tone */
        int TONE_CANCEL; /**< Tone number of CANCEL tone */
        int TONE_CURSOR; /**< Tone number during cursor operation */
        int TONE_ERROR;  /**< Tone number of error tone */
    } se;
};

/**
 * @brief Texture data structure
 *
 * This structure specifies the texture data used internally by the module. It
 * is part of the McGuiEnv structure.
 */
struct sMcGuiTexture {
    u_long* addr; /**< Header address of TIM data */
};

/**
 * @brief Memory Card GUI module structure
 *
 * This is the main structure used by the memory card GUI module.
 * Each member is a separate structure.
 */
struct McGuiEnv {
    struct sMcGuiCards cards; /**< Memory Card data structure */
    struct sMcGuiBg bg;       /**< BG data structure */
    struct sMcGuiController
        controller;               /**< Controller-related data structure */
    struct sMcGuiSnd sound;       /**< BGM and sound effect data structure */
    struct sMcGuiTexture texture; /**< Texture data structure */
    struct sMcGuiCursor cursor;   /**< Cursor data structure */
};

/**
 * @brief Load game data
 *
 * Invokes the load operation of the Memory Card screen.
 * Terminates when the load has completed or when the cancel button is clicked
 * on the Slot Selection screen.
 *
 * @param env Memory Card GUI module structure
 * @return 1 after the load operation completes,
 *         0 if the cancel button was used to terminate the load,
 *         -1 if an invalid value is specified in the McGuiEnv structure
 */
int McGuiLoad(struct McGuiEnv* env);

/**
 * @brief Save game data
 *
 * Invokes the save operation of the Memory Card screen.
 * Terminates when the save is completed or when the cancel button is clicked on
 * the Slot Selection screen.
 *
 * @param env Memory Card GUI module structure
 * @return 1 after the save operation completes,
 *         0 if the cancel button was used to terminate the save,
 *         -1 if an invalid value is specified in the McGuiEnv structure
 */
int McGuiSave(struct McGuiEnv* env);

/**
 * @brief Sets English mode
 *
 * Switches the messages used on the Memory Card screen to English. For display
 * in Japanese, this function should not be executed.
 *
 * @param env Memory Card GUI module structure
 * @param mode Language mode (always 1)
 * @return Status code
 */
int McGuiSetExternalFont(struct McGuiEnv* env, int mode);

#endif
