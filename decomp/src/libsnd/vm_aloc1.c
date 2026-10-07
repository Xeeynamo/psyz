#include "libsnd_private.h"

char _SsVmAlloc(short voice) {
    u8 alloc;
    u16 lowestKeyStat;
    char matches;
    u16 lowestAge;
    u8 lowest;
    u8 i;
    u16 lowestPrior;

    alloc = 99;
    lowestKeyStat = -1;
    matches = 0;
    lowestAge = 0;
    lowest = 99;
    lowestPrior = _svm_cur.tone_prior;
    for (i = 0; i < _SsVmMaxVoice; i++) {
        if (_svm_voice[i].unk1b == 0 && _svm_voice[i].key_stat == 0) {
            alloc = i;
            break;
        }
        if (_svm_voice[i].priority < lowestPrior) {
            lowestPrior = _svm_voice[i].priority;
            lowest = i;
            lowestKeyStat = _svm_voice[i].key_stat;
            lowestAge = _svm_voice[i].unk2;
            matches = 1;
        } else if (_svm_voice[i].priority == lowestPrior) {
            matches++;
            if (_svm_voice[i].key_stat < lowestKeyStat) {
                lowestAge = _svm_voice[i].unk2;
                lowestKeyStat = _svm_voice[i].key_stat;
                lowest = i;
            } else if (_svm_voice[i].key_stat == lowestKeyStat) {
                if (lowestAge < _svm_voice[i].unk2) {
                    lowestAge = _svm_voice[i].unk2;
                    lowest = i;
                }
            }
        }
    }
    if (alloc == 99) {
        alloc = lowest;
        if (matches == 0) {
            alloc = _SsVmMaxVoice;
        }
    }
    if (alloc < _SsVmMaxVoice) {
        for (i = 0; i < _SsVmMaxVoice; i++) {
            _svm_voice[i].unk2++;
        }
        _svm_voice[alloc].unk2 = 0;
        _svm_voice[alloc].priority = _svm_cur.tone_prior;
        if (_svm_voice[alloc].unk1b == 2) {
            SpuSetNoiseVoice(0, 0xFFFFFF);
        }
    }
    _svm_voice[alloc].auto_pan = 0;
    return alloc;
}
