#include "libspu_private.h"

long SpuGetKeyStatus(u_long voice_bit) {
    volatile SPU_VOICE_REG* voices;
    long voice;
    int i;
    int voice_mask;
    u16 volumex;

    voice = -1;
    for (i = 0; i < NUM_VOICES; i++) {
        if (voice_bit & (1 << i)) {
            voice = i;
            break;
        }
    }
    if (voice == -1) {
        return -1;
    }
#ifndef __psyz
    voices = _spu_RXX->rxx.voice;
    volumex = voices[voice].volumex;
#else
    volumex = SPUR(voice[voice].volumex);
#endif
    voice_mask = 1 << voice;
    if (_spu_keystat & voice_mask) {
        if (volumex > 0) {
            return 1;
        } else {
            return 3;
        }
    } else if (volumex > 0) {
        return 2;
    } else {
        return 0;
    }
}
