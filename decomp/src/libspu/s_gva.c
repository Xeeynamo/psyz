#include "libspu_private.h"

void SpuNGetVoiceAttr(s32 vNum, SpuVoiceAttr* attr);
void SpuGetVoiceAttr(SpuVoiceAttr* attr) {
    s32 voice;
    s32 i;

    voice = -1;
    for (i = 0; i < NUM_VOICES; i++) {
        if (attr->voice & (1 << i)) {
            voice = i;
            break;
        }
    }

    if (voice != -1) {
        SpuNGetVoiceAttr(voice, attr);
    }
}
