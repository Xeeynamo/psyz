#include "libspu_private.h"

static inline void assign(u_short val, short* out) {
    u_long offset = 0x8000;
    u_long uval = val;

    if (uval >= 0x4000) {
        *out = uval - offset;
    } else {
        *out = val;
    }
}

void SpuGetVoiceVolume(int vNum, short* volL, short* volR) {
    u_short left = SPURV(vNum, volume.left);
    u_short right = SPURV(vNum, volume.right);

    assign(left, volL);
    assign(right, volR);
}
