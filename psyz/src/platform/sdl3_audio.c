#include <psyz.h>
#include <psyz/log.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "../audio.h"

#define CD_SAMPLE_RATE 44100
#define CD_CHANNELS 2
#define CD_SAMPLE_SIZE 2 // 16-bit = 2 bytes
#define SECTOR_SIZE 2352
#define SAMPLES_PER_SECTOR (SECTOR_SIZE / (CD_CHANNELS * CD_SAMPLE_SIZE))
#define BUFFER_SECTORS 8 // Read 8 sectors at a time

static SDL_AudioStream* sdl_stream;
static SDL_Thread* thread;
static SDL_Mutex* mutex;
static FILE* track_file;
static int is_playing;
static int is_muted;
static int thread_should_exit;
static void (*cd_audio_end_cb)(void);

static int audio_thread_func(void* data) {
    unsigned char buffer[SECTOR_SIZE * BUFFER_SECTORS];
    while (!thread_should_exit) {
        SDL_LockMutex(mutex);
        if (is_playing && !is_muted && track_file && sdl_stream) {
            int queued = SDL_GetAudioStreamQueued(sdl_stream);
            int len = CD_SAMPLE_RATE * CD_CHANNELS * CD_SAMPLE_SIZE / 2;
            if (queued < len) {
                size_t read = fread(buffer, 1, sizeof(buffer), track_file);
                if (read > 0) {
                    SDL_PutAudioStreamData(sdl_stream, buffer, read);
                } else {
                    DEBUGF("cd audio playbacl reached end of file");
                    is_playing = 0;
                    void (*callback)(void) = cd_audio_end_cb;
                    SDL_UnlockMutex(mutex);
                    if (callback) {
                        callback();
                    }
                    SDL_LockMutex(mutex);
                }
            }
        }
        SDL_UnlockMutex(mutex);
        SDL_Delay(10);
    }

    return 0;
}

int Audio_Init(void) {
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            ERRORF("failed to init SDL audio: %s", SDL_GetError());
            return -1;
        }
    }

    SDL_AudioSpec spec = {
        .format = SDL_AUDIO_S16,
        .channels = CD_CHANNELS,
        .freq = CD_SAMPLE_RATE,
    };
    sdl_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!sdl_stream) {
        ERRORF("failed to open audio device: %s", SDL_GetError());
        return -1;
    }
    SDL_ResumeAudioStreamDevice(sdl_stream);

    mutex = SDL_CreateMutex();
    if (!mutex) {
        ERRORF("failed to create audio mutex: %s", SDL_GetError());
        SDL_DestroyAudioStream(sdl_stream);
        return -1;
    }

    thread_should_exit = 0;
    thread = SDL_CreateThread(audio_thread_func, "AudioThread", NULL);
    if (!thread) {
        ERRORF("failed to create audio thread: %s", SDL_GetError());
        SDL_DestroyMutex(mutex);
        SDL_DestroyAudioStream(sdl_stream);
        return -1;
    }

    DEBUGF("audio initialized");
    return 0;
}

void Audio_Shutdown(void) {
    if (thread) {
        thread_should_exit = 1;
        SDL_WaitThread(thread, NULL);
        thread = NULL;
    }
    if (mutex) {
        SDL_DestroyMutex(mutex);
        mutex = NULL;
    }
    if (track_file) {
        fclose(track_file);
        track_file = NULL;
    }
    if (sdl_stream) {
        SDL_DestroyAudioStream(sdl_stream);
        sdl_stream = NULL;
    }
}

void Audio_SetCdAudioEndCB(void (*callback)(void)) {
    SDL_LockMutex(mutex);
    cd_audio_end_cb = callback;
    SDL_UnlockMutex(mutex);
}

void Audio_PlayCdAudio(FILE* file) {
    if (!file) {
        ERRORF("Audio_PlayCdAudio: file is NULL");
        return;
    }
    SDL_LockMutex(mutex);
    if (track_file) {
        fclose(track_file);
        track_file = NULL;
    }
    track_file = file;
    if (sdl_stream) {
        SDL_ClearAudioStream(sdl_stream);
    }
    is_playing = 1;
    SDL_UnlockMutex(mutex);
}

void Audio_Stop(void) {
    SDL_LockMutex(mutex);
    is_playing = 0;
    if (track_file) {
        fclose(track_file);
        track_file = NULL;
    }
    if (sdl_stream) {
        SDL_ClearAudioStream(sdl_stream);
    }
    SDL_UnlockMutex(mutex);
}

void Audio_Unpause(void) {
    SDL_ResumeAudioStreamDevice(sdl_stream);
}

void Audio_Pause(void) {
    SDL_PauseAudioStreamDevice(sdl_stream);
}

void Audio_Mute(void) {
    SDL_LockMutex(mutex);
    is_muted = 1;
    if (sdl_stream) {
        SDL_ClearAudioStream(sdl_stream);
    }
    SDL_UnlockMutex(mutex);
}

void Audio_Demute(void) {
    SDL_LockMutex(mutex);
    is_muted = 0;
    SDL_UnlockMutex(mutex);
}
