#include <common.h>
#include <libcd.h>
#include "libcd_stream.h"

void StSetMask(u_long mask, u_long start, u_long end) {
    StSTART_FLAG = mask;
    StStartFrame = start;
    StEndFrame = end;
}
