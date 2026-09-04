#include <pspkernel.h>
#include <pspmoduleinfo.h>
#include <psyz/log.h>
#include <psyz/module.h>
#include <stdio.h>
#include <string.h>

struct PsyzModule {
    SceUID modId; // valid values are >= 0
    int used;
};

#define PSYZ_MODULE_MAX 256
static struct PsyzModule g_PsyzModules[PSYZ_MODULE_MAX] = {0};
static PsyzModule* AllocModule(void) {
    for (int i = 0; i < PSYZ_MODULE_MAX; i++) {
        if (!g_PsyzModules[i].used) {
            g_PsyzModules[i] = (struct PsyzModule){0};
            return &g_PsyzModules[i];
        }
    }
    return 0;
}

PsyzModule* Psyz_ModuleOpen(const char* name, void* param) {
    PsyzModule* module = AllocModule();
    if (!module) {
        ERRORF("too many modules loaded");
        return 0;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s.prx", name);
    SceUID modId = sceKernelLoadModule(path, 0, 0);
    if (modId < 0) {
        ERRORF("sceKernelLoadModule('%s') failed: 0x%08x", path, modId);
        return 0;
    }

    void* argp = param;
    int status = 0;
    int ret = sceKernelStartModule(modId, sizeof(argp), &argp, &status, 0);
    if (ret < 0) {
        ERRORF("sceKernelStartModule('%s') failed: 0x%08x", path, ret);
        sceKernelUnloadModule(modId);
        return 0;
    }
    module->modId = modId;
    module->used = 1;
    return module;
}

void Psyz_ModuleClose(PsyzModule* module) {
    if (!module) {
        return;
    }
    int status = 0;
    sceKernelStopModule(module->modId, 0, 0, &status, 0);
    sceKernelUnloadModule(module->modId);
    *module = (struct PsyzModule){0};
}
