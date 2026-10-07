#include <common.h>
#include <libcd.h>
#include "libcd_stream.h"

u_long StGetNext(u_long** addr, u_long** header) {
    volatile StHEADER* ptr;
    ptr = &StRingAddr[StRingIdx3];
    if (ptr->id == 1) {
        StRingIdx3 = 0;
        if (StEndFrame != 0) {
            ptr->id = 0;
        }
        ptr = &StRingAddr[StRingIdx3];
    }
    if (ptr->id == 2) {
        ptr->id = 4;
        *addr = (u_long*)(&StRingAddr[StRingSize] + (StRingIdx3 * 0x3F));
        *header = (u_long*)ptr;
        return 0;
    } else {
        return 1;
    }
}
