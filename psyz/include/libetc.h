#ifndef LIBETC_H
#define LIBETC_H

#include <types.h>

extern int PadIdentifier;

/* PAD I/O (SIO Pad) button definitions */
#define PADLup (1 << 12)    /**< D-Pad Up */
#define PADLdown (1 << 14)  /**< D-Pad Down */
#define PADLleft (1 << 15)  /**< D-Pad Left */
#define PADLright (1 << 13) /**< D-Pad Right */
#define PADRup (1 << 4)     /**< Triangle */
#define PADRdown (1 << 6)   /**< Cross */
#define PADRleft (1 << 7)   /**< Square */
#define PADRright (1 << 5)  /**< Circle */
#define PADi (1 << 9)       /**< L1 */
#define PADj (1 << 10)      /**< R1 */
#define PADk (1 << 8)       /**< L2 */
#define PADl (1 << 3)       /**< R2 */
#define PADm (1 << 1)       /**< Start */
#define PADn (1 << 2)       /**< Select */
#define PADo (1 << 0)       /**< Analog mode (DUAL SHOCK) */
#define PADh (1 << 11)      /**< Start */
#define PADL1 PADn
#define PADL2 PADo
#define PADR1 PADl
#define PADR2 PADm
#define PADstart PADh
#define PADselect PADk

#define _PAD(x, y) ((y) << ((x) << 4))

/* Mouse button definitions */
#define MOUSEleft (1 << 3)  /**< Left mouse button */
#define MOUSEright (1 << 2) /**< Right mouse button */

/* Video modes */
#define MODE_NTSC 0 /**< NTSC video mode */
#define MODE_PAL 1  /**< PAL video mode */

/**
 * @brief System callbacks structure
 */
struct Callbacks {
    void (*unk0)(void);
    void* (*DMACallback)(int dma, void (*func)());
    void (*unk8)(void);
    int (*ResetCallback)(void);
    int (*StopCallback)(void);
    int (*VSyncCallbacks)(int ch, void (*f)());
    int (*RestartCallback)(void);
};

extern u16 D_8002C2BA;
extern struct Callbacks* D_8002D340;

/* V-Sync functions */

/**
 * @brief Wait for vertical sync
 *
 * @param mode Wait mode
 * @return Number of V-syncs
 */
int VSync(int mode);

/**
 * @brief Set V-sync callback
 *
 * @param f Callback function
 * @return Previous callback
 */
int VSyncCallback(void (*f)());

/**
 * @brief Set V-sync callbacks
 *
 * @param ch Channel
 * @param f Callback function
 * @return Status
 */
int VSyncCallbacks(int ch, void (*f)());

/* DMA and system callbacks */

/**
 * @brief Set DMA callback
 *
 * @param dma DMA channel
 * @param func Callback function
 * @return Previous callback
 */
void* DMACallback(int dma, void (*func)());

/**
 * @brief Reset callback
 *
 * @return Status
 */
int ResetCallback(void);

/**
 * @brief Stop callback
 *
 * @return Status
 */
int StopCallback(void);

/**
 * @brief Restart callback
 *
 * @return Status
 */
int RestartCallback(void);

/**
 * @brief Check callback status
 *
 * Checks the callback status.
 *
 * @return Callback status flags
 */
u16 CheckCallback(void);

/* Video mode functions */

/**
 * @brief Get current video mode
 *
 * Returns the current video mode (NTSC or PAL).
 *
 * @return MODE_NTSC or MODE_PAL
 */
long GetVideoMode(void);

/**
 * @brief Set video mode
 *
 * Sets the video mode to NTSC or PAL.
 *
 * @param mode MODE_NTSC or MODE_PAL
 * @return Previous video mode
 */
long SetVideoMode(long mode);

/* Controller/PAD functions */

/**
 * @brief Initialize controller
 *
 * Initializes the controller subsystem.
 *
 * @param mode Initialization mode
 */
void PadInit(int mode);

/**
 * @brief Initialize controller (direct mode)
 *
 * Initializes the controller in direct mode.
 *
 * @param buf1 Buffer for port 1
 * @param buf2 Buffer for port 2
 * @return Status
 */
long PadInitDirect(u_char* buf1, u_char* buf2);

/**
 * @brief Initialize Multi Tap
 *
 * Initializes the Multi Tap adapter.
 *
 * @param buf1 Buffer for port 1
 * @param buf2 Buffer for port 2
 * @return Status
 */
long PadInitMtap(u_char* buf1, u_char* buf2);

/**
 * @brief Initialize gun controller
 *
 * Initializes the gun controller.
 *
 * @param buf Buffer for gun data
 * @param size Buffer size
 * @return Status
 */
