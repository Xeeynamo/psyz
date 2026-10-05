#include <SDL3/SDL.h>
#include <psyz/log.h>
#include "../psyz/os.h"

uint32_t Psyz_OsNowUs(void) { return (uint32_t)SDL_NS_TO_US(SDL_GetTicksNS()); }

uintptr_t Psyz_OsThreadSelf(void) {
    return (uintptr_t)SDL_GetCurrentThreadID();
}

void* Psyz_OsLockCreate(void) { return SDL_CreateMutex(); }
void Psyz_OsLock(void* lock) { SDL_LockMutex((SDL_Mutex*)lock); }
void Psyz_OsUnlock(void* lock) { SDL_UnlockMutex((SDL_Mutex*)lock); }

void* Psyz_OsBellCreate(void) { return SDL_CreateSemaphore(0); }

void Psyz_OsBellRing(void* bell) {
    SDL_Semaphore* sem = (SDL_Semaphore*)bell;
    if (SDL_GetSemaphoreValue(sem) == 0) {
        SDL_SignalSemaphore(sem);
    }
}

void Psyz_OsBellWait(void* bell, uint32_t timeout_us) {
    SDL_Semaphore* sem = (SDL_Semaphore*)bell;
    if (timeout_us == PSYZ_OS_FOREVER) {
        SDL_WaitSemaphore(sem);
        return;
    }
    // round up: waking early would spin until the deadline
    SDL_WaitSemaphoreTimeout(
        sem, (Sint32)(timeout_us / 1000 + (timeout_us % 1000 != 0)));
}

static int SDLCALL ThreadEntry(void* fn) {
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
    ((void (*)(void))fn)();
    return 0;
}

int Psyz_OsThreadStart(void (*fn)(void), const char* name) {
    SDL_Thread* thread = SDL_CreateThread(ThreadEntry, name, (void*)fn);
    if (!thread) {
        WARNF("cannot start thread %s: %s", name, SDL_GetError());
        return -1;
    }
    SDL_DetachThread(thread);
    return 0;
}
