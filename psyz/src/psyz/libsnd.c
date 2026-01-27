#include <psyz.h>
#include <libspu.h>
#include <libsnd.h>
#include <psyz/log.h>

short _svm_stereo_mono;
short _svm_damper;
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

short SsSeqOpen(u_long* addr, short vab_id) {
    NOT_IMPLEMENTED;
    return 0;
}

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

int _SsInitSoundSep(short flag, short i, short vab_id, unsigned int* addr) {
    NOT_IMPLEMENTED;
    return -1;
}

void SsSeqCalledTbyT(void) { NOT_IMPLEMENTED; }

s32 _SsVmSetSeqVol(s16 seq_sep_no, u16 voll, u16 volr, s16 arg3) {
    NOT_IMPLEMENTED;
    return 0;
}

void _SsVmSeqKeyOff(s16 seq_sep_num) { NOT_IMPLEMENTED; }

void _SsNoteOn(short a0, short a1, unsigned char a2, unsigned char a3) {
    NOT_IMPLEMENTED;
}
void _SsSetProgramChange(short a0, short a1, unsigned char a2) {
    NOT_IMPLEMENTED;
}
void _SsGetMetaEvent(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsSetPitchBend(short a0, short a1) { NOT_IMPLEMENTED; }
void _SsSetControlChange(short a0, short a1, unsigned char a2) {
    NOT_IMPLEMENTED;
}
void _SsContBankChange(short a0, short a1) { NOT_IMPLEMENTED; }
void _SsContDataEntry(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContMainVol(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContPanpot(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContExpression(short a0, short a1, unsigned char a2) {
    NOT_IMPLEMENTED;
}
void _SsContDamper(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContExternal(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContNrpn1(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContNrpn2(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContRpn1(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContRpn2(short a0, short a1, unsigned char a2) { NOT_IMPLEMENTED; }
void _SsContResetAll(short a0, short a1) { NOT_IMPLEMENTED; }

void _SsSetNrpnVabAttr0(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr1(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr2(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr3(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr4(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr5(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr6(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr7(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr8(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr9(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr10(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr11(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr12(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr13(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr14(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr15(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr16(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr17(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr18(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}
void _SsSetNrpnVabAttr19(
    short a0, short a1, short a2, VagAtr a3, short a4, unsigned char a5) {
    NOT_IMPLEMENTED;
}