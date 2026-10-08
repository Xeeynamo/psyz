#include <common.h>
#include <libcd.h>
#include "libcd_stream.h"

int init_ring_status(s32 arg0, u32 arg1) {
    s32 i;

    for (i = 0; i < arg1; i++) {
        *((s32*)((u_char*)StRingAddr + ((i + arg0) << 5))) = 0;
    }
#ifdef __psyz
    return 0;
#endif
}
