#include <psyz/module.h>
#include "shared.h"

static const char* combos[] = {"my_ovl2: FIRST", "my_ovl2: SECOND"};
static int internal_counter = 0;

void Psyz_ModuleStart(void* param) {
    MainLog(combos[internal_counter++]);
    g_SubOverlayResult = internal_counter != 1;
}

void Psyz_ModuleStop(void) {}
