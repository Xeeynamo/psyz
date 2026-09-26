#include "libspu_private.h"

void SpuSetVoiceSRAttr(s32 vNum, u16 SR, long SRmode) {
    s32 var_a3;
    u16 x;
    int y;

    var_a3 = 0x100;
    y = vNum * 8;
    switch (SRmode) {
    case 1:
        var_a3 = 0;
        break;
    case 5:
        var_a3 = 0x200;
        break;
    case 7:
        var_a3 = 0x300;
        break;
    }
    x = SPUR_RAW(y + 5);
    x &= 0x3F;
    x |= (SR | var_a3) << 6;
    SPUW_RAW(y + 5, x);
    _spu_Fw1ts();
}
