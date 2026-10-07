#include "libsnd_private.h"

int _SsReadDeltaValue(short seq_access_num, short seq_num) {
    struct SeqStruct* score = &_ss_score[seq_access_num][seq_num];
    int value;
    unsigned char next;
    int result;

    value = *score->seq_ptr++;
    if (value == 0) {
        return 0;
    }
    if (value & 0x80) {
        value &= 0x7F;
        do {
            next = *score->seq_ptr++;
            value = (value << 7) + (next & 0x7F);
        } while (next & 0x80);
    }
    result = value * 10;
    score->unk88 += result;
    return result;
}
