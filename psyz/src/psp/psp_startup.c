#include <pspkernel.h>

PSP_MODULE_INFO("PSYZ", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024); // 1MB of heap for kernel stuff (threads, callbacks)

static volatile int quit_requested = 0;

int Psyz_QuitRequested(void) { return quit_requested; }

static int ExitCallback(int arg1, int arg2, void* common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    quit_requested = 1;
    sceKernelExitGame();
    return 0;
}

static int CallbackThread(SceSize args, void* argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("exit_cb", ExitCallback, NULL);
    if (cbid >= 0) {
        sceKernelRegisterExitCallback(cbid);
    }
    sceKernelSleepThreadCB();
    return 0;
}

// crt0 calls this before main, no need for the user to manually call this
__attribute__((constructor)) static void SetupCallbacks(void) {
    int thid = sceKernelCreateThread(
        "callbacks", CallbackThread, 0x11, 0x1000, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, NULL);
    }
}
