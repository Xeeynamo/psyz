#include <psyz/module.h>
#include <psyz/log.h>
#include <string.h>
#if defined(__PSP__)
#include <pspkernel.h>
#endif
#include "shared.h"

// proof that an overlay can use an exported global variable
int g_SampleCounter = 42;
int g_SubOverlayResult = -1;

// proof that an overlay can call an exported function
void MainLog(const char* s) {
#if defined(__PSP__)
    sceIoWrite(1, s, strlen(s));
#else
    INFOF("%s", s);
#endif
}

int SampleLoadAndUnloadOverlay(void) {
    struct SampleState state = {0};
    int expected_counter = g_SampleCounter + 1;
    PsyzModule mod = Psyz_ModuleOpen("my_ovl", &state);
    if (!mod) {
        MainLog("failed to open overlay 'my_ovl'\n");
        return 1;
    }
    if (g_SampleCounter != expected_counter || state.starts != 1 ||
        state.stops != 0) {
        MainLog("if you see this, the module Start callback failed.\n");
        return 1;
    }
    state.pfnEntrypoint();
    Psyz_ModuleClose(mod);
    if (g_SampleCounter != expected_counter || state.starts != 1 ||
        state.stops != 1) {
        MainLog("if you see this, the module Stop callback failed.\n");
        return 1;
    }
    return 0;
}

int SampleLoadMultipleOverlays(void) {
    PsyzModule mod = Psyz_ModuleOpen("my_ovl2", NULL);
    if (!mod) {
        MainLog("failed to open overlay 'my_ov2l'\n");
        return 1;
    }
    Psyz_ModuleClose(mod);
    return 0;
}

int SampleOverlaysCanLoadOverlays(void) {
    g_SubOverlayResult = -1;
    PsyzModule mod = Psyz_ModuleOpen("my_ovl2", SampleLoadAndUnloadOverlay);
    if (!mod) {
        MainLog("failed to open overlay 'my_ovl2'\n");
        return 1;
    }
    Psyz_ModuleClose(mod);
    return g_SubOverlayResult;
}

int SampleOverlaysLoseStatusWhenUnloaded(void) {
    g_SubOverlayResult = -1;

    PsyzModule mod = Psyz_ModuleOpen("my_ovl3", SampleLoadAndUnloadOverlay);
    if (!mod) {
        MainLog("failed to open overlay 'my_ovl3'\n");
        return 1;
    }
    Psyz_ModuleClose(mod);

    mod = Psyz_ModuleOpen("my_ovl3", SampleLoadAndUnloadOverlay);
    if (!mod) {
        MainLog("failed to open overlay 'my_ovl3'\n");
        return 1;
    }
    Psyz_ModuleClose(mod);

    return g_SubOverlayResult;
}

int main(int argc, char* argv[]) {
    int any_failed = 0;
    int did_fail = 0;

    did_fail = SampleLoadAndUnloadOverlay();
    if (did_fail) {
        MainLog("FAIL SampleLoadAndUnloadOverlay");
        any_failed = 1;
    }

    did_fail = SampleLoadMultipleOverlays();
    if (did_fail) {
        MainLog("FAIL SampleLoadMultipleOverlays");
        any_failed = 1;
    }

    did_fail = SampleOverlaysCanLoadOverlays();
    if (did_fail) {
        MainLog("FAIL SampleOverlaysCanLoadOverlays");
        any_failed = 1;
    }

    did_fail = SampleOverlaysLoseStatusWhenUnloaded();
    if (did_fail) {
        MainLog("FAIL SampleOverlaysLoseStatusWhenUnloaded");
        any_failed = 1;
    }

    if (!any_failed) {
        MainLog("PASS");
    }

#if defined(__PSP__)
    sceKernelExitGame();
#endif
    return any_failed;
}
