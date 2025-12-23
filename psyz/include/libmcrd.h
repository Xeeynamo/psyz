#ifndef LIBMCRD_H
#define LIBMCRD_H

#include <types.h>

/* Error codes */
#define McErrNone 0x00         /**< Normal exit / Connected */
#define McErrCardNotExist 0x01 /**< Card is not connected */
#define McErrCardInvalid 0x02  /**< Bad card / Communication error */
#define McErrNewCard 0x03      /**< New card (card was replaced) */
#define McErrNotFormat 0x04    /**< Not formatted */
#define McErrFileNotExist 0x05 /**< File not found */
#define McErrAlreadyExist 0x06 /**< File already exists */
#define McErrBlockFull 0x07    /**< Not enough available blocks */

/* Function codes for callbacks */
#define McFuncExist 0x01     /**< MemCardExist */
#define McFuncAccept 0x02    /**< MemCardAccept */
#define McFuncReadFile 0x03  /**< MemCardReadFile */
#define McFuncWriteFile 0x04 /**< MemCardWriteFile */
#define McFuncReadData 0x05  /**< MemCardReadData */
#define McFuncWriteData 0x06 /**< MemCardWriteData */

/**
 * @brief Memory card callback function type
 */
typedef void (*MemCB)(unsigned long cmds, unsigned long result);

/**
 * @brief Check Memory Card status
 *
 * Tests connection with the Memory Card specified by chan. If the card is
 * connected, additional information is obtained. If the card is new,
 * _card_clear() and _card_load() are executed, allowing the use of file access
 * functions. The function is asynchronous and returns immediately. Use
 * MemCardSync() or an exit callback to determine completion and get the result.
 * Maximum time required is 76 VSyncs (approximately 4 VSyncs if card not
 * present).
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @return 1 if registration successful, otherwise 0
 */
long MemCardAccept(long chan);

/**
 * @brief Define exit callback
 *
 * Sets the callback function (func) to be triggered when an asynchronous
 * function completes. If func is 0, no callback is generated.
 *
 * @param func Pointer to callback function
 * @return The address of the previously set callback
 */
MemCB MemCardCallback(MemCB func);

/**
 * @brief Close file
 *
 * Closes the file that was opened with MemCardOpen(). It is an asynchronous
 * function that exits immediately.
 */
void MemCardClose(void);

/**
 * @brief Create a new file in the Memory Card
 *
 * Creates the specified file in the Memory Card. It is a synchronous function;
 * blocking time is 1 - 4 VSyncs for normal exit, 4 - 76 VSyncs otherwise.
 * Doesn't enter a critical section. The block parameter is given in units of
 * 8192 bytes.
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @param file Filename
 * @param blocks Number of blocks
 * @return Error code (0x00 = success, -1 if async function is active)
 */
long MemCardCreateFile(long chan, char* file, long blocks);

/**
 * @brief Delete file from Memory Card
 *
 * Deletes the specified file from the Memory Card. It is a synchronous
 * function; blocking time: 1 - 4 VSyncs for normal exit, 4 - 76 VSyncs
 * otherwise. Does not enter critical section.
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @param file Filename
 * @return Error code (0x00 = success, -1 if async function is active)
 */
long MemCardDeleteFile(long chan, char* file);

/**
 * @brief Terminate Memory Card system
 *
 * Terminates the Memory Card system. It is a synchronous function.
 * MemCardStop() needs to be executed first if the system was activated from
 * MemCardStart().
 */
void MemCardEnd(void);

/**
 * @brief Get connection status of card
 *
 * Tests the connection status of the Memory Card specified by chan.
 * MemCardExist() is faster than MemCardAccept(), since it checks only the
 * presence of the card. The function is asynchronous and exits immediately. Use
 * MemCardSync() or an exit callback to determine completion. Time required is
 * approximately 4 VSyncs.
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @return 1 if the command was successfully registered; 0 otherwise
 */
long MemCardExist(long chan);

/**
 * @brief Format Memory Card
 *
 * Formats the Memory Card specified by chan. It is a synchronous function;
 * blocking time: 144 VSyncs. Does not enter critical section.
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @return Error code (0x00 = success, -1 if async function is active)
 */
