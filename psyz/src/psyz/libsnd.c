#include <psyz.h>
#include <libspu.h>
#include <libsnd.h>
#include <log.h>

short SsUtSetVVol(short vc, short voll, short volr) {
    DEBUGF("not implemented\n");
    return 0;
}

void SsSetSerialAttr(char s_num, char attr, char mode) { NOT_IMPLEMENTED; }

void SsSeqPlay(short seq_access_num, char play_mode, short l_count) {
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

void SsSeqClose(short seq_access_num) { NOT_IMPLEMENTED; }

void SsSeqStop(short seq_access_num) { NOT_IMPLEMENTED; }

short SsUtKeyOnV(short voice, short vabId, short prog, short tone, short note,
                 short fine, short voll, short volr) {
    NOT_IMPLEMENTED;
    return 0;
}

void SsUtReverbOn(void) { NOT_IMPLEMENTED; }

short SsUtSetReverbType(short type) {
    NOT_IMPLEMENTED;
    return -1;
}

void SsSetTableSize(char* table, short s_max, short t_max) { NOT_IMPLEMENTED; }

void SsStart(void) { NOT_IMPLEMENTED; }

char SsSetReservedVoice(char voices) {
    DEBUGF("%d", voices);
    return 0;
}

void SsSetTickMode(long tick_mode) { DEBUGF("%d", tick_mode); }

void MySsInitHot();
void SsInitHot(void) { MySsInitHot(); }

void SsSetStereo(void) { NOT_IMPLEMENTED; }

void SsSetMono(void) { NOT_IMPLEMENTED; }

void SsSetSerialVol(char s_num, short voll, short volr) {
    DEBUGF("s_num %d voll %d volr %d", s_num, voll, volr);
}

void SsUtSetReverbDepth(short ldepth, short rdepth) {
    DEBUGF("ldepth %d rdepth %d", ldepth, rdepth);
}

short SsVabTransCompleted(short immediateFlag) {
    NOT_IMPLEMENTED;
    return 1;
}

short SsVabOpenHeadSticky(u_char* addr, u_long vabid, u_long sbaddr) {
    NOT_IMPLEMENTED;
    return 0;
}

short SsVabTransBodyPartly(u_char* addr, u_long bufsize, short vabid) {
    NOT_IMPLEMENTED;
    return 0;
}

short SsVabOpenHead(unsigned char* addr, short vabid) {
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

void SsUtSetReverbDelay(short delay) { NOT_IMPLEMENTED; }

void SsUtReverbOff(void) { NOT_IMPLEMENTED; }

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

void SsSepClose(short sep_access_num) { NOT_IMPLEMENTED; }

void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr) {
    NOT_IMPLEMENTED;
}

void SsSeqCalledTbyT(void) { NOT_IMPLEMENTED; }

void SsSepPlay(
    short sep_access_num, short seq_num, char play_modoe, short l_count) {
    NOT_IMPLEMENTED;
}

void SsSeqSetCrescendo(short sep_access_num, short vol, long v_time) {
    NOT_IMPLEMENTED;
}