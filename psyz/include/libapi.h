#ifndef LIBAPI_H
#define LIBAPI_H

/**
 * @file libapi.h
 * @brief Kernel API Library
 *
 * This library provides low-level kernel services including device I/O,
 * event management, thread control, exception handling, and system calls.
 *
 * Key features:
 * - Device I/O (open, close, read, write, ioctl)
 * - Event management (OpenEvent, CloseEvent, EnableEvent, DeliverEvent)
 * - Thread control (OpenTh, CloseTh, ChangeTh)
 * - Exception handling (EnterCriticalSection, ExitCriticalSection)
 * - Memory Card low-level access (_card_* functions)
 * - Controller/PAD initialization (InitPAD, StartPAD, StopPAD)
 * - File loading and execution (Load, Exec, LoadExec)
 */

#include <types.h>

/* Number of registers for TCB */
#ifndef NREGS
#define NREGS 32
#endif

/* Structures */

/**
 * @brief Directory entries
 *
 * Stores information relating to files registered in the file system.
 */
struct DIRENTRY {
    char name[20];         /**< Filename */
    long attr;             /**< Attributes (dependent on file system) */
    long size;             /**< File size (in bytes) */
    struct DIRENTRY* next; /**< Pointer to next file entry (for user) */
    char system[8];        /**< Reserved by system */
};

/**
 * @brief Event Control Block
 *
 * Stores information for each event.
 */
struct EvCB {
    unsigned long desc; /**< Cause descriptor */
    long status;        /**< Status */
    long spec;          /**< Event type */
    long mode;          /**< Mode */
    long (*FHandler)(); /**< Pointer to a function type handler */
    long system[2];     /**< Reserved by system */
};

/**
 * @brief Execution file data structure
 *
 * Stores information for loading and executing a program. The data is stored
 * in the first 2K bytes of the execution file (PS-X EXE format). By adding
 * stack information and transferring it to Exec(), the program is activated.
 */
struct EXEC {
    unsigned long pc0;    /**< Execution start address */
    unsigned long gp0;    /**< gp register initial value */
    unsigned long t_addr; /**< Starting address of initialized text section */
    unsigned long t_size; /**< Size of text section */
    unsigned long d_addr; /**< Starting address of initialized data section */
    unsigned long d_size; /**< Size of initialized data section */
    unsigned long b_addr; /**< Uninitialized data section start address */
    unsigned long b_size; /**< Uninitialized data section size */
    unsigned long s_addr; /**< Stack start address (specified by the user) */
    unsigned long s_size; /**< Stack size (specified by the user) */
    unsigned long sp;     /**< Register shunt variable */
    unsigned long fp;     /**< Register shunt variable */
    unsigned long gp;     /**< Register shunt variable */
    unsigned long ret;    /**< Register shunt variable */
    unsigned long base;   /**< Register shunt variable */
};

/**
 * @brief Task Control Block
 *
 * Stores a context (including contents of the registers) for thread
 * management.
 */
struct TCB {
    long status;              /**< Status */
    long mode;                /**< Mode */
    unsigned long reg[NREGS]; /**< Register saving area */
    long system[6];           /**< Reserved by system */
};

/**
 * @brief Task Control Block Header
 *
 * Used for thread management. entry is a pointer to the currently executing
 * TCB.
 */
struct TCBH {
    struct TCB* entry; /**< Pointer to execution TCB */
    long flag;         /**< System reserved */
};

/**
 * @brief System Table Information
 *
 * Information about various system tables used by the kernel. The tables
 * begin at address 0x00000100.
 */
struct ToT {
    unsigned long* head; /**< Pointer to a system table start address */
    long size;           /**< System table size (in bytes) */
};

/* Functions */

/* ============================================================================
 * Event Management
 * ============================================================================
 */

/**
 * @brief Open an event
 *
 * Secures an EvCB for an event with the descriptor desc and event class spec.
 * Must be executed in a critical section.
 *
 * @param desc Cause descriptor
 * @param spec Event type
 * @param mode Mode
 * @param func Pointer to the handler function
 * @return Event descriptor on success, -1 on failure
 */
