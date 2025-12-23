#ifndef PSYZ_AUDIO_H
#define PSYZ_AUDIO_H

int Audio_Init(void);
void Audio_Shutdown(void);
void Audio_SetCdAudioEndCB(void (*callback)(void));
void Audio_PlayCdAudio(FILE* file);
void Audio_Stop(void);
void Audio_Pause(void);
void Audio_Mute(void);
void Audio_Demute(void);

#endif
