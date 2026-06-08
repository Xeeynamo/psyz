#include <psyz.h>
#include <libspu.h>
#include <psyz/log.h>
#include "../../decomp/src/libspu/libspu_private.h"

static SPU_RXX spu_RXX;
static u32 _dma_spu_madr;
static u32 _dma_spu_bcr;
static u32 _dma_spu_chcr;
static u32 _dma_dpcr;
static u32 _spu_delay;
union SpuUnion* _spu_RXX = (union SpuUnion*)&spu_RXX;
u32* dma_spu_madr = &_dma_spu_madr;
u32* dma_spu_bcr = &_dma_spu_bcr;
u32* dma_spu_chcr = &_dma_spu_chcr;
u32* dma_dpcr = &_dma_dpcr;
u32* spu_delay = &_spu_delay;
volatile u16 _spu_RQ[10] = {0};

s32 _spu_t(s32 arg0, ...) {
    NOT_IMPLEMENTED;
    return 0;
}

void SpuSetVoiceAttr(SpuVoiceAttr* arg) { NOT_IMPLEMENTED; }

long SpuSetReverbModeParam(SpuReverbAttr* attr) {
    NOT_IMPLEMENTED;
    return 0;
}

u_long _SpuSetAnyVoice(long on_off, u_long voice_bit, int arg2, int arg3) {
    NOT_IMPLEMENTED;
    return 0;
}
