#include <psyz/module.h>
#include "shared.h"

void Psyz_ModuleStart(void* param) { MainLog("my_ovl2: start\n"); }

void Psyz_ModuleStop(void) { MainLog("my_ovl2: stop\n"); }
