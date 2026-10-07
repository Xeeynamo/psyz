#include <common.h>
#include <libcd.h>
#include "libcd_stream.h"

void StSetRing(u_long* ring_addr, u_long ring_size) {
    StRingAddr = (StHEADER*)ring_addr;
    StRingSize = ring_size;
    StClearRing();
}
