#include "libspu_private.h"

void SpuGetReverbModeParam(SpuReverbAttr* attr) {
    attr->mode = _spu_rev_attr.mode;
    attr->delay = _spu_rev_attr.delay;
    attr->feedback = _spu_rev_attr.feedback;
    attr->depth.left = _spu_rev_attr.depth.left;
    attr->depth.right = _spu_rev_attr.depth.right;
}
