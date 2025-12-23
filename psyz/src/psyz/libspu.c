#include <psyz.h>
#include <libspu.h>
#include <log.h>
#include "../../decomp/src/libspu/libspu_private.h"

static SPU_RXX spu_RXX;
union SpuUnion* _spu_RXX = (union SpuUnion*)&spu_RXX;

void _SpuInit(s32 arg0) { NOT_IMPLEMENTED; }

s32 _spu_t(s32 arg0, ...) {
    NOT_IMPLEMENTED;
    return 0;
}

void SpuSetVoiceAttr(SpuVoiceAttr* arg) { NOT_IMPLEMENTED; }

void SpuSetCommonAttr(SpuCommonAttr* arg) { NOT_IMPLEMENTED; }

long SpuSetReverbModeParam(SpuReverbAttr* attr) {
    NOT_IMPLEMENTED;
    return 0;
}

long SpuMallocWithStartAddr(u_long addr, long size) {
    NOT_IMPLEMENTED;
    return -1;
}

SpuIRQCallbackProc SpuSetIRQCallback(SpuIRQCallbackProc in) {
    NOT_IMPLEMENTED;
    return in;
}

long SpuSetTransferMode(long mode) {
    NOT_IMPLEMENTED;
    return 0;
}

void SpuSetKey(long on_off, unsigned long voice_bit) { NOT_IMPLEMENTED; }
