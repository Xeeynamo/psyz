#include "libsnd_private.h"

void _SsVmKeyOnNow(unsigned short vagCount, unsigned short pitch) {
    volatile int pad;
    unsigned chL;
    unsigned chR;
    unsigned vol;
    short keyon1, keyon2;
    struct SeqStruct* ss;
    unsigned short voice;
    int volMul;

    volMul = _svm_vh->mvol * 16383;
    vol = (_svm_cur.volume * volMul) / 16129;
    chR = ((vol * _svm_cur.mvol) * _svm_cur.tone_vol) / 16129;
    voice = _svm_cur.voice * 8;
    vol = chR;
#ifndef __psyz
    ss = &_ss_score[_svm_cur.seq_sep_no & 0xFF]
                   [(_svm_cur.seq_sep_no >> 8) & 0xFF];
#endif
    if (_svm_cur.seq_sep_no != 0x21) {
#ifdef __psyz
        // move code to avoid ASAN to trigger for OOB on seq_sep_no==0x21
        ss = &_ss_score[_svm_cur.seq_sep_no & 0xFF]
                       [(_svm_cur.seq_sep_no >> 8) & 0xFF];
#endif
        vol = (chR * ss->voll) / 127;
        chR = (chR * ss->volr) / 127;
    }
    if (_svm_cur.tone_pan < 64) {
        chR = (chR * _svm_cur.tone_pan) / 63;
        chL = vol;
    } else {
        chL = (vol * (127 - _svm_cur.tone_pan)) / 63;
    }
    if (_svm_cur.mpan < 64) {
        chR = (chR * _svm_cur.mpan) / 63;
    } else {
        chL = (chL * (127 - _svm_cur.mpan)) / 63;
    }
    if (_svm_cur.pan < 64) {
        chR = (chR * _svm_cur.pan) / 63;
    } else {
        chL = (chL * (127 - _svm_cur.pan)) / 63;
    }
    if (_svm_stereo_mono == 1) {
        if (chL < chR) {
            chL = chR;
        } else {
            chR = chL;
        }
    }
    chL = (chL * chL) / 16383;
    chR = (chR * chR) / 16383;
    ((short*)_svm_sreg_buf)[voice + 2] = pitch;
    ((short*)_svm_sreg_buf)[voice + 0] = chL;
    ((short*)_svm_sreg_buf)[voice + 1] = chR;
    _svm_sreg_dirty[_svm_cur.voice] |= 7;
    _svm_voice[_svm_cur.voice].unk04 = pitch;
    if (_svm_cur.voice < 16) {
        keyon1 = 1 << _svm_cur.voice;
        keyon2 = 0;
    } else {
        keyon1 = 0;
        keyon2 = 1 << (_svm_cur.voice - 16);
    }
    if (_svm_cur.tone_mode & 4) {
        _svm_orev1 |= keyon1;
        _svm_orev2 |= keyon2;
    } else {
        _svm_orev1 &= ~keyon1;
        _svm_orev2 &= ~keyon2;
    }
    _svm_okon1 |= keyon1;
    _svm_okon2 |= keyon2;
    _svm_okof1 &= ~_svm_okon1;
    _svm_okof2 &= ~_svm_okon2;
}
