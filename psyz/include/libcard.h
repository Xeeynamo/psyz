#ifndef LIBCARD_H
#define LIBCARD_H

/**
 * @file libcard.h
 * @brief Memory Card Low-Level Library
 *
 * This library provides low-level Memory Card functions used by the kernel.
 * These functions are lower-level than libmcrd and are typically called
 * internally. For application development, use libmcrd instead.
 *
 * Key functions:
 * - InitCARD() - Initialize Memory Card driver
 * - StartCARD() - Start Memory Card system
 * - StopCARD() - Stop Memory Card system
 * - _card_info() - Get Memory Card information
 * - _card_load() - Load Memory Card directory
 * - _card_clear() - Clear Memory Card
 * - _card_write() - Write block to Memory Card
 * - _card_read() - Read block from Memory Card
 *
 * Note: Most applications should use the higher-level libmcrd library instead.
 */

#include <types.h>

/**
 * @brief Initialize Memory Card driver
 *
 * Initializes the Memory Card driver.
 *
 * @param val Use of control routine in ROM (0: do not use, 1: use)
 */
void InitCARD(long val);

/**
 * @brief Start Memory Card system
 *
 * Starts the Memory Card system.
 *
 * @return Status code
 */
long StartCARD(void);

/**
 * @brief Stop Memory Card system
 *
 * Stops the Memory Card system.
 *
 * @return Status code
 */
long StopCARD(void);

/**
 * @brief Initialize Memory Card file system
 *
 * Initializes the Memory Card file system. This file system is not initialized
 * automatically, so it is necessary to call this function.
 *
 * This function must be called after InitCARD() and StartCARD().
 */
void _bu_init(void);

/**
 * @brief Set automatic format function
 *
 * When val is 0, the automatic format function is disabled; when val is 1,
 * it is enabled. This function should be used for testing purposes only.
 *
 * @param val Indicates automatic formatting (0: disabled, 1: enabled)
 * @return Previously set automatic format value
 */
long _card_auto(long val);

/**
 * @brief Get a Memory Card BIOS event
 *
 * Returns the device number of the Memory Card that just generated an event.
 *
 * @return 2-digit hex device number
 */
long _card_chan(void);

/**
 * @brief Clear unconfirmed flags
 *
 * Performs a dummy write to the system management area of the card and clears
 * the card's unconfirmed flags.
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * This function executes asynchronously, so it returns immediately. Processing
 * completion is communicated by an event (HwCARD/EvSpIOE, HwCARD/EvSpTIMOUT,
 * HwCARD/EvSpNEW, HwCARD/EvSpERROR, or HwCARD/EvSpUNKOWN).
 *
 * @param chan Port number x 16 + Card number
 * @return 1 if registration successful, otherwise 0
 */
long _card_clear(long chan);

/**
 * @brief Format the Memory Card
 *
 * Formats the Memory Card. When calculating chan, "port number" is 0 for
 * Port 1 and 1 for Port 2. "Card number" is zero when a standard controller
 * is connected, and may be in the range 0-3 if a Multi Tap is connected.
 *
 * Does not enter critical section. Synchronous functions are blocked for
 * approximately 144 Vsync.
 *
 * @param chan Port number x 16 + Card number
 * @return 1 if formatting is successful, otherwise 0
 */
long _card_format(long chan);

/**
 * @brief Get card status
 *
 * Tests the connection of the Memory Card specified in chan.
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * This function executes asynchronously, so it returns immediately. Processing
 * completion is communicated by an event (SwCARD/EvSpIOE, SwCARD/EvSpTIMOUT,
 * SwCARD/EvSpNEW, or SwCARD/EvSpERROR).
 *
 * Do not use _new_card() to suppress EvSpNEW.
 *
 * @param chan Port number x 16 + Card number
 * @return 1 if registration successful, otherwise 0
 */
long _card_info(long chan);

