#include "libspu_private.h"

void SpuFree(u_long addr) {
    int i;

    for (i = 0; i < _spu_AllocBlockNum; i++) {
        if ((u_long)_spu_memList[i].addr & 0x40000000) {
            break;
        }
        if (_spu_memList[i].addr == (u_long*)addr) {
            _spu_memList[i].addr =
                (u_long*)((u_long)_spu_memList[i].addr | 0x80000000);
            break;
        }
    }
    _spu_gcSPU();
}
