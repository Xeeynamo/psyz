#include <psyz.h>
#include <libspu.h>
#include <log.h>

void _SpuInit(void) { NOT_IMPLEMENTED; }

void SpuSetVoiceAttr(SpuVoiceAttr* arg) { NOT_IMPLEMENTED; }

void SpuSetCommonAttr(SpuCommonAttr* arg) { NOT_IMPLEMENTED; }

long SpuSetReverb(long on_off) {
    NOT_IMPLEMENTED;
    return on_off;
}

long SpuSetReverbModeParam(SpuReverbAttr* attr) {
    NOT_IMPLEMENTED;
    return 0;
}

long SpuMallocWithStartAddr(unsigned long addr, long size) {
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

void SpuSetKey(long on_off, unsigned long voice_bit) {
    DEBUGF("on_off %d voice_bit %08X", on_off, voice_bit);
}
