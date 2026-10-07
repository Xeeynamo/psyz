#include "libsnd_private.h"

short SsSeqOpenJ(u_long* addr, short vab_id) {
    short bit;
    short flag;
    unsigned char exit_loop;
    u8* seq;

    flag = 0;
    seq = (u8*)addr;
    if (_snd_openflag == -1) {
        printf("Can't Open Sequence data any more\n\n");
        return -1;
    }
    bit = 0;
    exit_loop = 0;
    while (!exit_loop) {
        if ((_snd_openflag & (1 << bit)) == 0U) {
            flag = bit;
            exit_loop = 1;
        }
        bit++;
    }
    _snd_openflag |= 1 << flag;
    if (_SsInitSoundSeq(flag, vab_id, seq) == -1) {
        return -1;
    }
    return flag;
}
