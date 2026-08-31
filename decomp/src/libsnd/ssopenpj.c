#include "libsnd_private.h"

short SsSepOpenJ(unsigned long* addr, short vab_id, short seq_cnt) {
    short bit;
    short i;
    short flag;
    int len;
    unsigned char var_a3;

    flag = 0;
    if (_snd_openflag == -1) {
        printf("Can't Open Sequence data any more\n\n");
        return -1;
    }
    bit = 0;
    var_a3 = 0;
    do {
        if ((_snd_openflag & (1 << bit)) == 0U) {
            flag = bit;
            var_a3 = 1;
        }
        bit++;
    } while (var_a3 == 0);
    _snd_openflag |= 1 << flag;
    for (i = 0; i < seq_cnt; i++) {
        len = _SsInitSoundSep(flag, i, vab_id, addr);
        var_a3 = 1;
        if (len == -var_a3) {
            return -1;
        }
        addr = (unsigned long*)((unsigned char*)addr + len);
    }
    return flag;
}
