#include "libspu_private.h"

u_long SpuSetPitchLFOVoice(long on_off, u_long voice_bit) {
    return _SpuSetAnyVoice(on_off, voice_bit, 0xC8, 0xC9);
}
