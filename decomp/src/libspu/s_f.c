#include "libspu_private.h"

u_long SpuFlush(u_long ev) {
    int i;
    u_long ret;
    u_long mask;

    ret = 0;
    mask = ev;

    if ((mask == SPU_EVENT_ALL || ev & SPU_EVENT_PITCHLFO) &&
        _spu_RQmask & SPU_EVENT_PITCHLFO) {
        _spu_RQmask &= ~SPU_EVENT_PITCHLFO;
        SPUW(chan_fm[0], _spu_RQ[4]);
        SPUW(chan_fm[1], _spu_RQ[5]);
        ret |= SPU_EVENT_PITCHLFO;
    }
    if ((mask == SPU_EVENT_ALL || ev & SPU_EVENT_NOISE) &&
        _spu_RQmask & SPU_EVENT_NOISE) {
        _spu_RQmask &= ~SPU_EVENT_NOISE;
        SPUW(noise_mode[0], _spu_RQ[6]);
        SPUW(noise_mode[1], _spu_RQ[7]);
        ret |= SPU_EVENT_NOISE;
    }
    if ((mask == SPU_EVENT_ALL || ev & SPU_EVENT_REVERB) &&
        _spu_RQmask & SPU_EVENT_REVERB) {
        _spu_RQmask &= ~SPU_EVENT_REVERB;
        SPUW(rev_mode[0], _spu_RQ[8]);
        SPUW(rev_mode[1], _spu_RQ[9]);
        ret |= SPU_EVENT_REVERB;
    }
    if ((mask == SPU_EVENT_ALL || ev & SPU_EVENT_KEY) &&
        _spu_RQmask & SPU_EVENT_KEY) {
        _spu_RQmask &= ~SPU_EVENT_KEY;
        SPUW(key_on[0], _spu_RQ[0]);
        SPUW(key_on[1], _spu_RQ[1]);
        SPUW(key_off[0], _spu_RQ[2]);
        SPUW(key_off[1], _spu_RQ[3]);
        for (i = 0; i < 4; i++) {
            _spu_RQ[i] = 0;
        }
        ret |= SPU_EVENT_KEY;
        if (_spu_env & SPU_ENV_EVENT_QUEUEING) {
            _spu_keystat = _spu_RQvoice;
        }
    }
    return ret;
}
