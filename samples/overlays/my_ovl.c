#include <psyz/module.h>
#include "shared.h"

__attribute__((constructor)) static void OverlayCtor(void) {
    MainLog("module ctor");
}

__attribute__((destructor)) static void OverlayDtor(void) {
    MainLog("module dtor");
}

void Psyz_ModuleStart(void* param) {
    MainLog("my_ovl: hello world!");
    g_SampleCounter++;
}

void Psyz_ModuleStop(void) { MainLog("my_ovl: stopping"); }
