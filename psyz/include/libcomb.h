#ifndef LIBCOMB_H
#define LIBCOMB_H

#include <types.h>

/**
 * @brief Link cable driver control
 *
 * Offers the same functionality as ioctl() to an SIO device.
 * All the macros in this library are versions of this command.
 *
 * @param cmd Command
 * @param arg Subcommand
 * @param param Argument
 * @return Depends on the control command cmd
 */
long _comb_control(u_long cmd, u_long arg, u_long param);

/**
 * @brief Initialize link cable driver
 *
 * Initializes the link cable driver.
 */
void AddCOMB(void);

/**
 * @brief Clear interrupt from expanded SIO in the driver
 *
 * If val is non-0, an interrupt from an expansion SIO in the driver is cleared.
 * This is used only when other expansion SIO drivers are also present.
 *
 * @param val Interrupt cause clear flag
 */
void ChangeClearSIO(long val);

/**
 * @brief Remove link cable driver from kernel
 *
 * Removes link cable driver from kernel.
 */
void DelCOMB(void);

/* Macro functions */

/**
 * @brief Get asynchronous communication request status
 *
 * Determines whether an asynchronous input/output request has been made.
 *
 * @param param 0: asynchronous write, 1: asynchronous read
 * @return 1 if request has been made; 0 otherwise
 */
#define CombAsyncRequest(param) _comb_control(0, 6, (param))

/**
 * @brief Get remaining transmit or receive data
 *
 * Gets the remaining data count from the asynchronous read or asynchronous
 * write being processed.
 *
 * @param param 0: asynchronous write, 1: asynchronous read
 * @return The number of bytes remaining
 */
#define CombBytesRemaining(param) _comb_control(0, 5, (param))

/**
 * @brief Get number of bytes available for reading
 *
 * Returns the number of bytes that can be read.
 *
 * @return Number of bytes to read
 */
#define CombBytesToRead() _comb_control(0, 4, 0)

/**
 * @brief Get number of bytes available for writing
 *
 * Returns the number of bytes that can be written.
 *
 * @return Number of bytes to write
 */
#define CombBytesToWrite() _comb_control(0, 4, 0)

/**
 * @brief Cancel asynchronous read
 *
 * Cancels the asynchronous reading operation.
 */
#define CombCancelRead() _comb_control(2, 3, 0)

/**
 * @brief Cancel asynchronous write
 *
 * Cancels the asynchronous writing operation.
 */
#define CombCancelWrite() _comb_control(2, 2, 0)

/**
 * @brief Get control line status
 *
 * Returns the control line status.
 *
 * @return Control line status
 */
#define CombControlStatus() _comb_control(0, 1, 0)

/**
 * @brief Get CTS status
 *
 * Checks if CTS is active.
 *
 * @return 1 if CTS==1, 0 otherwise
 */
#define CombCTS() _comb_control(3, 1, 0)

/**
 * @brief Get communication rate
 *
 * Returns the communication rate in bps.
 *
 * @return Communication rate in bps
 */
#define CombGetBPS() _comb_control(0, 3, 0)

/**
 * @brief Get communication mode
 *
 * Returns the communication mode.
 *
 * @return Communication mode
 */
#define CombGetMode() _comb_control(0, 2, 0)

/**
 * @brief Get packet size
 *
 * Returns the unit-number of characters for receiving.
 *
 * @return Packet size
 */
#define CombGetPacketSize() _comb_control(0, 4, 0)

/**
 * @brief Reset serial controller
 *
 * Resets the serial controller. Controller status, communication mode and
 * communication speed are saved.
 */
#define CombReset() _comb_control(2, 0, 0)

/**
 * @brief Reset error status
 *
 * Clears the bits related to the driver status error.
 * Includes a function which indicates the completion of the interrupt
 * processing to the driver.
 */
#define CombResetError() _comb_control(2, 1, 0)

/**
 * @brief Reset VBLANK
 *
 * Resets VBLANK-related state.
 */
#define CombResetVBLANK() _comb_control(2, 1, 0)

/**
 * @brief Set communication rate
 *
 * Sets the communication rate in bps.
 *
 * @param bps Communication rate in bps
 */
#define CombSetBPS(bps) _comb_control(1, 3, (bps))

/**
 * @brief Set control line status
 *
 * Sets the control line status.
 *
 * @param status Control line status value
 */
#define CombSetControl(status) _comb_control(1, 1, (status))

/**
 * @brief Set communication mode
 *
 * Sets the communication mode.
 *
 * @param mode Communication mode
 */
#define CombSetMode(mode) _comb_control(1, 2, (mode))

/**
 * @brief Set packet size
 *
 * Sets the unit-number of characters for receiving.
 *
 * @param size Packet size
 */
#define CombSetPacketSize(size) _comb_control(1, 4, (size))

/**
 * @brief Set RTS status
 *
 * Sets the RTS (Request To Send) line status.
 *
 * @param val 1: RTS is made 1, 0: RTS is made 0
 */
#define CombSetRTS(val) _comb_control(3, 0, (val))

/**
 * @brief Get SIO status
 *
 * Returns the serial controller status.
 *
 * @return Serial controller status
 */
#define CombSioStatus() _comb_control(0, 0, 0)

/**
 * @brief Set wait callback function
 *
 * Registers a wait callback function.
 *
 * @param func Pointer to the callback function
 * @return Previously registered callback function pointer
 */
#define CombWaitCallback(func) _comb_control(4, 0, (u_long)(func))

#endif
