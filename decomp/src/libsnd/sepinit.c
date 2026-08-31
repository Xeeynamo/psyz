#include "libsnd_private.h"

#ifndef __psyz
INCLUDE_ASM("asm/nonmatchings/libsnd/sepinit", _SsInitSoundSep);
#else
int _SsInitSoundSep(short flag, short i, short vab_id, unsigned long* addr) {
    int len = 0;
    struct SeqStruct* score = &_ss_score[flag][i];
    int channel;
    u8 resolution_hi;
    u8 resolution_lo;
    u8 tempo_hi;
    u8 tempo_mid;
    int tempo;
    int tempo_quotient;
    int tempo_remainder;
    char rhythm_n;
    char rhythm_d;
    int size_hi;
    u8 size_mid_hi;
    u8 size_mid_lo;
    u8 size_lo;
    s32 delta_value;
    int data_len;
    unsigned int ticks;
    u8* cursor;

    score->unk20 = 1;
    score->unk15 = 0;
    score->unk16 = 0;
    score->unk17 = 0;
    score->unk18 = 0;
    score->unk19 = 0;
    score->unk1A = 0;
    score->unk1B = 0;
    score->unk1C = 0;
    score->unk1D = 0;
    score->unk1E = 0;
    score->unk1F = 0;
    score->play_mode = 0;
    score->unk21 = 0;
    score->unk52 = 1;
    score->unk50 = 0;
    score->unk26 = vab_id;
    score->unk56 = 0;
    score->unk84 = 0;
    score->delta_value = 0;
    score->unk8c = 0;
    score->unk90 = 0;
    score->unk80 = 0;
    score->unk24 = 0;
    for (channel = 0; channel < 16; channel++) {
        score->panpot[channel] = 0x40;
        score->programs[channel] = channel;
        score->vol[channel] = 0x7F;
    }

    score->unk0 = (u8*)addr;
    if (i == 0) {
        if ((*score->unk0 == 'S') || (*score->unk0 == 'p')) {
            score->unk0 = (u8*)addr + 6;
            if (((u8*)addr)[5] != 0) {
                printf("This is not SEP Data.\n");
                return -1;
            }
            score->unk0 = (u8*)addr + 8;
            len += 8;
        }
    } else {
        score->unk0 = (u8*)addr + 2;
        len += 2;
    }

    {
        int usec_per_minute = 60000000;

        cursor = score->unk0;
        score->unk0 = cursor + 1;
        resolution_hi = cursor[0];
        score->unk0 = cursor + 2;
        resolution_lo = cursor[1];
        score->unk50 = resolution_lo | (resolution_hi << 8);
        cursor = score->unk0;
        score->unk0 = cursor + 1;
        tempo_hi = cursor[0];
        score->unk0 = cursor + 2;
        tempo_mid = cursor[1];
        score->unk0 = cursor + 3;
        tempo = (tempo_hi << 16) | (tempo_mid << 8) | cursor[2];
        score->unk8c = tempo;
        len += 5;
        tempo_quotient = usec_per_minute / tempo;
        tempo_remainder = usec_per_minute % tempo;
        if ((tempo / 2) < tempo_remainder)
            score->unk8c = tempo_quotient + 1;
        else
            score->unk8c = tempo_quotient;
    }

    score->unk94 = score->unk8c;
    rhythm_n = *score->unk0++;
    ((u8*)&score->unk24)[0] = rhythm_n;
    rhythm_d = *score->unk0++;
    ((u8*)&score->unk24)[1] = rhythm_d;
    cursor = score->unk0;
    score->unk0 = cursor + 1;
    size_hi = cursor[0];
    score->unk0 = cursor + 2;
    size_mid_hi = cursor[1];
    score->unk0 = cursor + 3;
    size_mid_lo = cursor[2];
    score->unk0 = cursor + 4;
    size_lo = cursor[3];
    data_len =
        (size_hi << 24) + (size_mid_hi << 16) + (size_mid_lo << 8) + size_lo;
    delta_value = _SsReadDeltaValue(flag, i);
    ticks = score->unk50 * score->unk8c;
    score->unk84 = delta_value;
    score->unk90 = delta_value;
    *(u32*)&score->unk10 = 0;
    score->next_sep_pos = score->unk0;
    score->read_pos = score->unk0;
    score->loop_pos = score->unk0;
    len += 6;

    if ((ticks * 10) < (VBLANK_MINUS * 60)) {
        score->unk54 = score->unk52 = (VBLANK_MINUS * 600) / ticks;
    } else {
        score->unk52 = -1;
        score->unk54 = (score->unk50 * score->unk8c * 10) / (VBLANK_MINUS * 60);
        if ((VBLANK_MINUS * 30) <
            (score->unk50 * score->unk8c * 10) % (VBLANK_MINUS * 60))
            score->unk54++;
    }
    score->unk56 = score->unk54;
    return len + data_len;
}
#endif