extern long OpenEvent(unsigned long desc, long spec, long mode, long (*func)());

/**
 * @brief Close an event
 *
 * Releases the EvCB specified by event. Must be executed in a critical
 * section.
 *
 * @param event Event descriptor
 * @return 1 on success, 0 on failure
 */
extern long CloseEvent(long event);

/**
 * @brief Enable occurrence of an event
 *
 * Enables occurrence of an event specified by the descriptor event. Changes
 * the event status to EvStACTIVE.
 *
 * @param event Event descriptor
 * @return 1 on success, 0 on failure
 */
extern long EnableEvent(long event);

/**
 * @brief Disable an event
 *
 * Inhibits occurrence of an event specified by the descriptor event. Changes
 * the event status to EvStWAIT.
 *
 * @param event Event descriptor
 * @return 1 on success, 0 on failure
 */
extern long DisableEvent(long event);

/**
 * @brief Test an event
 *
 * Checks to see whether or not the event specified by the descriptor event has
 * occurred. If so, restores the event state to EvStACTIVE.
 *
 * @param event Event descriptor
 * @return 1 if event occurred, 0 otherwise
 */
extern long TestEvent(long event);

/**
 * @brief Wait for the occurrence of an event
 *
 * Waits until an event specified by the descriptor event occurs, and returns
 * after restoring the event state to EvStACTIVE.
 *
 * @param event Event descriptor
 * @return 1 on success, 0 on failure
 */
extern long WaitEvent(long event);

/**
 * @brief Generate an event
 *
 * Delivers an event if the event's current status is EvStACTIVE. Must be
 * executed in a critical section.
 *
 * @param ev1 Cause descriptor
 * @param ev2 Event class
 */
extern void DeliverEvent(unsigned long ev1, long ev2);

/**
 * @brief Cancel an event
 *
 * Returns event state from EvStALREADY to EvStACTIVE if the event mode is
 * EvMdNOINTR. Must be executed in a critical section.
 *
 * @param ev1 Cause descriptor
 * @param ev2 Event class
 */
extern void UnDeliverEvent(unsigned long ev1, long ev2);

/* ============================================================================
 * Thread Management
 * ============================================================================
 */

/**
 * @brief Open a thread
 *
 * Secures a TCB for a given thread, and initializes it with the arguments
 * given. Must be executed in a critical section.
 *
 * @param func Pointer to the execution start function
 * @param sp Stack pointer value
 * @param gp Global pointer value
 * @return Thread descriptor on success, -1 on failure
 */
extern unsigned long OpenTh(
    unsigned long (*func)(), unsigned long sp, unsigned long gp);

/**
 * @brief Close a thread
 *
 * Closes a thread and releases its TCB. Must be executed in a critical
 * section.
 *
 * @param thread Thread descriptor
 * @return 1 on success, 0 on failure
 */
extern long CloseTh(unsigned long thread);

/**
 * @brief Change the thread to be executed
 *
 * Transfers execution to the thread specified by thread. The current thread is
 * saved in a TCB. Returns when the original thread is restored.
 *
 * @param thread Thread descriptor
 * @return 1 on success and re-execution, 0 on failure
 */
extern long ChangeTh(unsigned long thread);

/* ============================================================================
 * Exception and Critical Section Handling
 * ============================================================================
 */

/**
 * @brief Disable interrupts
 *
 * Disables interrupts (enters a critical section). Executes an internal system
 * call and destroys the interrupt context.
 *
 * @return 0 when called in a critical section, 1 otherwise
 */
extern long EnterCriticalSection(void);

/**
 * @brief Enable interrupts
 *
 * Enables interrupts (exits from a critical section). Executes an internal
 * system call and destroys the interrupt context.
 */
extern void ExitCriticalSection(void);

/**
 * @brief Suppress interrupts
 *
 * Suppresses interrupts. No system call interrupt is generated internally, so
 * can be invoked in event handling and callback functions. Must be executed in
 * a critical section.
 */
extern void SwEnterCriticalSection(void);

/**
 * @brief Enable interrupts
 *
 * Enables interrupts. No system call interrupt is generated internally, so can
 * be invoked in event handling and callback functions. Must be executed in a
 * critical section.
 */
