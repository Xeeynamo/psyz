#include "libsnd_private.h"

short _SsInitSoundSeq(short seq_no, short vab_id, u8* addr) {
    struct SeqStruct* score = _ss_score[seq_no];
    int channel;
    u8 hi;
    u8 mid;

    score->vab_id = vab_id;
    score->resolution = 0;
    score->unk18 = 0;
    score->unk19 = 0;
    score->unk1E = 0;
    score->fn_idx = 0;
    score->unk1B = 0;
    score->unk1F = 0;
    score->channel_idx = 0;
    score->unk84 = 0;
    score->unk88 = 0;
    score->tempo = 0;
    score->unk56 = 0;
    score->unk21 = 0;
    score->l_count = 1;
    score->play_mode = 0;
    score->delta_value = 0;
    score->unk1C = 0;
    score->unk1D = 0;
    score->unk15 = 0;
    score->running_status = 0;
    score->channel_mute = 0;
    score->rhythm_n = 0;
    score->rhythm_d = 0;
    for (channel = 0; channel < 16; channel++) {
        score->programs[channel] = channel;
        score->panpot[channel] = 0x40;
        score->vol[channel] = 0x7F;
    }
    score->unk52 = 1;
    score->seq_ptr = addr;
    if (*score->seq_ptr == 'S' || *score->seq_ptr == 'p') {
        score->seq_ptr += 8;
        if (addr[7] != 1) {
            printf("This is not SEQ Data.\n");
            return -1;
        }
    } else {
        printf("This is an old SEQ Data Format.\n");
        return 0;
    }
    hi = *score->seq_ptr++;
    score->resolution = (hi << 8) | *score->seq_ptr++;
    hi = *score->seq_ptr++;
    mid = *score->seq_ptr++;
    score->tempo = (hi << 16) | (mid << 8) | *score->seq_ptr++;
    if (score->tempo / 2 < 60000000 % score->tempo) {
        score->tempo = 60000000 / score->tempo + 1;
    } else {
        score->tempo = 60000000 / score->tempo;
    }
    score->unk94 = score->tempo;
    score->rhythm_n = *score->seq_ptr++;
    score->rhythm_d = *score->seq_ptr++;
    score->delta_value = score->unk84 = _SsReadDeltaValue(seq_no, 0);
    score->unk10 = 0;
    score->read_pos = score->seq_ptr;
    score->next_sep_pos = score->seq_ptr;
    score->loop_pos = score->seq_ptr;
    if (score->resolution * score->tempo * 10 < VBLANK_MINUS * 60) {
        score->unk54 = score->unk52 =
            VBLANK_MINUS * 600 / (score->resolution * score->tempo);
    } else {
        score->unk52 = -1;
        score->unk54 =
            score->resolution * score->tempo * 10 / (VBLANK_MINUS * 60);
        if (VBLANK_MINUS * 30 <
            score->resolution * score->tempo * 10 % (VBLANK_MINUS * 60)) {
            score->unk54++;
        }
    }
    score->unk56 = score->unk54;
    return 0;
}
