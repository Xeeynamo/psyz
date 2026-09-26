#include <pspkernel.h>
#include <pspmoduleinfo.h>
#include <psyz/log.h>
#include <psyz/module.h>
#include <stdio.h>
#include <string.h>

struct PsyzInternalModule {
    SceUID modId; // valid values are >= 0
    int used;
};

#define PSYZ_MODULE_MAX 256
static struct PsyzInternalModule g_PsyzModules[PSYZ_MODULE_MAX] = {0};
static PsyzModule AllocModule(void) {
    for (int i = 0; i < PSYZ_MODULE_MAX; i++) {
        if (!g_PsyzModules[i].used) {
            g_PsyzModules[i] = (struct PsyzInternalModule){.used = 1};
            return i + 1;
        }
    }
    return 0;
}

PsyzModule Psyz_ModuleOpen(const char* name, void* param) {
    PsyzModule descriptor = AllocModule();
    if (!descriptor) {
        ERRORF("too many modules loaded");
        return 0;
    }

    struct PsyzInternalModule* module = &g_PsyzModules[descriptor - 1];
    char path[256];
    if (!name || snprintf(path, sizeof(path), "%s.prx", name) >= sizeof(path)) {
        ERRORF("invalid module path '%s'", path);
        module->used = 0;
        return 0;
    }
    SceUID modId = sceKernelLoadModule(path, 0, 0);
    if (modId < 0) {
        ERRORF("sceKernelLoadModule('%s') failed: 0x%08x", path, modId);
        module->used = 0;
        return 0;
    }

    void* argp = param;
    int status = 0;
    int ret = sceKernelStartModule(modId, sizeof(argp), &argp, &status, 0);
    if (ret < 0) {
        ERRORF("sceKernelStartModule('%s') failed: 0x%08x", path, ret);
        ret = sceKernelUnloadModule(modId); // module stalled
        if (ret < 0) {
            ERRORF("sceKernelUnloadModule failed: 0x%08x", ret);
            module->modId = modId;
        } else {
            module->used = 0;
        }
        return 0;
    }
    module->modId = modId;
    return descriptor;
}

void Psyz_ModuleClose(PsyzModule descriptor) {
    if (!descriptor) {
        return;
    }
    if (descriptor > PSYZ_MODULE_MAX) {
        ERRORF("module %d is invalid", descriptor);
        return;
    }
    struct PsyzInternalModule* module = &g_PsyzModules[descriptor - 1];
    if (!module->used) {
        WARNF("module is already closed");
        return;
    }
    int status = 0;
    int ret = sceKernelStopModule(module->modId, 0, 0, &status, 0);
    if (ret < 0) {
        ERRORF("sceKernelStopModule failed: 0x%08x", ret);
        return;
    }
    ret = sceKernelUnloadModule(module->modId);
    if (ret < 0) {
        ERRORF("sceKernelUnloadModule failed: 0x%08x", ret);
        return;
    }
    *module = (struct PsyzInternalModule){0};
}
