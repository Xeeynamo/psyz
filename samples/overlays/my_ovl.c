#include <psyz/module.h>
#include "shared.h"

static struct SampleState* state;

static void Entrypoint(void) { MainLog("my_ovl: entrypoint called"); }

void Psyz_ModuleStart(void* param) {
    MainLog("my_ovl: start\n");
    if (state || !param) {
        MainLog("my_ovl: invalid initial state\n");
        return;
    }
    state = param;
    state->pfnEntrypoint = Entrypoint;
    state->starts++;
    g_SampleCounter++;
}

void Psyz_ModuleStop(void) {
    MainLog("my_ovl: stop\n");
    if (state) {
        state->stops++;
    }
}
