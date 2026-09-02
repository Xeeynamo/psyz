#include "libspu_private.h"

typedef struct {
    u16 unk0; // key_on
    u16 unk2; // key_on
    u16 unk4; // key off
    u16 unk6; // key off
} SpuRQ;

extern volatile s32 _spu_RQmask;
extern volatile s32 _spu_RQvoice;
extern volatile SpuRQ _spu_RQ;

void SpuSetKey(long on_off, u_long voice_bit) {
    u_short lo_bits;
    u_short hi_bits;

    voice_bit &= 0xFFFFFF;
    lo_bits = voice_bit;
    hi_bits = voice_bit >> 16;

    switch (on_off) {
    case 1:
        if (_spu_env & 1) {
            _spu_RQ.unk0 = lo_bits;
            _spu_RQ.unk2 = hi_bits;
            _spu_RQmask |= 1;
            _spu_RQvoice |= voice_bit;
            if (_spu_RQ.unk4 & voice_bit) {
                _spu_RQ.unk4 &= ~voice_bit;
            }
            if (_spu_RQ.unk6 & hi_bits) {
                _spu_RQ.unk6 &= ~hi_bits;
            }
        } else {
            SPUW(key_on[0], lo_bits);
            SPUW(key_on[1], hi_bits);
            _spu_keystat |= voice_bit;
        }
        break;
    case 0:
        if (_spu_env & 1) {
            _spu_RQ.unk4 = lo_bits;
            _spu_RQ.unk6 = hi_bits;
            _spu_RQmask |= 1;
            _spu_RQvoice &= ~voice_bit;
            if (_spu_RQ.unk0 & voice_bit) {
                _spu_RQ.unk0 &= ~voice_bit;
            }
            if (_spu_RQ.unk2 & hi_bits) {
                _spu_RQ.unk2 &= ~hi_bits;
            }
        } else {
            SPUW(key_off[0], lo_bits);
            SPUW(key_off[1], hi_bits);
            _spu_keystat &= ~voice_bit;
        }
        break;
    }
}