extern void SwExitCriticalSection(void);

/**
 * @brief Cause an interrupt
 *
 * Causes an interrupt, and stores the current context in the execution TCB.
 * Also valid in a critical section. Executes an internal call and destroys the
 * exception context.
 */
extern void Exception(void);

/**
 * @brief Return from exception
 *
 * Accesses the exception context and returns from exception processing. Used
 * in an event handler or callback function.
 */
extern void ReturnFromException(void);

/* ============================================================================
 * System Configuration and Control
 * ============================================================================
 */

/**
 * @brief Modify the kernel configuration
 *
 * Modifies system configuration parameters. Should be executed at the head of
 * the first execution file.
 *
 * @param ev Number of event control block (EvCB) elements
 * @param tcb Number of task control block (TCB) elements
 * @param sp Ignored
 * @return 1 on success, 0 on failure
 */
extern long SetConf(unsigned long ev, unsigned long tcb, unsigned long sp);

/**
 * @brief Get the kernel configuration
 *
 * Stores a system configuration parameter set by SetConf().
 *
 * @param ev Pointer to number of event management blocks
 * @param tcb Pointer to number of task management blocks
 * @param sp Ignored
 */
extern void GetConf(unsigned long* ev, unsigned long* tcb, unsigned long* sp);

/**
 * @brief Modify the valid memory size
 *
 * Changes the valid memory size to n. Must be 2 or 8 (megabytes).
 *
 * @param n Valid memory size (in megabytes)
 */
extern void SetMem(unsigned long n);

/**
 * @brief Flush instruction cache
 *
 * Flushes the instruction cache (I-cache). Must be executed in a critical
 * section.
 */
extern void FlushCache(void);

/**
 * @brief Display the system error screen
 *
 * Displays a detected system error for the user. On PlayStation, calls exit().
 *
 * @param c Error identification character (Alphabetic character)
 * @param n Error identification code (0 to 999)
 */
extern void SystemError(char c, long n);

/* ============================================================================
 * Register Access Functions
 * ============================================================================
 */

/**
 * @brief Get cause register value
 *
 * Gets the value of the cause register (a coprocessor control register).
 *
 * @return Current cause register value
 */
extern unsigned long GetCr(void);

/**
 * @brief Get value of gp register
 *
 * Gets the value of the gp register.
 *
 * @return Current gp register value
 */
extern unsigned long GetGp(void);

/**
 * @brief Get value of stack pointer
 *
 * Gets value of sp register.
 *
 * @return Current sp register value
 */
extern unsigned long GetSp(void);

/**
 * @brief Get value of status register
 *
 * Gets the value of the status register.
 *
 * @return Current status register value
 */
extern unsigned long GetSr(void);

/**
 * @brief Set the stack pointer
 *
 * Sets the sp register to the value new_sp.
 *
 * @param new_sp Value to set sp register
 * @return Previous sp register value
 */
extern unsigned long SetSp(unsigned long new_sp);

/**
 * @brief Get address of system stack
 *
 * Gets the highest address of a system stack area for event handler function
 * execution. The size of the stack area is 2 K-bytes.
 *
 * @return Highest address of the system stack area
 */
extern long GetSysSp(void);

/* ============================================================================
 * Root Counter Functions
 * ============================================================================
 */

/**
 * @brief Set a root counter
 *
 * Sets the root counter in spec to the target value in target, and the mode in
 * mode.
 *
 * @param spec Root counter specification
 * @param target Target value
 * @param mode Mode
 * @return 1 on success, 0 on failure
 */
extern long SetRCnt(long spec, unsigned short target, long mode);

/**
 * @brief Get value of a root counter
 *
 * Returns the current value of root counter spec. To be used when root counter
 * spec has been set to polling mode (RCntMdNOINTR).
 *
 * @param spec Root counter
 * @return 32-bit unsigned expanded counter value, or -1 on failure
 */
extern long GetRCnt(long spec);

/**
 * @brief Start a root counter
 *
 * Enables interrupts for root counter spec.
 *
 * @param spec Root counter
 * @return 1 on success, 0 on failure
 */
