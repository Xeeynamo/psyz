#include "libsnd_private.h"

char SsSetReservedVoice(char voices) {
    char temp_v1;

    if (voices > NUM_VOICES || voices == 0) {
        return -1;
    }
    _SsVmMaxVoice = voices;
    return _SsVmMaxVoice;
}
