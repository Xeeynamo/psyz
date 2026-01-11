#include <psyz.h>
#include <libspu.h>
#include <log.h>

void SpuSetVoiceAttr(SpuVoiceAttr* arg) { DEBUGF("SpuSetVoiceAttr"); }

long SpuMallocWithStartAddr(u_long addr, long size) {
    NOT_IMPLEMENTED;
    return -1;
}

SpuIRQCallbackProc SpuSetIRQCallback(SpuIRQCallbackProc in) {
    NOT_IMPLEMENTED;
    return in;
}

long SpuSetTransferMode(long mode) {
    NOT_IMPLEMENTED;
    return 0;
}

long SpuClearReverbWorkArea(long rev_mode) {
    NOT_IMPLEMENTED;
    return 0;
}

void SpuSetKey(long on_off, u_long voice_bit) {
    DEBUGF("on_off %d voice_bit %08X", on_off, voice_bit);
}
