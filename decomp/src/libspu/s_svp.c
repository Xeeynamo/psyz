#include "libspu_private.h"

void SpuSetVoicePitch(s32 vNum, u16 pitch) {
    SPUW_RAW(vNum * 8 + 2, pitch);
    _spu_Fw1ts();
}
