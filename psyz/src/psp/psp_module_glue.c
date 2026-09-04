// same shim as the platform/ counterpart, with some crucial differences:
// - module name must be declare, inherit stack size of parent process.
// - module_start and module_stop are native PRX entrypoint callbacks.
// - ctor and dtor for PRXs are not supported, must call manually.
#include <pspkernel.h>
#include <pspmoduleinfo.h>
#include <psyz/module.h>

// PRX modules must have a different name, or the OS will reject them.
// Module name is resolved in function(psyz_add_module MODULE_NAME)
#ifndef PSYZ_MODULE_NAME
#define PSYZ_MODULE_NAME "psyz_module"
#endif

PSP_MODULE_INFO(PSYZ_MODULE_NAME, 0, 1, 0);

#if PSYZ_MODULE_HAS_VAR_IMPORTS
// Defined by psyz_var_import_apply.c and the generated <library>_resolve.c
extern void psyz_var_import_apply(unsigned int (*resolve)(unsigned int nid));
extern unsigned int host_resolve_var(unsigned int nid);
#endif

// Bounds of .ctors/.dtors, provided by psyz/src/psp/linkfile_psyz_prx
extern void (*__psyz_ctors_start[])(void);
extern void (*__psyz_ctors_end[])(void);
extern void (*__psyz_dtors_start[])(void);
extern void (*__psyz_dtors_end[])(void);

static void RunArray(void (**begin)(void), void (**end)(void), int reverse) {
    int count = (int)(end - begin);
    for (int i = 0; i < count; i++) {
        void (*fn)(void) = begin[reverse ? count - 1 - i : i];
        if (fn && fn != (void (*)(void))-1) {
            fn();
        }
    }
}

int module_start(SceSize args, void* argp) {
#if PSYZ_MODULE_HAS_VAR_IMPORTS
    // before ctor: a constructor may already touch an imported host variable.
    psyz_var_import_apply(host_resolve_var);
#endif
    RunArray(__psyz_ctors_start, __psyz_ctors_end, 0);
    Psyz_ModuleStart((argp && args >= sizeof(void*)) ? *(void**)argp : 0);
    return 0;
}

int module_stop(SceSize args, void* argp) {
    (void)args;
    (void)argp;
    Psyz_ModuleStop();
    RunArray(__psyz_dtors_start, __psyz_dtors_end, 1);
    return 0;
}
