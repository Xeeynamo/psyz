#include "libspu_private.h"

void SpuSetVoiceARAttr(s32 vNum, u16 AR, long ARmode) {
    u8 x;
    u16 flg;

    x = SPUR_RAW(vNum * 8 + 4 + 0);
    flg = (ARmode == 5) << 7;
    AR |= flg;
    AR <<= 8;

    SPUW_RAW(vNum * 8 + 4 + 0, x | AR);
    _spu_Fw1ts();
}
