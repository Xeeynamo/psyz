#include "libspu_private.h"

void SpuSetVoiceStartAddr(s32 vNum, u_long startAddr) {
    _spu_FsetRXXa(vNum * 8 + 3, startAddr);
    _spu_Fw1ts();
}
