#include "libsnd_private.h"

void SsUtAllKeyOff(short mode) {
    short i;

    for (i = 0; i < _SsVmMaxVoice; i++) {
        _svm_voice[i].unk2 = 0x18;
        _svm_voice[i].unk6 = 0;
        _svm_voice[i].unke = 0xFF;
        _svm_voice[i].unk10 = 0;
        _svm_voice[i].prog = 0;
        _svm_voice[i].tone = 0xFF;
        ((SPU_VOICE_REG*)_svm_sreg)[i << 19 >> 19].addr = 0x200;
        ((SPU_VOICE_REG*)_svm_sreg)[i << 19 >> 19].pitch = 0x1000;
        ((SPU_VOICE_REG*)_svm_sreg)[i << 19 >> 19].adsr[0] = 0x80FF;
        ((SPU_VOICE_REG*)_svm_sreg)[i << 19 >> 19].volume.left = 0;
        ((SPU_VOICE_REG*)_svm_sreg)[i << 19 >> 19].volume.right = 0;
        ((SPU_VOICE_REG*)_svm_sreg)[i << 19 >> 19].adsr[1] = 0x4000;
        _svm_cur.field_0x1a = i;
        _SsVmKeyOffNow(1);
    }
}