extern long StartRCnt(unsigned long spec);

/**
 * @brief Stop a root counter
 *
 * Disables interrupts for root counter spec.
 *
 * @param spec Root counter
 * @return 1 on success, 0 on failure
 */
extern long StopRCnt(long spec);

/**
 * @brief Reset a root counter
 *
 * Resets the root counter spec to 0.
 *
 * @param spec Root counter specification
 * @return 1 on success, 0 on failure
 */
extern long ResetRCnt(long spec);

/* ============================================================================
 * Controller/PAD Functions
 * ============================================================================
 */

/**
 * @brief Initialize the controller
 *
 * Registers a receive data buffer for the controller. Always allocate 34 bytes
 * for buffer length.
 *
 * @param bufA Pointer to incoming data buffer A
 * @param bufB Pointer to incoming data buffer B
 * @param lenA Length of incoming data buffer A (in bytes)
 * @param lenB Length of incoming data buffer B (in bytes)
 * @return Always 1
 */
extern long InitPAD(char* bufA, char* bufB, long lenA, long lenB);

/**
 * @brief Start reading the controller
 *
 * Triggered by the interruption of a vertical retrace line, starts to read the
 * controller. Interrupts are permitted.
 *
 * @return Always returns 1
 */
extern long StartPAD(void);

/**
 * @brief Stop reading the controller
 *
 * Stops reading the controller. Interrupts are not permitted.
 */
extern void StopPAD(void);

/**
 * @brief Set the control driver
 *
 * If val is 1, interrupt processing in a control driver started by a vertical
 * retrace line interrupt is completed. If val is 0, processing is passed to a
 * lower priority interrupt module.
 *
 * @param val Vertical retrace line interruption clear flag
 */
extern void ChangeClearPAD(long val);

/**
 * @brief Enable communication with the controller
 *
 * Enables communication with a controller which was disabled with
 * DisablePAD().
 */
extern void EnablePAD(void);

/**
 * @brief Disable communication with the controller
 *
 * Temporarily disables communication with the controller.
 */
extern void DisablePAD(void);

int PAD_init(s32 /*??*/, s32* /*??*/);

/* ============================================================================
 * Font Functions
 * ============================================================================
 */

/**
 * @brief Get Kanji font pattern addresses
 *
 * Gets the starting address in the kernel of the font pattern for the Kanji
 * character specified by sjiscode.
 *
 * @param sjiscode Shift-JIS code
 * @return Starting address of a Kanji font pattern, or -1 if no font data
 */
extern unsigned long Krom2RawAdd(unsigned short sjiscode);

/**
 * @brief Get shift-JIS font pattern addresses
 *
 * Gets the starting address in the font pattern kernel for the
 * non-Kanji/Kanji level 1 character specified by sjiscode.
 *
 * @param sjiscode Shift-JIS code
 * @return Font pattern starting address, or address containing full space font
 * pattern
 */
extern unsigned long Krom2RawAdd2(unsigned short sjiscode);

/* ============================================================================
 * File System Functions
 * ============================================================================
 */

/**
 * @brief Install the ISO-9660 file system
 *
 * Installs the ISO-9660 file system driver that manages access to the CD-ROM.
 */
extern void _96_init(void);

/**
 * @brief Remove the ISO-9660 file system
 *
 * Removes the ISO-9660 file system driver that manages access to the CD-ROM.
 */
extern void _96_remove(void);

/* ============================================================================
 * Error Handling Functions
 * ============================================================================
 */

/**
 * @brief Get the latest I/O error code
 *
 * Gets the latest error code from all file descriptors.
 *
 * @return Error code
 */
extern int _get_errno(void);

/**
 * @brief Get an error code for a file descriptor
 *
 * Gets the most recent error code of the specified file descriptor.
 *
 * @param fd File descriptor
 * @return Error code
 */
extern int _get_error(int fd);

/* ============================================================================
 * System Functions
 * ============================================================================
 */

/**
 * @brief Reboot the system
 *
 * Reboots the system. Useful for demonstration programs; don't use for general
 * title applications.
 */
extern void _boot(void);

#endif
