#include "libspu_private.h"

void SpuSetKey(long on_off, u_long voice_bit) {
    u_short lo_bits;
    u_short hi_bits;

    voice_bit &= 0xFFFFFF;
    lo_bits = voice_bit;
    hi_bits = voice_bit >> 16;

    switch (on_off) {
    case 1:
        if (_spu_env & 1) {
            _spu_RQ[0] = lo_bits;
            _spu_RQ[1] = hi_bits;
            _spu_RQmask |= 1;
            _spu_RQvoice |= voice_bit;
            if (_spu_RQ[2] & voice_bit) {
                _spu_RQ[2] &= ~voice_bit;
            }
            if (_spu_RQ[3] & hi_bits) {
                _spu_RQ[3] &= ~hi_bits;
            }
        } else {
            SPUW(key_on[0], lo_bits);
            SPUW(key_on[1], hi_bits);
            _spu_keystat |= voice_bit;
        }
        break;
    case 0:
        if (_spu_env & 1) {
            _spu_RQ[2] = lo_bits;
            _spu_RQ[3] = hi_bits;
            _spu_RQmask |= 1;
            _spu_RQvoice &= ~voice_bit;
            if (_spu_RQ[0] & voice_bit) {
                _spu_RQ[0] &= ~voice_bit;
            }
            if (_spu_RQ[1] & hi_bits) {
                _spu_RQ[1] &= ~hi_bits;
            }
        } else {
            SPUW(key_off[0], lo_bits);
            SPUW(key_off[1], hi_bits);
            _spu_keystat &= ~voice_bit;
        }
        break;
    }
}
