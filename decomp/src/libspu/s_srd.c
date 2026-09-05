#include "libspu_private.h"

long SpuSetReverbDepth(SpuReverbAttr* attr) {
    u32 mask = attr->mask;
    s32 bSetAll = attr->mask == 0;

    if (bSetAll || (mask & SPU_REV_DEPTHL)) {
        SPUW(rev_vol.left, attr->depth.left);
        _spu_rev_attr.depth.left = attr->depth.left;
    }
    if (bSetAll || (mask & SPU_REV_DEPTHR)) {
        SPUW(rev_vol.right, attr->depth.right);
        _spu_rev_attr.depth.right = attr->depth.right;
    }
    return 0;
}
