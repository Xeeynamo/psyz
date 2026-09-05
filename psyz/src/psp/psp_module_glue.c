// same shim as the platform/ counterpart, with some crucial differences:
// - module name must be declare, inherit stack size of parent process.
// - module_start and module_stop are native PRX entrypoint callbacks.
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
extern int psyz_var_import_apply(unsigned int (*resolve)(unsigned int nid));
extern unsigned int host_resolve_var(unsigned int nid);
#endif

int module_start(SceSize args, void* argp) {
#if PSYZ_MODULE_HAS_VAR_IMPORTS
    if (psyz_var_import_apply(host_resolve_var) < 0) {
        return -1;
    }
#endif
    Psyz_ModuleStart((argp && args >= sizeof(void*)) ? *(void**)argp : 0);
    return 0;
}

int module_stop(SceSize args, void* argp) {
    (void)args;
    (void)argp;
    Psyz_ModuleStop();
    return 0;
}
