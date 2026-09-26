#include "libspu_private.h"

void SpuSetVoiceDR(s32 vNum, u16 DR) {
    u16 x;

    x = SPUR_RAW(vNum * 8 + 4) & 0xFF0F;
    x |= DR << 4;
    SPUW_RAW(vNum * 8 + 4, x);
    _spu_Fw1ts();
}
