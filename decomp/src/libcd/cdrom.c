#include <common.h>
#include <libcd.h>
#include "libcd_stream.h"

StHEADER* StRingAddr;
u_long* StRingBase;
s32 StRgb24, StEmu_Addr, StEmu_Idx;
volatile s32 StRingSize;
s32 StRingIdx1, StRingIdx2, StRingIdx3;
s32 StSTART_FLAG, StStartFrame, StEndFrame, StFinalSector, Stframe_no;
s16 Stsector_offset;
static s16 padding_D_800D1EF2;
s32 StCdIntrFlag, StCHANNEL, CChannel;
static s32 padding_D_800D1F00[2];

void StSetRing(u_long* ring_addr, u_long ring_size) {
    StRingAddr = (StHEADER*)ring_addr;
    StRingSize = ring_size;
    StClearRing();
}
