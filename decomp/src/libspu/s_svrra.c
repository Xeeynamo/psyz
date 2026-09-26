#include "libspu_private.h"

void SpuSetVoiceRRAttr(s32 vNum, u16 RR, long RRmode) {
    u16 x;
    s32 var_a3;

    var_a3 = 0;
    if (RRmode != 3) {
        var_a3 = (RRmode == 7) << 5;
    }

    x = SPUR_RAW(vNum * 8 + 5);
    x &= 0xFFC0;
    x |= RR | var_a3;

    SPUW_RAW(vNum * 8 + 5, x);
    _spu_Fw1ts();
}
