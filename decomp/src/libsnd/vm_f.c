#include "libsnd_private.h"

SPU_RXX* _svm_sreg = (SPU_RXX*)0x1F801C00;
SPU_VOICE_REG _svm_sreg_buf[NUM_VOICES];
char _svm_sreg_dirty[NUM_VOICES];
unsigned short _svm_okon1;
unsigned short _svm_okon2;
unsigned short _svm_okof1;
unsigned short _svm_okof2;
unsigned short _svm_orev1;
unsigned short _svm_orev2;
struct SpuVoice _svm_voice[NUM_VOICES];
unsigned int _svm_envx_ptr;
static int _dummy;
int _svm_envx_hist[16];
#ifdef __psyz
void (*_autovol)(short voice);
void (*_autopan)(short voice);
#else
void (*_autovol)();
void (*_autopan)();
#endif

extern unsigned char _svm_auto_kof_mode;

void _SsVmFlush(void) {
    s32 i;
    unsigned int var_s2;
    u16 temp_a0;
    u16 temp_a1;
    u16 temp_a2;
    u16 temp_v1;
    u16 rev1;
    u16 rev2;
    SPU_RXX* sreg;

    _svm_envx_ptr = (_svm_envx_ptr + 1) & 0xF;
    _svm_envx_hist[_svm_envx_ptr] = 0;
    for (i = 0; i < _SsVmMaxVoice; i++) {
        _svm_voice[i].unk6 = SPUR(voice[i].volumex);
        if (!_svm_voice[i].unk6) {
            _svm_envx_hist[_svm_envx_ptr] |= 1 << i;
        }
    }
    if (_svm_auto_kof_mode == 0) {
        var_s2 = -1;
        for (i = 0; i < LEN(_svm_envx_hist) - 1; i++) {
            var_s2 &= _svm_envx_hist[i];
        }
        for (i = 0; i < _SsVmMaxVoice; i++) {
            if (var_s2 & (1 << i)) {
                if (_svm_voice[i].unk1b == 2) {
                    SpuSetNoiseVoice(0, 0xFFFFFF);
                }
                _svm_voice[i].unk1b = 0;
            }
        }
    }
    _svm_okon1 &= ~_svm_okof1;
    _svm_okon2 &= ~_svm_okof2;
    for (i = 0; i < NUM_VOICES; i++) {
        if (_svm_voice[i].auto_vol) {
            _autovol(i);
        }
        if (_svm_voice[i].auto_pan) {
            _autopan(i);
        }
    }
    {
        char* dirty = _svm_sreg_dirty;
        u16* base = (u16*)_svm_sreg_buf;
        u16* adsr2 = base + 5;
        u16* adsr1 = base + 4;
        int offset = 0;
        u16* right = base + 1;
        u16* left = base;
        char* dirtyEnd = _svm_sreg_dirty + NUM_VOICES;

        do {
            if (*dirty & 1) {
#ifdef __psyz
                SPUWV(offset / sizeof(SPU_VOICE_REG), volume.left, *left);
                SPUWV(offset / sizeof(SPU_VOICE_REG), volume.right, *right);
#else
                u16* spuVoice = (u16*)(offset + (u32)_svm_sreg);
                spuVoice[0] = *left;
                spuVoice[1] = *right;
#endif
            }
            if (*dirty & 4) {
#ifdef __psyz
                SPUWV(offset / sizeof(SPU_VOICE_REG), pitch,
                      *(u16*)((u8*)_svm_sreg_buf + offset + 4));
#else
                u16* spuVoice = (u16*)(offset + (u32)_svm_sreg);
                spuVoice[2] = *(u16*)((u8*)_svm_sreg_buf + offset + 4);
#endif
            }
            if (*dirty & 8) {
#ifdef __psyz
                SPUWV(offset / sizeof(SPU_VOICE_REG), addr,
                      *(u16*)((u8*)_svm_sreg_buf + offset + 6));
#else
                u16* spuVoice = (u16*)(offset + (u32)_svm_sreg);
                spuVoice[3] = *(u16*)((u8*)_svm_sreg_buf + offset + 6);
#endif
            }
            if (*dirty & 0x10) {
#ifdef __psyz
                SPUWV(offset / sizeof(SPU_VOICE_REG), adsr[0], *adsr1);
                SPUWV(offset / sizeof(SPU_VOICE_REG), adsr[1], *adsr2);
#else
                u16* spuVoice = (u16*)(offset + (u32)_svm_sreg);
                spuVoice[4] = *adsr1;
                spuVoice[5] = *adsr2;
#endif
            }
            *dirty++ = 0;
            adsr2 += 8;
            adsr1 += 8;
            offset += sizeof(SPU_VOICE_REG);
            right += 8;
            left += 8;
#ifdef __psyz
        } while (dirty < dirtyEnd);
#else
        } while ((s32)dirty < (s32)dirtyEnd);
#endif
    }
    sreg = _svm_sreg;
    temp_v1 = _svm_okof1;
    temp_a0 = _svm_okof2;
    temp_a1 = _svm_okon1;
    temp_a2 = _svm_okon2;
    rev1 = _svm_orev1;
    rev2 = _svm_orev2;
    _svm_okof1 = 0;
    _svm_okof2 = 0;
    _svm_okon1 = 0;
    _svm_okon2 = 0;
#ifdef __psyz
    SPUW(key_off[0], temp_v1);
    SPUW(key_off[1], temp_a0);
    SPUW(key_on[0], temp_a1);
    SPUW(key_on[1], temp_a2);
    SPUW(rev_mode[0], rev1);
    SPUW(rev_mode[1], rev2);
#else
    sreg->key_off[0] = temp_v1;
    sreg->key_off[1] = temp_a0;
    sreg->key_on[0] = temp_a1;
    sreg->key_on[1] = temp_a2;
    sreg->rev_mode[0] = rev1;
    sreg->rev_mode[1] = rev2;
#endif
}
