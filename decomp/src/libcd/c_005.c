#include <common.h>
#include <libcd.h>
#include "libcd_stream.h"

void StSetStream(u_long mode, u_long start_frame, u_long end_frame,
                 void (*func1)(), void (*func2)()) {
    StSetMask(1U, start_frame, end_frame);
    StEmu_Addr = 0;
    StFunc1 = func1;
    StRgb24 = mode & 1;
    CChannel = 0;
    StCHANNEL = 0;
    Stsector_offset = 0;
    Stframe_no = 0;
    StFunc2 = func2;
}
