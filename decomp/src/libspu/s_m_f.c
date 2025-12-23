#include "libspu_private.h"

extern s32 _spu_AllocBlockNum;

void SpuFree(u_long addr) {
    s32 i;

    for (i = 0; i < _spu_AllocBlockNum; i++) {
        if (_spu_memList[i].addr & 0x40000000) {
            break;
        }
        if (_spu_memList[i].addr == addr) {
            _spu_memList[i].addr |= 0x80000000;
            break;
        }
    }
    _spu_gcSPU();
}
