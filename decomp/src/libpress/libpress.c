#include <common.h>
#include <libpress.h>
#include <libetc.h>
#include "libpress_private.h"

extern u32 D_800BF740[], D_800BF744[], D_800BF784[];
extern u32 D_800BF7C4[], D_800BF7C8[];

void DecDCTReset(int mode) {
    if (mode == 0)
        ResetCallback();
    MDEC_reset(mode);
}
DECDCTENV* DecDCTGetEnv(DECDCTENV* env) {
    int i;
    u32 *dst, *src;
    dst = (u32*)env->iq_y;
    src = D_800BF744;
    for (i = 15; i != -1; i--)
        *dst++ = *src++;
    dst = (u32*)env->iq_c;
    src = D_800BF784;
    for (i = 15; i != -1; i--)
        *dst++ = *src++;
    dst = (u32*)env->dct;
    src = D_800BF7C8;
    for (i = 31; i != -1; i--)
        *dst++ = *src++;
    return env;
}
DECDCTENV* DecDCTPutEnv(DECDCTENV* env) {
    int i;
    u32 *dst1, *src1, *dst2, *src2;
    dst1 = D_800BF744;
    src1 = (u32*)env->iq_y;
    for (i = 15; i != -1; i--)
        *dst1++ = *src1++;
    dst2 = D_800BF784;
    src2 = (u32*)env->iq_c;
    for (i = 15; i != -1; i--)
        *dst2++ = *src2++;
    MDEC_in((u_long*)D_800BF740, 32);
    MDEC_in((u_long*)D_800BF7C4, 32);
    return env;
}
int DecDCTBufSize(u_long* bs) { return *(u_short*)bs; }
void DecDCTin(u_long* buf, int mode) {
    if (mode & 1)
        *(u32*)buf &= 0xF7FFFFFF;
    else
        *(u32*)buf |= 0x08000000;
    if (mode & 2)
        *(u32*)buf |= 0x02000000;
    else
        *(u32*)buf &= 0xFDFFFFFF;
    MDEC_in(buf, *(u_short*)buf);
}
void DecDCTout(u_long* buf, int size) { MDEC_out(buf, size); }
int DecDCTinSync(int mode) {
    int result;
    if (mode == 0)
        result = MDEC_in_sync();
    else
        result = (MDEC_status() >> 29) & 1;
    return result;
}
int DecDCToutSync(int mode) {
    int result;
    if (mode == 0)
        result = MDEC_out_sync();
    else
        result = (MDEC_status() >> 24) & 1;
    return result;
}
int DecDCTinCallback(void (*cb)()) { return DMACallback(0, cb); }
int DecDCToutCallback(void (*cb)()) { return DMACallback(1, cb); }
INCLUDE_ASM("asm/nonmatchings/libpress/libpress", MDEC_reset);
INCLUDE_ASM("asm/nonmatchings/libpress/libpress", MDEC_in);
INCLUDE_ASM("asm/nonmatchings/libpress/libpress", MDEC_out);
INCLUDE_ASM("asm/nonmatchings/libpress/libpress", MDEC_in_sync);
INCLUDE_ASM("asm/nonmatchings/libpress/libpress", MDEC_out_sync);
INCLUDE_ASM("asm/nonmatchings/libpress/libpress", MDEC_status);
INCLUDE_ASM("asm/nonmatchings/libpress/libpress", timeout);
