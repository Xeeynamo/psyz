#include "libspu_private.h"

u_long _SpuSetAnyVoice(long on_off, u_long voice_bit, int lo_reg, int hi_reg) {
    u_long ret;

    if (_spu_env & 1) {
        ret = ((_spu_RQ[hi_reg - 0xC4] & 0xFF) << 16) | _spu_RQ[lo_reg - 0xC4];
    } else {
        ret = ((SPUR_RAW(hi_reg) & 0xFF) << 16) | SPUR_RAW(lo_reg);
    }

    switch (on_off) {
    case 1:
        if (_spu_env & 1) {
            _spu_RQ[lo_reg - 0xC4] |= voice_bit;
            _spu_RQ[hi_reg - 0xC4] |= (voice_bit >> 16) & 0xFF;
            _spu_RQmask |= on_off << ((lo_reg - 0xC6) >> 1);
        } else {
            SPUW_RAW(lo_reg, SPUR_RAW(lo_reg) | voice_bit);
            SPUW_RAW(hi_reg, SPUR_RAW(hi_reg) | ((voice_bit >> 16) & 0xFF));
        }
        ret |= voice_bit & 0xFFFFFF;
        break;
    case 0:
        if (_spu_env & 1) {
            _spu_RQ[lo_reg - 0xC4] &= ~voice_bit;
            _spu_RQ[hi_reg - 0xC4] &= ~((voice_bit >> 16) & 0xFF);
            _spu_RQmask |= 1 << ((lo_reg - 0xC6) >> 1);
        } else {
            SPUW_RAW(lo_reg, SPUR_RAW(lo_reg) & ~voice_bit);
            SPUW_RAW(hi_reg, SPUR_RAW(hi_reg) & ~((voice_bit >> 16) & 0xFF));
        }
        ret &= ~(voice_bit & 0xFFFFFF);
        break;
    }
    return ret & 0xFFFFFF;
}
