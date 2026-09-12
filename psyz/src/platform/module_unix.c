// Backend for POSIX platforms: Linux, macOS, iOS
#include <psyz/log.h>
#include <psyz/module.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#define PSYZ_MODULE_EXT ".dylib"
#else
#include <unistd.h>
#define PSYZ_MODULE_EXT ".so"
#endif

typedef void (*ModuleStartFn)(void* param);
typedef void (*ModuleStopFn)(void);

struct PsyzInternalModule {
    void* handle;
    ModuleStopFn stop;
};

#define PSYZ_MODULE_MAX 256
static struct PsyzInternalModule g_PsyzModules[PSYZ_MODULE_MAX] = {0};
static PsyzModule AllocModule(void) {
    for (int i = 0; i < PSYZ_MODULE_MAX; i++) {
        if (!g_PsyzModules[i].handle) {
            return i + 1;
        }
    }
    return 0;
}

// load modules relative to the executable path
static bool GetHostExecutablePath(char* dir, size_t size) {
    static char cached[512];
    static bool is_cached = false;
    static bool is_ok = false;
    if (!is_cached) {
        is_cached = true;
#if defined(__APPLE__)
        uint32_t len = (uint32_t)sizeof(cached);
        if (_NSGetExecutablePath(cached, &len) == 0) {
            char* lastSep = strrchr(cached, '/');
            is_ok = lastSep != 0;
            if (is_ok) {
                *lastSep = '\0';
            }
        }
#else
        ssize_t len = readlink("/proc/self/exe", cached, sizeof(cached) - 1);
        if (len > 0) {
            cached[len] = '\0';
            char* lastSep = strrchr(cached, '/');
            is_ok = lastSep != 0;
            if (is_ok) {
                *lastSep = '\0';
            }
        }
#endif
    }
    if (is_ok) {
        snprintf(dir, size, "%s", cached);
    }
    return is_ok;
}

PsyzModule Psyz_ModuleOpen(const char* name, void* param) {
    char path[512];
    char exeDir[512];

    if (GetHostExecutablePath(exeDir, sizeof(exeDir))) {
        snprintf(path, sizeof(path), "%s/%s" PSYZ_MODULE_EXT, exeDir, name);
    } else {
        snprintf(path, sizeof(path), "%s" PSYZ_MODULE_EXT, name);
    }

    void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        ERRORF("dlopen('%s') failed: %s", path, dlerror());
        return 0;
    }

    ModuleStartFn start = (ModuleStartFn)dlsym(handle, "psyz_module_start");
    ModuleStopFn stop = (ModuleStopFn)dlsym(handle, "psyz_module_stop");
    if (!start || !stop) {
        ERRORF("'%s': PsyZ module entrypoints not found", path);
        dlclose(handle);
        return 0;
    }

    PsyzModule descriptor = AllocModule();
    if (!descriptor) {
        ERRORF("'%s': too many modules loaded", path);
        dlclose(handle);
        return 0;
    }
    struct PsyzInternalModule* module = &g_PsyzModules[descriptor - 1];
    module->handle = handle;
    module->stop = stop;
    start(param);
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
    if (!module->handle) {
        WARNF("module is already closed");
        return;
    }
    module->stop();
    dlclose(module->handle);
    module->handle = 0;
}
