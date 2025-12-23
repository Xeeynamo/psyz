#include "libspu_private.h"
#include "libapi.h"

long SpuIsTransferCompleted(long flag) {
    s32 in_transfer;

    if (_spu_trans_mode == 1 || _spu_inTransfer == 1) {
        return 1;
    }
    in_transfer = TestEvent(_spu_EVdma);
    if (flag == 1) {
        if (in_transfer == 0) {
            do {
            } while (TestEvent(_spu_EVdma) == 0);
        }
        in_transfer = 1;
        _spu_inTransfer = in_transfer;
        return in_transfer;
    }
    if (in_transfer == 1) {
        _spu_inTransfer = in_transfer;
    }
    return in_transfer;
}
