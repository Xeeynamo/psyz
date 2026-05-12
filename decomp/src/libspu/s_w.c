#include "libspu_private.h"

unsigned long SpuWrite(unsigned char* addr, unsigned long size) {
    if (size > 0x7EFF0) {
        size = 0x7EFF0;
    }
    _spu_Fw(addr, size);
    if (_spu_transferCallback == NULL) {
        _spu_inTransfer = 0;
    }
    return size;
}
