#include <psyz.h>
#include <libspu.h>
#include <psyz/log.h>
#include "../../decomp/src/libspu/libspu_private.h"

static SPU_RXX spu_RXX;
union SpuUnion* _spu_RXX = (union SpuUnion*)&spu_RXX;

void _spu_init(int bHot) { NOT_IMPLEMENTED; }

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

void SpuFree(u_long* addr) {
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
