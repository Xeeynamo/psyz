#include "libsnd_private.h"

void SsSeqCalledTbyT(void) {
    int sIndex;
    int tIndex;
    int bit;

    if (_snd_ev_flag == 1) {
        return;
    }
    _snd_ev_flag = 1;
    _SsVmFlush();

    for (sIndex = 0; sIndex < _snd_seq_s_max; sIndex++) {
        bit = 1 << sIndex;
        if (!(_snd_openflag & bit)) {
            continue;
        }
        for (tIndex = 0; tIndex < _snd_seq_t_max; tIndex++) {
            if (_ss_score[sIndex][tIndex].flags & 1) {
                _SsSndPlay(sIndex, tIndex);
                if (_ss_score[sIndex][tIndex].flags & 0x10) {
                    _SsSndCrescendo(sIndex, tIndex);
                }
                if (_ss_score[sIndex][tIndex].flags & 0x20) {
                    _SsSndDecrescendo(sIndex, tIndex);
                }
                if (_ss_score[sIndex][tIndex].flags & 0x40) {
                    _SsSndTempo(sIndex, tIndex);
                }
                if (_ss_score[sIndex][tIndex].flags & 0x80) {
                    _SsSndTempo(sIndex, tIndex);
                }
            }
            if (_ss_score[sIndex][tIndex].flags & 2) {
                _SsSndPause(sIndex, tIndex);
            }
            if (_ss_score[sIndex][tIndex].flags & 8) {
                _SsSndReplay(sIndex, tIndex);
            }
            if (_ss_score[sIndex][tIndex].flags & 4) {
                _SsSndStop(sIndex, tIndex);
                _ss_score[sIndex][tIndex].flags = 0;
            }
        }
    }
    _snd_ev_flag = 0;
}