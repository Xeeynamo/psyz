#include "libsnd_private.h"

void _SsVmDoAllocate(void) {
    int i;

    _svm_cur.voiceOffset = _svm_cur.voice * sizeof(SPU_VOICE_REG) / 2;
    _svm_cur.field_0x1e = _svm_cur.fake_program * 16 + _svm_cur.tone;
    _svm_voice[_svm_cur.voice].key_stat = 0x7FFF;
    for (i = 0; i < NUM_VAB; i++) {
        _svm_envx_hist[i] &= ~(1 << _svm_cur.voice);
    }
    if ((_svm_cur.tone_vag_idx & 1) > 0) {
        ((short*)_svm_sreg_buf)[_svm_cur.voiceOffset + 3] =
            ((u16*)&_svm_pg[(_svm_cur.tone_vag_idx - 1) / 2].reserved2)[0];
        _svm_sreg_dirty[_svm_cur.voice] |= 8;
    } else {
        ((short*)_svm_sreg_buf)[_svm_cur.voiceOffset + 3] =
            ((u16*)&_svm_pg[(_svm_cur.tone_vag_idx - 1) / 2].reserved2)[1];
        _svm_sreg_dirty[_svm_cur.voice] |= 8;
    }
    ((short*)_svm_sreg_buf)[_svm_cur.voiceOffset + 4] =
        _svm_tn[_svm_cur.fake_program * 16 + _svm_cur.tone].adsr1;
    ((short*)_svm_sreg_buf)[_svm_cur.voiceOffset + 5] =
        _svm_tn[_svm_cur.fake_program * 16 + _svm_cur.tone].adsr2 + _svm_damper;
    _svm_sreg_dirty[_svm_cur.voice] |= 0x30;
}
