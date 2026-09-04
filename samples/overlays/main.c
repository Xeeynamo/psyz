#include <psyz/module.h>
#include <psyz/log.h>
#include <stdio.h>
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
    int ogCounter = g_SampleCounter;
    PsyzModule* mod = Psyz_ModuleOpen("my_ovl", 0);
    if (!mod) {
        goto error;
    }
    Psyz_ModuleClose(mod);
    if (ogCounter == g_SampleCounter) {
        MainLog("if you see this, then there's a bug.");
    }

exit:
#if defined(__PSP__)
    sceKernelExitGame();
#endif
    return did_fail;
error:
    did_fail = 1;
    goto exit;
}
