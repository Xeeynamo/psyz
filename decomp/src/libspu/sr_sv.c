#include "libspu_private.h"

u16 _spu_note2pitch(u16, u16, u16, u16);

static inline u16 inline_fn(u16 arg0, u16 arg1) { return arg0 & arg1; }

s32 _SpuRSetVoiceAttr(SpuVoiceAttr* attr, s32 min, s32 max, s32 nowait) {
    s32 bSetAll;
    s32 is_single;
    s32 var_a1_2;
    s32 var_a1_3;
    s32 var_a1_4;
    u16 volmode_left;
    u16 volmode_right;
    s32 voice;
    u16 temp_a1;
    u16 temp_a3;
    u16 var_a2;
    u16 vol_right;
    u16 vol_left;
    u32 mask;
    s32 pos;
    s32 adsr;

    vol_left = 0;
    vol_right = 0;
    is_single = min == max;
    mask = attr->mask;
    bSetAll = attr->mask == 0;

    if (min < 0) {
        min = 0;
    }
    if (min >= NUM_VOICES) {
        return -3;
    }
    if (max >= NUM_VOICES) {
        max = NUM_VOICES - 1;
    }
    if (max < 0 || max < min) {
        return -3;
    }
    max++;
    for (voice = min; voice < max; voice++) {
        if (!is_single && !(attr->voice & (1 << voice))) {
            continue;
        }
        pos = voice * 8;

        if (bSetAll || mask & SPU_VOICE_PITCH) {
            SPUW_RAW(pos + 2, attr->pitch);
        }
        if (bSetAll || mask & SPU_VOICE_SAMPLE_NOTE) {
            _spu_voice_centerNote[voice] = attr->sample_note;
        }
        if (bSetAll || mask & SPU_VOICE_NOTE) {
            temp_a1 = _spu_voice_centerNote[voice];
            temp_a3 = attr->note;
            SPUW_RAW(pos + 2, _spu_note2pitch(temp_a1 >> 8, temp_a1 & 0xFF,
                                              temp_a3 >> 8, temp_a3 & 0xFF));
        }
        if (bSetAll || mask & SPU_VOICE_ADSR_ADSR1) {
            SPUW_RAW(pos + 4, attr->adsr1);
        }
        if (bSetAll || mask & SPU_VOICE_ADSR_ADSR2) {
            SPUW_RAW(pos + 5, attr->adsr2);
        }
        if (bSetAll || mask & SPU_VOICE_VOLL) {
            if (bSetAll || mask & SPU_VOICE_VOLMODEL) {
                switch ((s16)attr->volmode.left) {
                case 1:
                    volmode_left = 0x8000;
                    break;
                case 2:
                    volmode_left = 0x9000;
                    break;
                case 3:
                    volmode_left = 0xA000;
                    break;
                case 4:
                    volmode_left = 0xB000;
                    break;
                case 5:
                    volmode_left = 0xC000;
                    break;
                case 6:
                    volmode_left = 0xD000;
                    break;
                case 7:
                    volmode_left = 0xE000;
                    break;
                case 0:
                    vol_left = attr->volume.left;
                    volmode_left = 0;
                    break;
                default:
                    vol_left = attr->volume.left;
                    volmode_left = 0;
                    break;
                }
            } else {
                vol_left = attr->volume.left;
                volmode_left = 0;
            }
            if (volmode_left != 0) {
                if ((s16)attr->volume.left >= 0x80) {
                    vol_left = 0x7F;
                } else if ((s16)attr->volume.left < 0) {
                    vol_left = 0;
                } else {
                    vol_left = attr->volume.left;
                }
            }
            SPUW_RAW(pos + 0, inline_fn(vol_left, 0x7FFF) | volmode_left);
        }
        if (bSetAll || mask & SPU_VOICE_VOLR) {
            if (bSetAll || mask & SPU_VOICE_VOLMODER) {
                switch ((s16)attr->volmode.right) {
                case 1:
                    volmode_right = 0x8000;
                    break;
                case 2:
                    volmode_right = 0x9000;
                    break;
                case 3:
                    volmode_right = 0xA000;
                    break;
                case 4:
                    volmode_right = 0xB000;
                    break;
                case 5:
                    volmode_right = 0xC000;
                    break;
                case 6:
                    volmode_right = 0xD000;
                    break;
                case 7:
                    volmode_right = 0xE000;
                    break;
                case 0:
                    vol_right = attr->volume.right;
                    volmode_right = 0;
                    break;
                default:
                    vol_right = attr->volume.right;
                    volmode_right = 0;
                    break;
                }
            } else {
                vol_right = attr->volume.right;
                volmode_right = 0;
            }
            if (volmode_right != 0) {
                if ((s16)attr->volume.right >= 0x80) {
                    vol_right = 0x7F;
                } else if ((s16)attr->volume.right < 0) {
                    vol_right = 0;
                } else {
                    vol_right = attr->volume.right;
                }
            }
            SPUW_RAW(pos + 1, inline_fn(vol_right, 0x7FFF) | volmode_right);
        }
        if (bSetAll || mask & SPU_VOICE_LSAX) {
            _spu_FsetRXXa(pos | 7, attr->loop_addr);
        }
        if (bSetAll || mask & SPU_VOICE_WDSA) {
            _spu_FsetRXXa(pos | 3, attr->addr);
        }
        if (bSetAll || mask & SPU_VOICE_ADSR_AR) {
            if (bSetAll || mask & SPU_VOICE_ADSR_AMODE) {
                switch (attr->a_mode) {
                case 1:
                    var_a1_2 = 0;
                    break;
                case 5:
                    var_a1_2 = 0x80;
                    break;
                default:
                    var_a1_2 = 0;
                    break;
                }
            } else {
                var_a1_2 = 0;
            }
            if (attr->ar > 0x7F) {
                var_a2 = 0x7F;
            } else {
                var_a2 = attr->ar;
            }
            adsr = SPUR_RAW(pos + 4);
            adsr &= 0x00FF;
            SPUW_RAW(pos + 4, adsr | ((var_a2 | var_a1_2) << 8));
        }
        if (bSetAll || mask & SPU_VOICE_ADSR_DR) {
            if (attr->dr > 0xF) {
                var_a2 = 0xF;
            } else {
                var_a2 = attr->dr;
            }
            adsr = SPUR_RAW(pos + 4);
            adsr &= 0xFF0F;
            SPUW_RAW(pos + 4, adsr | (var_a2 << 4));
        }
        if (bSetAll || mask & SPU_VOICE_ADSR_SL) {
            if (attr->sl > 0xF) {
                var_a2 = 0xF;
            } else {
                var_a2 = attr->sl;
            }
            adsr = SPUR_RAW(pos + 4);
            SPUW_RAW(pos + 4, (adsr & 0xFFF0) | var_a2);
        }
        if (bSetAll || mask & SPU_VOICE_ADSR_SR) {
            if (bSetAll || mask & SPU_VOICE_ADSR_SMODE) {
                switch (attr->s_mode) {
                case 1:
                    var_a1_3 = 0;
                    break;
                case 3:
                    var_a1_3 = 0x100;
                    break;
                case 5:
                    var_a1_3 = 0x200;
                    break;
                case 7:
                    var_a1_3 = 0x300;
                    break;
                default:
                    var_a1_3 = 0x100;
                    break;
                }
            } else {
                var_a1_3 = 0;
            }
            if (attr->sr > 0x7F) {
                var_a2 = 0x7F;
            } else {
                var_a2 = attr->sr;
            }
            adsr = SPUR_RAW(pos + 5);
            adsr &= 0x003F;
            SPUW_RAW(pos + 5, adsr | ((var_a2 | var_a1_3) << 6));
        }
        if (bSetAll || mask & SPU_VOICE_ADSR_RR) {
            if (bSetAll || mask & SPU_VOICE_ADSR_RMODE) {
                switch (attr->r_mode) {
                case 3:
                    var_a1_4 = 0;
                    break;
                case 7:
                    var_a1_4 = 0x20;
                    break;
                default:
                    var_a1_4 = 0;
                    break;
                }
            } else {
                var_a1_4 = 0;
            }
            if (attr->rr > 0x1F) {
                var_a2 = 0x1F;
            } else {
                var_a2 = attr->rr;
            }
            adsr = SPUR_RAW(pos + 5);
            adsr &= 0xFFC0;
            SPUW_RAW(pos + 5, adsr | (var_a2 | var_a1_4));
        }
    }
    if (nowait == 0) {
        _spu_Fw1ts();
    }
    return 0;
}
