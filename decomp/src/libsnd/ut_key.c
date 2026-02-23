#include "libsnd_private.h"

s16 SsUtKeyOn(
    s16 vabId, s16 prog, s16 tone, s16 note, s16 fine, s16 voll, s16 volr) {
    s16 voice;
    s32 tone2;

    if (_snd_ev_flag == 1) {
        return -1;
    }
    _snd_ev_flag = 1;

    if (_SsVmVSetUp(vabId, prog)) {
        _snd_ev_flag = 0;
        return -1;
    }
    _svm_cur.seq_sep_no = 0x21;
    _svm_cur.field_2_note = note;
    _svm_cur.field_0x3 = fine;
    _svm_cur.field_C_vag_idx = tone;
    if (voll == volr) {
        _svm_cur.field_0x5 = 0x40;
        _svm_cur.field_4_voll = voll;
    } else if (volr < voll) {
        _svm_cur.field_4_voll = voll;
        _svm_cur.field_0x5 = (volr * 0x40) / voll;
    } else {
        _svm_cur.field_4_voll = volr;
        _svm_cur.field_0x5 = 0x7F - ((voll * 0x40) / volr);
    }
    _svm_cur.field_A_mvol = _svm_pg[prog].mvol;
    _svm_cur.field_B_mpan = _svm_pg[prog].mpan;
    _svm_cur.field_0_sep_sep_no_tonecount = _svm_pg[prog].tones;

    tone2 = _svm_cur.field_C_vag_idx + (_svm_cur.field_7_fake_program * 0x10);
    _svm_cur.field_F_prior = _svm_tn[tone2].prior;
    _svm_cur.field_18_voice_idx = _svm_tn[tone2].vag;
    _svm_cur.field_D_vol = _svm_tn[tone2].vol;
    _svm_cur.field_E_pan = _svm_tn[tone2].pan;
    _svm_cur.field_10_centre = _svm_tn[tone2].center;
    _svm_cur.field_11_shift = _svm_tn[tone2].shift;
    _svm_cur.field_14_seq_sep_no = _svm_tn[tone2].mode;
    _svm_cur.field_12_mode = _svm_tn[tone2].min;
    _svm_cur.field_0x13 = _svm_tn[tone2].max;
    if (_svm_cur.field_18_voice_idx == 0) {
        _snd_ev_flag = 0;
        return -1;
    }

    voice = _SsVmAlloc(_svm_cur.field_18_voice_idx);
    if (voice == _SsVmMaxVoice) {
        _snd_ev_flag = 0;
        return -1;
    }

    _svm_cur.field_0x1a = voice;
    _svm_voice[voice].unke = 0x21;
    _svm_voice[voice].vabId = vabId;
    _svm_voice[voice].unk10 = _svm_cur.field_7_fake_program;
    _svm_voice[voice].prog = prog;
    _svm_voice[voice].unk0 = _svm_cur.field_18_voice_idx;
    _svm_voice[voice].tone = _svm_cur.field_C_vag_idx;
    _svm_voice[voice].note = note;
    _svm_voice[voice].unk1b = 1;
    _svm_voice[voice].unk2 = 0;

    _SsVmDoAllocate();
    if (_svm_cur.field_18_voice_idx == 0xFF) {
        vmNoiseOn(voice);
    } else {
        _SsVmKeyOnNow(1, note2pitch2(note, fine));
    }
    _snd_ev_flag = 0;
    return voice;
}

short SsUtKeyOff(short voice, short vabId, short prog, short tone, short note) {
    if (_snd_ev_flag != 1) {
        _snd_ev_flag = 1;
        if (voice >= 0 && voice < NUM_VOICES) {
            if (_svm_voice[voice].vabId == vabId &&
                _svm_voice[voice].prog == prog &&
                _svm_voice[voice].tone == tone &&
                _svm_voice[voice].note == note &&
                _svm_voice[voice].unk0 == 0xFF) {
                vmNoiseOff(voice);
            } else {
                _svm_cur.field_0x1a = voice;
                _SsVmKeyOffNow(0);
            }
            _svm_voice[voice].auto_pan = 0;
            _svm_voice[voice].auto_vol = 0;
            _snd_ev_flag = 0;
            return 0;
        }
        _snd_ev_flag = 0;
    }
    return -1;
}
