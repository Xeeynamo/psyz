#include <psyz/module.h>
#include <psyz/log.h>
#include <string.h>
#if defined(__PSP__)
#include <pspkernel.h>
#endif
#include "shared.h"

// proof that an overlay can use an exported global variable
int g_SampleCounter = 42;

// proof that an overlay can call an exported function
void MainLog(const char* s) {
#if defined(__PSP__)
    sceIoWrite(1, s, strlen(s));
#else
    INFOF("%s", s);
#endif
}

int main(int argc, char* argv[]) {
    int did_fail = 0;
    struct SampleState state = {0};
    PsyzModule mod = Psyz_ModuleOpen("my_ovl", &state);
    if (!mod) {
        MainLog("OVERLAYS_FAIL: open\n");
        did_fail = 1;
        goto exit;
    }
    if (g_SampleCounter != 43 || state.starts != 1 || state.stops != 0) {
        MainLog("if you see this, the module Start callback failed.\n");
        goto exit;
    }
    state.pfnEntrypoint();
    Psyz_ModuleClose(mod);
    if (g_SampleCounter != 43 || state.starts != 1 || state.stops != 1) {
        MainLog("if you see this, the module Stop callback failed.\n");
        did_fail = 1;
    }

exit:
#if defined(__PSP__)
    sceKernelExitGame();
#endif
    return did_fail;
}
