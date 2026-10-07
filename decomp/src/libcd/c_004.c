#include <common.h>
#include <libcd.h>
#include "libcd_stream.h"

CdlLOC previous_frame_location;
s32 next_frameid;
static s32 padding_D_800D1C60[2];

void data_ready_callback(void) {
    StHEADER* ptr = &StRingAddr[StRingIdx2];
    do {
    } while (0);

    ptr->id = 2;
    previous_frame_location = ptr->loc;
    next_frameid = ptr->frameCount;
    StRingIdx2 = StRingIdx1;
    if (StFunc1 != NULL) {
        StFunc1();
    }
    StFinalSector = 0;
}

s32 StGetBackloc(CdlLOC* loc) {
    if (StMode) {
        return -1;
    }
    CdIntToPos(CdPosToInt(&previous_frame_location) + 1, loc);
    return next_frameid;
}
