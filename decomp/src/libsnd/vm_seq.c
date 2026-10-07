#include "libsnd_private.h"

short _SsVmSetSeqVol(short seq_sep_no, u16 voll, u16 volr, short arg3) {
    struct SeqStruct* score;
    int vol;
    int vol_scaled;
    int tone;
    unsigned voll_t, volr_t;
    u8 pan;
    short i;

    score = &_ss_score[seq_sep_no & 0xFF][(seq_sep_no & 0xFF00) >> 8];
    _svm_cur.seq_sep_no = seq_sep_no;
    if (voll == 0) {
        voll = 1;
    }
    if (volr == 0) {
        volr = 1;
    }
    score->voll = voll;
    score->volr = volr;
    if (score->voll > 127) {
        score->voll = 127;
    }
    if (score->volr > 127) {
        score->volr = 127;
    }
    if (arg3 == 1) {
        for (i = 0; i < _SsVmMaxVoice; i++) {
            if ((u16)_svm_voice[i].seq_sep_no == (u16)seq_sep_no) {
                vol =
                    _svm_voice[i].voll1 * score->vol[score->channel_idx] / 127;
                vol_scaled = vol * 0x3FFF;
                voll_t = _svm_vh->mvol * vol_scaled / 0x3F01;
                tone = _svm_voice[i].fake_program * 16 + _svm_voice[i].tone;
                volr_t = voll_t * _svm_pg[_svm_voice[i].prog].mvol *
                         _svm_tn[tone].vol / 0x3F01;
                voll_t = volr_t * score->voll / 127;
                volr_t = volr_t * score->volr / 127;
                pan = _svm_tn[tone].pan;
                if (pan < 64) {
                    voll = voll_t;
                    volr = (volr_t * pan) / 63;
                } else {
                    volr = volr_t;
                    voll = (voll_t * (127 - pan)) / 63;
                }
                pan = _svm_pg[_svm_voice[i].prog].mpan;
                if (pan < 64) {
                    volr = (volr * pan) / 63;
                } else {
                    voll = (voll * (127 - pan)) / 63;
                }
                pan = _svm_voice[i].pan;
                if (pan < 64) {
                    volr = (volr * pan) / 63;
                } else {
                    voll = (voll * (127 - pan)) / 63;
                }
                if (_svm_stereo_mono == 1) {
                    if (voll < volr) {
                        voll = volr;
                    } else {
                        volr = voll;
                    }
                }
                voll = (voll * voll) / 0x3FFF;
                volr = (volr * volr) / 0x3FFF;
                ((short*)_svm_sreg_buf)[i * 8 + 0] = voll;
                ((short*)_svm_sreg_buf)[i * 8 + 1] = volr;
                _svm_sreg_dirty[i] |= 3;
            }
        }
    }
    return _svm_cur.seq_sep_no;
}

INCLUDE_ASM("asm/nonmatchings/libsnd/vm_seq", _SsVmGetSeqVol);

INCLUDE_ASM("asm/nonmatchings/libsnd/vm_seq", _SsVmGetSeqLVol);

INCLUDE_ASM("asm/nonmatchings/libsnd/vm_seq", _SsVmGetSeqRVol);

void _SsVmSeqKeyOff(s16 seq_sep_num) {
    u8 i;

    for (i = 0; i < _SsVmMaxVoice; i++) {
        if (_svm_voice[i].seq_sep_no == seq_sep_num) {
            _svm_cur.voice = i;
            _SsVmKeyOffNow(0);
        }
    }
}
