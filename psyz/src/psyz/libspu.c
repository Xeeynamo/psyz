#include <psyz.h>
#include <libspu.h>
#include <psyz/log.h>
#include "../../decomp/src/libspu/libspu_private.h"

static SPU_RXX spu_RXX;
static unsigned _dma_dpcr;
union SpuUnion* _spu_RXX = (union SpuUnion*)&spu_RXX;
unsigned* dma_dpcr = &_dma_dpcr; // TODO this must be removed
volatile u16 _spu_RQ[10] = {0};

void SpuSetVoiceAttr(SpuVoiceAttr* arg) { NOT_IMPLEMENTED; }

long SpuSetReverbModeParam(SpuReverbAttr* attr) {
    NOT_IMPLEMENTED;
    return 0;
}

u_long _SpuSetAnyVoice(long on_off, u_long voice_bit, int arg2, int arg3) {
    NOT_IMPLEMENTED;
    return 0;
}
