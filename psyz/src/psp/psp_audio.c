// Simple PCM to sceAudio, mixed by the sceSasCore SPU in psp_sas.c
#include <pspaudio.h>
#include <pspkernel.h>
#include <psyz.h>
#include <psyz/log.h>
#include <stdint.h>
#include <string.h>
#include "psp.h"

#define N_CHANNELS 2
#define BUF_FRAMES 512 // must be a multiple of 64 for sceAudio

static int chan = -1;
static SceUID sema = -1;
static SceUID thid = -1;
static volatile int stop_requested;
static volatile int is_paused;
static int is_audio_init;
static uintptr_t audio_stack_lo, audio_stack_hi;

void Psyz_SpuSasPrerender(void);
static int AudioThread(SceSize args, void* argp) {
    // double-buffered so the SPU can mix one block while the other one plays
    static short __attribute__((aligned(64))) buf[2][BUF_FRAMES * N_CHANNELS];
    int cur = 0;
    (void)args;
    (void)argp;
    while (!stop_requested) {
        if (!is_paused) {
            Psyz_SpuSasPrerender();
        }
        Psyz_AudioLock();
        for (int i = 0; i < (int)sizeof(buf[cur]); i += 64) {
            psp_dcache_claim_line((char*)buf[cur] + i);
        }
        if (is_paused) {
            memset(buf[cur], 0, sizeof(buf[cur]));
        } else {
            Psyz_SpuPullSamples(buf[cur], BUF_FRAMES);
        }
        Psyz_AudioUnlock();
        sceAudioOutputBlocking(chan, PSP_AUDIO_VOLUME_MAX, buf[cur]);
        cur ^= 1;
    }
    return 0;
}

int Psyz_AudioInit(void) {
    if (is_audio_init) {
        return 0;
    }
    Psyz_SpuInit();
    chan = sceAudioChReserve(
        PSP_AUDIO_NEXT_CHANNEL, BUF_FRAMES, PSP_AUDIO_FORMAT_STEREO);
    if (chan < 0) {
        ERRORF("failed to reserve an audio channel: %08x", chan);
        return -1;
    }
    sema = sceKernelCreateSema("psyz_audio_lock", 0, 1, 1, NULL);
    if (sema < 0) {
        ERRORF("failed to create the audio semaphore: %08x", sema);
        sceAudioChRelease(chan);
        chan = -1;
        return -1;
    }
    stop_requested = 0;
    is_paused = 0;
    thid =
        sceKernelCreateThread("psyz_audio", AudioThread, 0x12, 0x4000, 0, NULL);
    if (thid < 0) {
        ERRORF("failed to create the audio thread: %08x", thid);
        sceKernelDeleteSema(sema);
        sceAudioChRelease(chan);
        sema = -1;
        chan = -1;
        return -1;
    }
    SceKernelThreadInfo info;
    memset(&info, 0, sizeof(info));
    info.size = sizeof(info);
    if (sceKernelReferThreadStatus(thid, &info) == 0) {
        audio_stack_lo = (uintptr_t)info.stack;
        audio_stack_hi = audio_stack_lo + (uintptr_t)info.stackSize;
    }
    is_audio_init = 1;
    DEBUGF("audio initialized");
    sceKernelStartThread(thid, 0, NULL);
    return 0;
}

void Psyz_AudioDestroy(void) {
    if (!is_audio_init) {
        return;
    }
    stop_requested = 1;
    sceKernelWaitThreadEnd(thid, NULL);
    sceKernelDeleteThread(thid);
    sceKernelDeleteSema(sema);
    sceAudioChRelease(chan);
    thid = -1;
    sema = -1;
    chan = -1;
    audio_stack_lo = audio_stack_hi = 0;
    is_audio_init = 0;
}

static SceUID CurrentThread(void) {
    // sceKernelGetThreadId is a slow syscall that could introduce latency
    // spikes during the execution. The following temporary hack allows to
    // detect the current thread ID via its stack pointer, saving a syscall.
    uintptr_t sp = (uintptr_t)__builtin_frame_address(0);
    if (sp - audio_stack_lo < audio_stack_hi - audio_stack_lo) {
        return thid;
    }
    return sceKernelGetThreadId();
}

static SceUID lock_owner = -1;
static int lock_depth;
void Psyz_AudioLock(void) {
    if (sema < 0) {
        return;
    }
    SceUID self = CurrentThread();
    if (lock_owner == self) {
        lock_depth++;
        return;
    }
    sceKernelWaitSema(sema, 1, NULL);
    lock_owner = self;
    lock_depth = 1;
}

void Psyz_AudioUnlock(void) {
    if (sema < 0) {
        return;
    }
    if (lock_depth > 1) {
        lock_depth--;
        return;
    }
    lock_owner = -1;
    lock_depth = 0;
    sceKernelSignalSema(sema, 1);
}

void Psyz_AudioPause(void) { is_paused = 1; }

void Psyz_AudioUnpause(void) { is_paused = 0; }
