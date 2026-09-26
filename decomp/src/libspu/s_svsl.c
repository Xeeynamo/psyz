#include "libspu_private.h"

void SpuSetVoiceSL(s32 vNum, u16 SL) {
    u16 x;

    x = SPUR_RAW(vNum * 8 + 4) & 0xFFF0;
    x |= SL;
    SPUW_RAW(vNum * 8 + 4, x);
    _spu_Fw1ts();
}
