#include "libspu_private.h"

long SpuSetReverb(long on_off) {
    short cnt;
    switch (on_off) {
    case 0:
        _spu_rev_flag = 0;
        cnt = _spu_RXX->rxx.spucnt;
        cnt &= ~0x80;
        _spu_RXX->rxx.spucnt = cnt;
        break;
    case 1:
        if ((_spu_rev_reserve_wa != on_off) &&
            _SpuIsInAllocateArea_(_spu_rev_offsetaddr)) {
            _spu_rev_flag = 0;
            cnt = _spu_RXX->rxx.spucnt;
            cnt &= ~0x80;
            _spu_RXX->rxx.spucnt = cnt;
        } else {
            _spu_rev_flag = on_off;
            cnt = _spu_RXX->rxx.spucnt;
            cnt |= 0x80;
            _spu_RXX->rxx.spucnt = cnt;
        }
        break;
    }
    return _spu_rev_flag;
}
