#include "libspu_private.h"

void SpuSetVoiceAttr(SpuVoiceAttr* attr) {
    _SpuRSetVoiceAttr(attr, 0, NUM_VOICES - 1, 0);
}
