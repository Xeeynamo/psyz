#include "libspu_private.h"

void SpuSetVoiceLoopStartAddr(s32 vNum, u_long lsa) {
    _spu_FsetRXXa(vNum * 8 + 7, lsa);
    _spu_Fw1ts();
}
