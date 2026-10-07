#ifndef LIBCD_STREAM_H
#define LIBCD_STREAM_H
#include <common.h>
#include <libcd.h>

extern StHEADER* StRingAddr;
extern volatile s32 StRingSize;
extern s32 StRingIdx1, StRingIdx2, StRingIdx3, StFinalSector;
extern s32 Stframe_no, StCdIntrFlag, StEndFrame, StStartFrame, StSTART_FLAG;
extern s32 StRgb24, StMode, StCHANNEL, CChannel, StEmu_Addr;
extern s16 Stsector_offset;
extern void (*StFunc1)(void), (*StFunc2)(void);
extern int DS_active;
extern volatile u_char *D_800B5410, *D_800B541C;
int init_ring_status(s32 start, u32 count);
void data_ready_callback(void);
void StCdInterrupt(void);
int DsDataCallback(void (*callback)(void));
int DsReadyCallback(void (*callback)(u_char, u_char*));
#endif