long MemCardFormat(long chan);

/**
 * @brief Get file directory entry
 *
 * Gets the directory entry information for the file at the specified position.
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @param entry Entry number
 * @param name Buffer to receive filename
 * @param blocks Pointer to receive block count
 * @return Error code (0x00 = success)
 */
long MemCardGetDirentry(long chan, long entry, char* name, long* blocks);

/**
 * @brief Initialize Memory Card system
 *
 * Initializes the Memory Card system with the specified mode. It is a
 * synchronous function. This function must be called before using any other
 * Memory Card functions. After initialization, call MemCardStart() to begin
 * operations.
 *
 * @param mode Initialization mode (1 for shared with controller)
 * @return 1 on success
 */
long MemCardInit(long mode);

/**
 * @brief Open file
 *
 * Opens the specified file on the Memory Card for reading or writing. It is a
 * synchronous function; blocking time: 1 - 4 VSyncs for normal exit, 4 - 76
 * VSyncs otherwise. Doesn't enter critical section.
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @param file Filename
 * @param mode Open mode (0 = read, 1 = write)
 * @return Error code (0x00 = success, -1 if async function is active)
 */
long MemCardOpen(long chan, char* file, long mode);

/**
 * @brief Read data from opened file
 *
 * Reads specified bytes from the file opened with MemCardOpen(). It is an
 * asynchronous function and exits immediately. Use MemCardSync() or an exit
 * callback to determine completion.
 *
 * @param dest Pointer to destination buffer
 * @param bytes Number of bytes to read
 * @return 1 if registration successful, otherwise 0
 */
long MemCardReadData(unsigned long* dest, long bytes);

/**
 * @brief Read file from Memory Card
 *
 * Reads the entire contents of the specified file. It is an asynchronous
 * function and exits immediately. Use MemCardSync() or an exit callback to
 * determine completion.
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @param file Filename
 * @param dest Pointer to destination buffer
 * @return 1 if registration successful, otherwise 0
 */
long MemCardReadFile(long chan, char* file, unsigned long* dest);

/**
 * @brief Start Memory Card system
 *
 * Starts the Memory Card system initialized with MemCardInit(). It is a
 * synchronous function.
 */
void MemCardStart(void);

/**
 * @brief Stop Memory Card system
 *
 * Stops the Memory Card system. It is a synchronous function.
 */
void MemCardStop(void);

/**
 * @brief Wait for asynchronous operation completion
 *
 * Waits for completion of an asynchronous operation or checks its status.
 *
 * @param mode 0 = wait for completion, 1 = check status without waiting
 * @param cmds Pointer to receive completed command code
 * @param result Pointer to receive result code
 * @return 0 if operation is complete, positive value if still in progress,
 * negative if error
 */
long MemCardSync(long mode, unsigned long* cmds, unsigned long* result);

/**
 * @brief Unformat Memory Card
 *
 * Unformats the Memory Card specified by chan. It is a synchronous function.
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @return Error code (0x00 = success, -1 if async function is active)
 */
long MemCardUnformat(long chan);

/**
 * @brief Write data to opened file
 *
 * Writes specified bytes to the file opened with MemCardOpen(). It is an
 * asynchronous function and exits immediately. Use MemCardSync() or an exit
 * callback to determine completion.
 *
 * @param src Pointer to source buffer
 * @param bytes Number of bytes to write
 * @return 1 if registration successful, otherwise 0
 */
long MemCardWriteData(unsigned long* src, long bytes);

/**
 * @brief Write file to Memory Card
 *
 * Writes data to the specified file. It is an asynchronous function and exits
 * immediately. Use MemCardSync() or an exit callback to determine completion.
 *
 * @param chan Port number + card number (port A: 0x00, port B: 0x10; card
 * number normally 0)
 * @param file Filename
 * @param src Pointer to source buffer
 * @param offset Offset in file (in bytes)
 * @param bytes Number of bytes to write
 * @return 1 if registration successful, otherwise 0
 */
long MemCardWriteFile(
    long chan, char* file, unsigned long* src, long offset, long bytes);

#endif
