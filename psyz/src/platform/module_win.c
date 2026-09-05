// Backend for Windows-like targets
#include <psyz/log.h>
#include <psyz/module.h>
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef void (*ModuleStartFn)(void* param);
typedef void (*ModuleStopFn)(void);

struct PsyzInternalModule {
    HMODULE handle;
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
        unsigned long len =
            GetModuleFileNameA(0, cached, (DWORD)sizeof(cached));
        if (len == 0 || len >= sizeof(cached)) {
            return false;
        }
        char* lastSep = strrchr(cached, '\\');
        if (!lastSep) {
            return false;
        }
        *lastSep = '\0';
        is_ok = true;
    }
    if (is_ok) {
        _snprintf_s(dir, size, _TRUNCATE, "%s", cached);
    }
    return is_ok;
}

PsyzModule Psyz_ModuleOpen(const char* name, void* param) {
    char path[512];
    char exeDir[512];

    if (GetHostExecutablePath(exeDir, sizeof(exeDir))) {
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s.dll", exeDir, name);
    } else {
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s.dll", name);
    }

    HMODULE handle = LoadLibraryA(path);
    if (!handle) {
        ERRORF("LoadLibraryA('%s') failed: 0x%lx", path, GetLastError());
        return 0;
    }

    ModuleStartFn start =
        (ModuleStartFn)GetProcAddress(handle, "psyz_module_start");
    ModuleStopFn stop =
        (ModuleStopFn)GetProcAddress(handle, "psyz_module_stop");
    if (!start || !stop) {
        ERRORF("'%s': PsyZ module entrypoints not found", path);
        FreeLibrary(handle);
        return 0;
    }

    PsyzModule descriptor = AllocModule();
    if (!descriptor) {
        ERRORF("'%s': too many modules loaded", path);
        FreeLibrary(handle);
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
    FreeLibrary(module->handle);
    module->handle = 0;
}
