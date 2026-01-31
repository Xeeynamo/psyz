#ifndef PSYZ_OVERLAY_SDL3_GL_H
#define PSYZ_OVERLAY_SDL3_GL_H
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called once after SDL3/OpenGL initialization completes.
// Use this to initialize overlay backends (e.g., ImGui_ImplSDL3_InitForOpenGL).
typedef void (*PsyzOverlayInitCB_SDL3GL)(SDL_Window* window, SDL_GLContext ctx);

// Called for each SDL3 input event.
// Use this to forward events to overlay input handling (e.g., ImGui).
typedef void (*PsyzOverlayEventCB_SDL3)(const SDL_Event* event);

// Register the SDL3+OpenGL initialization callback.
// Returns the previous callback, or NULL if none was set.
PsyzOverlayInitCB_SDL3GL Psyz_OverlayInit_SDL3GL(PsyzOverlayInitCB_SDL3GL cb);

// Register the SDL3 event callback.
// Returns the previous callback, or NULL if none was set.
PsyzOverlayEventCB_SDL3 Psyz_OverlayEvent_SDL3(PsyzOverlayEventCB_SDL3 cb);

#ifdef __cplusplus
}
#endif

#endif // PSYZ_OVERLAY_SDL3_GL_H
