#include "libsnd_private.h"

void _SsClose(s16 seq_sep_num) {
    s32 seq_num;
    _SsVmSetSeqVol(seq_sep_num, 0, 0, 1);
    _SsVmSeqKeyOff(seq_sep_num);
    _snd_openflag &= ~(1 << seq_sep_num);
    for (seq_num = 0; seq_num < _snd_seq_t_max; seq_num++) {
        _ss_score[seq_sep_num][seq_num].unk98 = 0;
        _ss_score[seq_sep_num][seq_num].unk22 = 0xFF;
        _ss_score[seq_sep_num][seq_num].unk23 = 0;
        _ss_score[seq_sep_num][seq_num].unk48 = 0;
        _ss_score[seq_sep_num][seq_num].unk4A = 0;
        _ss_score[seq_sep_num][seq_num].unk9C = 0;
        _ss_score[seq_sep_num][seq_num].unkA0 = 0;
        _ss_score[seq_sep_num][seq_num].unk4C = 0;
        _ss_score[seq_sep_num][seq_num].unkAC = 0;
        _ss_score[seq_sep_num][seq_num].unkA8 = 0;
        _ss_score[seq_sep_num][seq_num].unkA4 = 0;
        _ss_score[seq_sep_num][seq_num].unk4E = 0;
        _ss_score[seq_sep_num][seq_num].unk58 = 0x7F;
        _ss_score[seq_sep_num][seq_num].unk5A = 0x7F;
    }
}

void SsSeqClose(short seq_access_num) { _SsClose(seq_access_num); }

void SsSepClose(short sep_access_num) { _SsClose(sep_access_num); }
