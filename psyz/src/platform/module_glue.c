// simple shim that abstracts the PSP/Windows entrypoints
#include <psyz/module.h>
#include <psyz/log.h>

#if defined(_WIN32)
#define PSYZ_MODULE_EXPORT __declspec(dllexport)
#else
#define PSYZ_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#ifndef PSYZ_MODULE_NAME
#define PSYZ_MODULE_NAME "psyz_module"
#endif

PSYZ_MODULE_EXPORT void psyz_module_start(void* param) {
    INFOF("start %s", PSYZ_MODULE_NAME);
    Psyz_ModuleStart(param);
}

PSYZ_MODULE_EXPORT void psyz_module_stop() {
    INFOF("stop %s", PSYZ_MODULE_NAME);
    Psyz_ModuleStop();
}
