#ifndef PSYZ_IOS_PLATFORM_H
#define PSYZ_IOS_PLATFORM_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

int Psyz_IosAdjustPath(char* dst, const char* src, int maxlen);

void Psyz_IosAttachWindow(SDL_Window* window);
void Psyz_IosDetachWindow(void);

void Psyz_IosSetTouchControlsVisible(int visible);
int Psyz_IosTouchControlsActive(void);
unsigned int Psyz_IosReadTouchControls(void);
void Psyz_IosResetTouchControls(void);

#ifdef __cplusplus
}
#endif

#endif
