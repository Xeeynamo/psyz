// Simple PCM to sceAudio, uses the PsyZ SPU software emulation
#include <pspaudio.h>
#include <pspkernel.h>
#include <psyz.h>
#include <psyz/log.h>
#include <string.h>

#define N_CHANNELS 2
#define BUF_FRAMES 1024 // must be a multiple of 64 for sceAudio

static int chan = -1;
static SceUID sema = -1;
static SceUID thid = -1;
static volatile int stop_requested;
static volatile int is_paused;
static int is_audio_init;

static int AudioThread(SceSize args, void* argp) {
    // double-buffered so the SPU can mix one block while the other one plays
    static short __attribute__((aligned(64))) buf[2][BUF_FRAMES * N_CHANNELS];
    int cur = 0;
    (void)args;
    (void)argp;
    while (!stop_requested) {
        Psyz_AudioLock();
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
    sceKernelStartThread(thid, 0, NULL);
    is_audio_init = 1;
    DEBUGF("audio initialized");
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
    is_audio_init = 0;
}

static SceUID lock_owner = -1;
static int lock_depth;
void Psyz_AudioLock(void) {
    if (sema < 0) {
        return;
    }
    SceUID self = sceKernelGetThreadId();
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