long PadInitGun(u_char* buf, int size);

/**
 * @brief Read controller state
 *
 * Reads the current state of the controller.
 *
 * @param id Controller ID
 * @return Button state flags
 */
u_long PadRead(int id);

/**
 * @brief Stop controller
 *
 * Stops the controller subsystem.
 */
void PadStop(void);

/**
 * @brief Get controller state
 *
 * Gets the state of the specified controller.
 *
 * @param id Controller ID
 * @return Controller state
 */
long PadGetState(int id);

/**
 * @brief Set controller actuation
 *
 * Sets the actuation (vibration) for DUAL SHOCK controllers.
 *
 * @param id Controller ID
 * @param align Alignment
 * @param num Number of actuators
 * @return Status
 */
long PadSetAct(int id, u_char* align, int num);

/**
 * @brief Set controller actuation with alignment
 *
 * Sets the actuation for DUAL SHOCK controllers with alignment.
 *
 * @param id Controller ID
 * @param align Alignment data
 * @return Status
 */
long PadSetActAlign(int id, u_char* align);

/**
 * @brief Set controller main mode
 *
 * Sets the main operating mode for the controller.
 *
 * @param id Controller ID
 * @param offs Offset
 * @param lock Lock mode
 * @return Status
 */
long PadSetMainMode(int id, int offs, int lock);

/**
 * @brief Get controller actuation info
 *
 * Gets actuation information for DUAL SHOCK controllers.
 *
 * @param id Controller ID
 * @param info Info array
 * @param num Number of actuators
 * @return Number of actuators
 */
long PadInfoAct(int id, int info[][2], int num);

/**
 * @brief Get controller combination info
 *
 * Gets combination information for the controller.
 *
 * @param id Controller ID
 * @param list List array
 * @param num Number of combinations
 * @return Number of combinations
 */
long PadInfoComb(int id, int list[], int num);

/**
 * @brief Get controller mode info
 *
 * Gets mode information for the controller.
 *
 * @param id Controller ID
 * @param term Terminal mode
 * @param offs Offset
 * @return Mode information
 */
long PadInfoMode(int id, int term, int offs);

/**
 * @brief Enable controller communication
 *
 * Enables communication with controllers.
 *
 * @param flag Enable flag
 */
void PadEnableCom(u_long flag);

/**
 * @brief Enable gun controller
 *
 * Enables the gun controller.
 *
 * @param flag Enable flag
 */
void PadEnableGun(u_char flag);

/**
 * @brief Remove gun controller
 *
 * Removes the gun controller.
 */
void PadRemoveGun(void);

/**
 * @brief Start controller communication
 *
 * Starts controller communication.
 */
void PadStartCom(void);

/**
 * @brief Stop controller communication
 *
 * Stops controller communication.
 */
void PadStopCom(void);

/**
 * @brief Check V-sync
 *
 * Checks if controller operations are synchronized with V-sync.
 *
 * @return V-sync status
 */
long PadChkVsync(void);

/* Multi Tap functions */

/**
 * @brief Initialize Multi Tap
 *
 * Initializes the Multi Tap adapter.
 *
 * @param buf1 Buffer for port 1
 * @param len1 Length of buffer 1
 * @param buf2 Buffer for port 2
 * @param len2 Length of buffer 2
 * @return Status
 */
long InitTAP(u_char* buf1, u_long len1, u_char* buf2, u_long len2);

/**
 * @brief Start Multi Tap
 *
 * Starts the Multi Tap.
 */
void StartTAP(void);

/**
 * @brief Stop Multi Tap
 *
 * Stops the Multi Tap.
 */
void StopTAP(void);

/**
 * @brief Enable Multi Tap
 *
 * Enables the Multi Tap.
 */
void EnableTAP(void);

/**
 * @brief Disable Multi Tap
 *
 * Disables the Multi Tap.
 */
void DisableTAP(void);

/* Gun controller functions */

/**
 * @brief Initialize gun controller
 *
 * Initializes the gun controller.
 *
 * @param buf Buffer for gun data
 * @param len Buffer length
 * @return Status
 */
long InitGUN(u_char* buf, int len);

/**
 * @brief Start gun controller
 *
 * Starts the gun controller.
 */
void StartGUN(void);

/**
 * @brief Stop gun controller
 *
 * Stops the gun controller.
 */
void StopGUN(void);

/**
 * @brief Select gun controller
 *
 * Selects the gun controller port.
 *
 * @param port Port number
 */
void SelectGUN(int port);

/**
 * @brief Remove gun controller
 *
 * Removes the gun controller.
 */
void RemoveGUN(void);

#endif
