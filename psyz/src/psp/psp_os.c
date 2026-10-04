#include <pspkernel.h>
#include <pspthreadman.h>
#include <psyz/log.h>
#include "../psyz/os.h"

// Above the game's main thread (0x20) and below the audio thread (0x12).
#define IRQ_THREAD_PRIORITY 0x18

uint32_t Psyz_OsNowUs(void) { return sceKernelGetSystemTimeLow(); }

uintptr_t Psyz_OsThreadSelf(void) { return (uintptr_t)sceKernelGetThreadId(); }

void* Psyz_OsLockCreate(void) {
    SceUID sema = sceKernelCreateSema("psyz_kernel_lock", 0, 1, 1, NULL);
    return sema < 0 ? NULL : (void*)(intptr_t)sema;
}

void Psyz_OsLock(void* lock) {
    sceKernelWaitSema((SceUID)(intptr_t)lock, 1, NULL);
}

void Psyz_OsUnlock(void* lock) {
    sceKernelSignalSema((SceUID)(intptr_t)lock, 1);
}

void* Psyz_OsBellCreate(void) {
    SceUID sema = sceKernelCreateSema("psyz_kernel_bell", 0, 0, 1, NULL);
    return sema < 0 ? NULL : (void*)(intptr_t)sema;
}

void Psyz_OsBellRing(void* bell) {
    sceKernelSignalSema((SceUID)(intptr_t)bell, 1);
}

void Psyz_OsBellWait(void* bell, uint32_t timeout_us) {
    SceUInt timeout = timeout_us;
    sceKernelWaitSema((SceUID)(intptr_t)bell, 1,
                      timeout_us == PSYZ_OS_FOREVER ? NULL : &timeout);
}

static int ThreadEntry(SceSize args, void* argp) {
    (*(void (**)(void))argp)();
    return 0;
}

int Psyz_OsThreadStart(void (*fn)(void), const char* name) {
    SceUID thid = sceKernelCreateThread(
        name, ThreadEntry, IRQ_THREAD_PRIORITY, 0x4000, 0, NULL);
    if (thid < 0) {
        WARNF("cannot start thread %s: %08x", name, thid);
        return -1;
    }
    sceKernelStartThread(thid, sizeof(fn), &fn);
    return 0;
}
