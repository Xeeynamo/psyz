#ifndef PSYZ_MODULE_H
#define PSYZ_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PsyzModule PsyzModule;

/**
 * @brief Load a module dynamically to simulate PS1 overlays
 *
 * Modules will be able to use global variables and functions from the host
 * process that loads them. A module will offer the APIs Psyz_ModuleStart and
 * Psyz_ModuleStop to allocate and free module resources, abstracted from the
 * operating system specifics.
 *
 * Use psyz_exports(${PROJECT_NAME}) for the main executable to export all
 * the public symbols accessible from the modules.
 * Use psyz_add_module(MODULE_NAME SOURCES $SOURCE_LIST HOST ${PROJECT_NAME})
 * to create MODULE_NAME.dll (or similar), and link to it all public symbols
 * from the specified host.
 *
 * The module path is relative to the executable location, not relative to the
 * current working directory. Alternatively, specify an absolute path.
 *
 * Features:
 * - Recompile and swap modules at runtime without closing the main executable.
 * - Modules can load other modules.
 * - Modules can invoke functions from HOST.
 * - Modules can read and write HOST global variables.
 * - Modules can export its symbols to HOST via a struct passed as param.
 *
 * Known limitations:
 * - PSP modules do not run constructors or destructors.
 * - The module name must be unique, otherwise it will fail to load on PSP.
 * - A module is unique to its HOST, and can't be loaded elsewhere.
 * - The same module cannot be loaded more than once.
 * - Recompiling HOST must follow all its modules to be re-linked to it.
 *
 * Supported targets:
 * - Linux: shared object .so
 * - macOS: .dylib
 * - Windows: .dll
 * - PSP: .prx
 * Unverified or unsupported:
 * - iOS
 * - emscripten
 *
 * @param name Module name or path, without its extension.
 * @param param Parameter passed to Psyz_ModuleStart(void* param).
 * @return Opaque handle, or NULL on failure.
 */
PsyzModule* Psyz_ModuleOpen(const char* name, void* param);

/**
 * @brief Call the module's Psyz_ModuleStop() and release module.
 *
 * @param module Handle returned by Psyz_ModuleOpen(). NULL is a no-op.
 */
void Psyz_ModuleClose(PsyzModule* module);

/**
 * @brief Called once when the module is loaded.
 *
 * NOTE: This function is called in the module. The function is not meant to
 * be called manually.
 *
 * @param param The pointer the host passed to Psyz_ModuleOpen().
 */
void Psyz_ModuleStart(void* param);

/**
 * @brief Called once, before the module is unloaded.
 *
 * NOTE: This function is called in the module. The function is not meant to
 * be called manually.
 */
void Psyz_ModuleStop(void);

#ifdef __cplusplus
}
#endif

#endif
