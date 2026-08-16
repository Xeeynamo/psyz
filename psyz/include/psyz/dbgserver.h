#ifndef PSYZ_DBGSERVER_H
#define PSYZ_DBGSERVER_H

/**
 * @file dbgserver.h
 * @brief HTTP debug server for external tooling.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts and binds the runtime debug server on 127.0.0.1:<port>.
 *
 * Registers a VSync callback (via Psyz_SetVSyncCb) that executes queued
 * commands from the game thread; safe to call once from anywhere before or
 * after the game loop starts. Only one server instance can run at a time.
 *
 * @param port TCP port to bind, or 0 to let the OS pick an ephemeral port
 * @return bound port on success, -1 on failure
 */
int Psyz_DebugServer(int port);

/**
 * @brief Stop the debug server.
 *
 * Safe to call if the server is not running.
 */
void Psyz_DebugServerStop(void);

#ifdef __cplusplus
}
#endif

#endif
