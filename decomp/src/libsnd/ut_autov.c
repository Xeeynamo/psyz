#include "libsnd_private.h"

short SsUtAutoVol(
    short voice, short start_vol, short end_vol, short delta_time) {
    _autovol = SetAutoVol;
    if (voice >= 0 && voice < NUM_VOICES) {
        SeAutoVol(voice, start_vol, end_vol, delta_time);
        return 0;
    }
    return -1;
}