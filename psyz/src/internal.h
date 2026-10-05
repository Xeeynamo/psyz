// Private header for PSY-Z internal macros
// Not part of the public API - do not include from external code

#ifndef PSYZ_INTERNAL_H
#define PSYZ_INTERNAL_H

#define INCLUDE_ASM(path, func)
#define WEAK_INCLUDE_ASM(path, func)
#define NOP

#ifndef LEN
#define LEN(x) ((s32)(sizeof(x) / sizeof(*(x))))
#endif

#ifndef LENU
#define LENU(x) ((u32)(sizeof(x) / sizeof(*(x))))
#endif

#ifndef CLAMP
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#endif

#define VRAM_W 1024
#define VRAM_H 512

// SDL platform lifecycle code uses this to avoid undoing an explicit game
// audio pause when the application returns from the background.
int Psyz_AudioIsPaused(void);

// Delivers a kernel event as an emulated device interrupt would: right away,
// or at ExitCriticalSection when the caller is inside a critical section.
void Psyz_KernelRaise(unsigned int desc, unsigned int spec);

// Runs fn as an interrupt on the kernel's interrupt thread. Never blocks, so
// the audio thread can use it.
void Psyz_KernelPost(void (*fn)(void));

// Raises the vblank interrupt; VSync calls it once per emulated frame.
void Psyz_KernelVBlank(void);

// Services due timers when the host has no thread to run them.
void Psyz_KernelPoll(void);

#endif
