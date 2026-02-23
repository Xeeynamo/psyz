#include "libspu_private.h"

s32 _SpuIsInAllocateArea(u32 arg0) {
    s32 i;

    if (_spu_memList == NULL) {
        return 0;
    }
    for (i = 0;; i++) {
        if ((u_long)_spu_memList[i].addr & 0x80000000) {
            continue;
        }
        if ((u_long)_spu_memList[i].addr & 0x40000000) {
            break;
        }
        if (arg0 <= ((u_long)_spu_memList[i].addr & 0x0FFFFFFF)) {
            return 1;
        }
        if (arg0 < ((u_long)_spu_memList[i].addr & 0x0FFFFFFF) +
                       _spu_memList[i].size) {
            return 1;
        }
    }
    return 0;
}

s32 _SpuIsInAllocateArea_(u32 arg0) {
    s32 i;

    arg0 <<= _spu_mem_mode_plus;
    if (_spu_memList == NULL) {
        return 0;
    }
    for (i = 0; 1; i++) {
        if ((u_long)_spu_memList[i].addr & 0x80000000) {
            continue;
        }
        if ((u_long)_spu_memList[i].addr & 0x40000000) {
            break;
        }
        if (arg0 <= ((u_long)_spu_memList[i].addr & 0x0FFFFFFF)) {
            return 1;
        }
        if (arg0 < ((u_long)_spu_memList[i].addr & 0x0FFFFFFF) +
                       _spu_memList[i].size) {
            return 1;
        }
    }
    return 0;
}