/**
 * @brief Test logical format
 *
 * Reads file management information for the card specified by chan in the
 * file system in order to get asynchronous access using the I/O management
 * service.
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * _card_load() must be called at least once before you can use open() on a
 * Memory Card file in O_NOWAIT mode. It does not have to be called again
 * unless a card is changed.
 *
 * This function executes asynchronously, so it returns immediately. Processing
 * completion is communicated by an event (SwCARD/EvSpIOE, SwCARD/EvSpTIMOUT,
 * SwCARD/EvSpNEW, or SwCARD/EvSpERROR).
 *
 * @param chan Port number x 16 + Card number
 * @return 1 if the read is successful, otherwise 0
 */
long _card_load(long chan);

/**
 * @brief Read one block from the Memory Card
 *
 * Reads 128 bytes of buffer data into buf from the target block number (block)
 * of the Memory Card of the specified channel (chan).
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * This function executes asynchronously so it returns immediately after
 * completion. Actual processing termination is communicated by an event
 * (HwCARD/EvSpIOE, HwCARD/EvSpTIMOUT, HwCARD/EvSpNEW, HwCARD/EvSpERROR, or
 * HwCARD/EvSpUNKOWN). Multiplex processing to the same card slot can't be
 * performed.
 *
 * This function exists within the low-level interface and is one of the
 * special functions used for testing.
 *
 * @param chan Port number x 16 + card number
 * @param block Target block number
 * @param buf Pointer to 128 byte data buffer
 * @return 1 if successful processing registration, otherwise 0
 */
long _card_read(long chan, long block, long* buf);

/**
 * @brief Get Memory Card BIOS status
 *
 * Gets the Memory Card BIOS status of each slot, drv. Specify drv as 0 for
 * Port 1, 1 for Port 2.
 *
 * This is a synchronous function.
 *
 * If the Memory Card BIOS is in run state, it can return any of the following
 * values:
 * - 0x01: Idle processing
 * - 0x02: READ processing
 * - 0x04: WRITE processing
 * - 0x08: Connection test processing registration
 * - 0x11: No registered processing (just prior to EvSpTIMOUT generation)
 * - 0x21: No registered processing (just prior to EvSpERROR generation)
 *
 * @param drv Port number (0: Port 1, 1: Port 2)
 * @return Memory Card BIOS status value
 */
long _card_status(long drv);

/**
 * @brief Wait for Memory Card BIOS completion
 *
 * Wait until registration processing completes for the drv slot. Specify drv
 * as 0 for Port 1, 1 for Port 2.
 *
 * @param drv Sets slot number (0: Port 1, 1: Port 2)
 * @return Always 1
 */
long _card_wait(long drv);

/**
 * @brief Write to one block of the Memory Card (for testing only)
 *
 * Writes 128 bytes of buffer data pointed to by buf to the target block number
 * (block) of the Memory Card of the specified channel (chan).
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * This function executes asynchronously, so it returns immediately. Actual
 * processing termination is communicated by an event (HwCARD/EvSpIOE,
 * HwCARD/EvSpTIMOUT, HwCARD/EvSpNEW, HwCARD/EvSpERROR, or HwCARD/EvSpUNKOWN).
 * Multiplex processing to the same card slot can't be performed.
 *
 * This is a low-level function that should be used for testing only. It
 * bypasses the memory card file system; therefore, in a released product,
 * use the C file-handling routines such as write().
 *
 * @param chan Port number x 16 + card number
 * @param block Target block number
 * @param buf Pointer to 128-byte data buffer
 * @return 1 if registration successful, otherwise 0
 */
long _card_write(long chan, long block, long* buf);

/**
 * @brief Change settings of unconfirmed flag test
 *
 * Masks the generation of an EvSpNEW event immediately after _card_read() or
 * _card_write().
 *
 * Terminates immediately even though it is a synchronous function.
 */
void _new_card(void);

#endif /* LIBCARD_H */
