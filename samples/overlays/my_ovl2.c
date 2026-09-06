#include <psyz/module.h>
#include "shared.h"

typedef int (*PFN_LoadSubOverlay)(void);

void Psyz_ModuleStart(void* param) {
    MainLog("my_ovl2: start\n");
    PFN_LoadSubOverlay pfnLoadSubOverlay = (PFN_LoadSubOverlay)param;
    if (pfnLoadSubOverlay) {
        g_SubOverlayResult = pfnLoadSubOverlay();
    }
}

void Psyz_ModuleStop(void) { MainLog("my_ovl2: stop\n"); }
