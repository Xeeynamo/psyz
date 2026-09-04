// Backend for Windows-like targets
#include <psyz/module.h>
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*ModuleStartFn)(void* param);
typedef void (*ModuleStopFn)(void);
typedef void(__cdecl* PsyzModuleFn)(void* param);

struct PsyzModule {
    HMODULE handle;
    ModuleStopFn stop;
};

static char g_PsyzModuleLastError[512] = "";
static void SetError(const char* what) {
    DWORD err = GetLastError();
    char msg[256] = "";
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg, sizeof(msg), 0);
    _snprintf_s(g_PsyzModuleLastError, sizeof(g_PsyzModuleLastError), _TRUNCATE,
                "%s: %s (0x%lx)", what, msg, err);
}

const char* Psyz_ModuleError(void) { return g_PsyzModuleLastError; }

static const char* GetExeDir(void) {
    static char dir[512];
    static bool done = false;
    static bool isok = false;
    if (done) {
        return isok ? dir : 0;
    }
    done = true;

    unsigned long len = GetModuleFileNameA(0, dir, sizeof(dir));
    if (len == 0 || len >= sizeof(dir)) {
        return 0;
    }
    char* lastSep = strrchr(dir, '\\');
    if (!lastSep) {
        return 0;
    }
    *lastSep = '\0';

    isok = true;
    return dir;
}

PsyzModule* Psyz_ModuleOpen(const char* name, void* param) {
    char path[512];
    const char* exeDir = GetExeDir();
    if (exeDir) {
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s.dll", exeDir, name);
    } else {
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s.dll", name);
    }

    HMODULE handle = LoadLibraryA(path);
    if (!handle) {
        SetError("LoadLibraryA failed");
        return 0;
    }

    ModuleStartFn start =
        (ModuleStartFn)GetProcAddress(handle, "psyz_module_start");
    ModuleStopFn stop =
        (ModuleStopFn)GetProcAddress(handle, "psyz_module_stop");
    if (!start || !stop) {
        _snprintf_s(g_PsyzModuleLastError, sizeof(g_PsyzModuleLastError),
                    _TRUNCATE, "%s: PsyZ module entrypoints not found", path);
        FreeLibrary(handle);
        return 0;
    }

    PsyzModule* module = (PsyzModule*)malloc(sizeof(PsyzModule));
    if (!module) {
        _snprintf_s(g_PsyzModuleLastError, sizeof(g_PsyzModuleLastError),
                    _TRUNCATE, "%s: out of memory", path);
        FreeLibrary(handle);
        return 0;
    }
    module->handle = handle;
    module->stop = stop;
    module->param = param;
    start(param);
    return module;
}

void Psyz_ModuleClose(PsyzModule* module) {
    if (!module) {
        return;
    }
    module->stop(module->param);
    FreeLibrary(module->handle);
    free(module);
}
