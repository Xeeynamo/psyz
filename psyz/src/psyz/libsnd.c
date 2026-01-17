#include <psyz.h>
#include <libspu.h>
#include <libsnd.h>
#include <psyz/log.h>

short _svm_stereo_mono;
SpuReverbAttr _svm_rattr;
u_long _snd_seq_tick_env[0x100]; // HACK: make it tick enough

void _SsInit(void) {
    _svm_stereo_mono = 0;
    NOT_IMPLEMENTED;
}

s32 _SpuIsInAllocateArea_(u32 arg0) {
    NOT_IMPLEMENTED;
    return 0;
}

short SsUtSetVVol(short vc, short voll, short volr) {
    NOT_IMPLEMENTED;
    return 0;
}

void Snd_SetPlayMode(
    short sep_access_num, short seq_num, char play_mode, short l_count) {
    NOT_IMPLEMENTED;
}

void SsSetMVol(short voll, short volr) { NOT_IMPLEMENTED; }

void SsSeqSetVol(short seq_access_num, short voll, short volr) {
    NOT_IMPLEMENTED;
}

short SsSeqOpen(u_long* addr, short vab_id) {
    NOT_IMPLEMENTED;
    return 0;
}

void SsSeqStop(short seq_access_num) { NOT_IMPLEMENTED; }

short SsUtKeyOnV(short voice, short vabId, short prog, short tone, short note,
                 short fine, short voll, short volr) {
    NOT_IMPLEMENTED;
    return 0;
}

void SsStart(void) { NOT_IMPLEMENTED; }

char SsSetReservedVoice(char voices) {
    NOT_IMPLEMENTED;
    return 0;
}

void MySsInitHot();
void SsInitHot(void) { MySsInitHot(); }

short SsVabTransCompleted(short immediateFlag) {
    NOT_IMPLEMENTED;
    return 1;
}

short SsVabTransBodyPartly(u_char* addr, u_long bufsize, short vabid) {
    NOT_IMPLEMENTED;
    return 0;
}

short SsVabOpenHeadWithMode(
    u_char* addr, short vabid, short mode, u_long sbaddr) {
    NOT_IMPLEMENTED;
    return 0;
}

short SsVabTransBody(unsigned char* addr, short vabid) {
    NOT_IMPLEMENTED;
    return 0;
}
void SsVabClose(short vab_id) { NOT_IMPLEMENTED; }

void SsEnd(void) { NOT_IMPLEMENTED; }

void SsSetMarkCallback(
    short access_num, short seq_num, SsMarkCallbackProc proc) {
    NOT_IMPLEMENTED;
}

short SsIsEos(short access_num, short seq_num) {
    NOT_IMPLEMENTED;
    return 0;
}

short SsUtKeyOn(short vabid, short prog, short tone, short note, short fine,
                short voll, short volr) {
    NOT_IMPLEMENTED;
    return 0;
}

short SsUtKeyOff(short voice, short vabid, short prog, short tone, short note) {
    NOT_IMPLEMENTED;
    return 0;
}

void SsUtAllKeyOff(short mode) { NOT_IMPLEMENTED; }

void SsSepStop(short sep_access_num, short seq_num) { NOT_IMPLEMENTED; }

short SsSepOpen(u_long* addr, short vab_id, short seq_num) {
    NOT_IMPLEMENTED;
    return 0;
}

void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr) {
    NOT_IMPLEMENTED;
}

void SsSeqCalledTbyT(void) { NOT_IMPLEMENTED; }

void SsSeqSetCrescendo(short sep_access_num, short vol, long v_time) {
    NOT_IMPLEMENTED;
}

s32 _SsVmSetSeqVol(s16 seq_sep_no, u16 voll, u16 volr, s16 arg3) {
    NOT_IMPLEMENTED;
    return 0;
}
void _SsVmSeqKeyOff(s16 seq_sep_num) { NOT_IMPLEMENTED; }
