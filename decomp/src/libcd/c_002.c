#include <common.h>
#include <libcd.h>
#include "libcd_stream.h"

void (*StFunc1)(void), (*StFunc2)(void);

void StClearRing(void) {
    StRingIdx3 = 0;
    StRingIdx2 = 0;
    StRingIdx1 = 0;
    StFinalSector = 0;
    init_ring_status(0, StRingSize);
    StCdIntrFlag = 0;
    Stsector_offset = 0;
    Stframe_no = 0;
}
