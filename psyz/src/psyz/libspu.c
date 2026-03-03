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

void _spu_Fw1ts(void) {}
void _spu_FwriteByIO(void* addr, int len) { NOT_IMPLEMENTED; }

s32 _spu_t(s32 arg0, ...) {
    NOT_IMPLEMENTED;
    return 0;
}

void _spu_FsetRXX(u32 offset, u32 value, u32 mode) { NOT_IMPLEMENTED; }

void _spu_FiDMA(void) { NOT_IMPLEMENTED; }

void SpuSetVoiceAttr(SpuVoiceAttr* arg) { NOT_IMPLEMENTED; }

long SpuSetReverbModeParam(SpuReverbAttr* attr) {
    NOT_IMPLEMENTED;
    return 0;
}

SpuIRQCallbackProc SpuSetIRQCallback(SpuIRQCallbackProc in) {
    NOT_IMPLEMENTED;
    return in;
}

long SpuSetTransferMode(long mode) {
    NOT_IMPLEMENTED;
    return 0;
}

void SpuSetKey(long on_off, u_long voice_bit) { NOT_IMPLEMENTED; }

long SpuInitMalloc(long num, char* top) {
    NOT_IMPLEMENTED; // can't use original implementation
}

u_long SpuSetTransferStartAddr(u_long addr) {
    NOT_IMPLEMENTED;
    return 0;
}

u_long SpuWrite(u_char* addr, u_long size) {
    NOT_IMPLEMENTED;
    return 0;
}

long* SpuMallocWithStartAddr(long* addr, long size) {
    NOT_IMPLEMENTED;
    return (long*)-1;
}

long* SpuMalloc(long size) {
    NOT_IMPLEMENTED;
    return (long*)-1;
}

void SpuFree(long* addr) {
    for (int i = 0; i < _spu_AllocBlockNum; i++) {
        if (!_spu_memList[i].addr) {
            continue;
        }
        if (_spu_memList[i].addr == addr) {
            NOT_IMPLEMENTED; // can't use original implementation
            break;
        }
    }
}

u_long SpuSetNoiseVoice(long on_off, u_long voice_bit) {
    NOT_IMPLEMENTED;
    return 0;
}
