// Host primitives behind the emulated PS1 kernel. Private to PsyZ.
#ifndef PSYZ_OS_H
#define PSYZ_OS_H

#include <stdint.h>

#define PSYZ_OS_FOREVER UINT32_MAX

// Monotonic clock in microseconds. It wraps every ~71 minutes, so compare
// two readings through their signed difference.
uint32_t Psyz_OsNowUs(void);

// Identifier of the calling thread, never 0.
uintptr_t Psyz_OsThreadSelf(void);

// Non-recursive mutex.
void* Psyz_OsLockCreate(void);
void Psyz_OsLock(void* lock);
void Psyz_OsUnlock(void* lock);

// Auto-reset signal: a ring wakes one waiter, or the next one to wait.
void* Psyz_OsBellCreate(void);
void Psyz_OsBellRing(void* bell);
void Psyz_OsBellWait(void* bell, uint32_t timeout_us);

// Starts a detached thread that runs above the game's own priority.
// Returns 0 on success, -1 when the host cannot run threads.
int Psyz_OsThreadStart(void (*fn)(void), const char* name);

#endif
