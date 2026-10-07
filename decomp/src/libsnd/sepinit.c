#include "libsnd_private.h"

#ifndef __psyz
INCLUDE_ASM("asm/nonmatchings/libsnd/sepinit", _SsInitSoundSep);
#else
int _SsInitSoundSep(short flag, short i, short vab_id, unsigned long* addr) {
    struct SeqStruct* score;
    int channel;
    int len;
    int data_len;
    int delta_value;
    unsigned int ticks;
    u8* cursor;
    u8 res_hi;
    u8 tempo_hi;
    u8 tempo_mid;
    u8 size_hi;
    u8 size_mid_hi;
    u8 size_mid_lo;

    len = 0;
    score = &_ss_score[flag][i];
    score->l_count = 1;
    score->unk15 = 0;
    score->running_status = 0;
    score->channel_idx = 0;
    score->unk18 = 0;
    score->unk19 = 0;
    score->fn_idx = 0;
    score->unk1B = 0;
    score->unk1C = 0;
    score->unk1D = 0;
    score->unk1E = 0;
    score->unk1F = 0;
    score->play_mode = 0;
    score->unk21 = 0;
    score->unk52 = 1;
    score->resolution = 0;
    score->vab_id = vab_id;
    score->unk56 = 0;
    score->unk84 = 0;
    score->unk88 = 0;
    score->tempo = 0;
    score->delta_value = 0;
    score->channel_mute = 0;
    score->rhythm_n = 0;
    score->rhythm_d = 0;
    for (channel = 0; channel < 16; channel++) {
        score->panpot[channel] = 0x40;
        score->programs[channel] = channel;
        score->vol[channel] = 0x7F;
    }

    score->seq_ptr = (u8*)addr;
    if (i == 0) {
        if (*score->seq_ptr == 'S' || *score->seq_ptr == 'p') {
            score->seq_ptr += 5;
            if (*score->seq_ptr++ != 0) {
                printf("This is not SEP Data.\n");
                return -1;
            }
            score->seq_ptr += 2;
            len += 8;
        }
    } else {
        score->seq_ptr += 2;
        len += 2;
    }

    cursor = score->seq_ptr;
    score->seq_ptr = cursor + 1;
    res_hi = cursor[0];
    score->seq_ptr = cursor + 2;
    score->resolution = cursor[1] | (res_hi << 8);
    cursor = score->seq_ptr;
    score->seq_ptr = cursor + 1;
    tempo_hi = cursor[0];
    score->seq_ptr = cursor + 2;
    tempo_mid = cursor[1];
    score->seq_ptr = cursor + 3;
    score->tempo = (tempo_hi << 16) | (tempo_mid << 8) | cursor[2];
    len += 5;
    if ((score->tempo / 2) < (60000000 % score->tempo)) {
        score->tempo = (60000000 / score->tempo) + 1;
    } else {
        score->tempo = 60000000 / score->tempo;
    }
    score->unk94 = score->tempo;
    cursor = score->seq_ptr;
    score->seq_ptr = cursor + 1;
    score->rhythm_n = cursor[0];
    cursor = score->seq_ptr;
    score->seq_ptr = cursor + 1;
    score->rhythm_d = cursor[0];
    cursor = score->seq_ptr;
    score->seq_ptr = cursor + 1;
    size_hi = cursor[0];
    score->seq_ptr = cursor + 2;
    size_mid_hi = cursor[1];
    score->seq_ptr = cursor + 3;
    size_mid_lo = cursor[2];
    score->seq_ptr = cursor + 4;
    data_len =
        (size_hi << 24) + (size_mid_hi << 16) + (size_mid_lo << 8) + cursor[3];
    len += 6;
    delta_value = _SsReadDeltaValue(flag, i);
    ticks = score->resolution * score->tempo;
    score->unk84 = delta_value;
    score->delta_value = delta_value;
    score->unk10 = 0;
    score->read_pos = score->seq_ptr;
    score->next_sep_pos = score->seq_ptr;
    score->loop_pos = score->seq_ptr;

    if ((ticks * 10) < (VBLANK_MINUS * 60)) {
        score->unk54 = score->unk52 = (VBLANK_MINUS * 600) / ticks;
    } else {
        score->unk52 = -1;
        score->unk54 =
            (score->resolution * score->tempo * 10) / (VBLANK_MINUS * 60);
        if ((VBLANK_MINUS * 30) <
            (score->resolution * score->tempo * 10) % (VBLANK_MINUS * 60)) {
            score->unk54++;
        }
    }
    score->unk56 = score->unk54;
    return len + data_len;
}
#endif
