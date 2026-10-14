#include <common.h>
#include <libpress.h>
#include <libetc.h>
#include "libpress_private.h"

static u32 mdec_iq[] = {
    0x40000001, 0x13101002, 0x16161310, 0x16161616, 0x1B1A181A, 0x1A1A1B1B,
    0x1B1B1A1A, 0x1D1D1D1B, 0x1D222222, 0x1B1B1D1D, 0x20201D1D, 0x26252222,
    0x22232325, 0x28262623, 0x30302828, 0x38382E2E, 0x5345453A, 0x13101002,
    0x16161310, 0x16161616, 0x1B1A181A, 0x1A1A1B1B, 0x1B1B1A1A, 0x1D1D1D1B,
    0x1D222222, 0x1B1B1D1D, 0x20201D1D, 0x26252222, 0x22232325, 0x28262623,
    0x30302828, 0x38382E2E, 0x5345453A};

static u32 mdec_coef[] = {
    0x60000000, 0x5A825A82, 0x5A825A82, 0x5A825A82, 0x5A825A82, 0x6A6D7D8A,
    0x18F8471C, 0xB8E3E707, 0x82759592, 0x30FB7641, 0x89BECF04, 0xCF0489BE,
    0x764130FB, 0xE7076A6D, 0xB8E38275, 0x7D8A471C, 0x959218F8, 0xA57D5A82,
    0x5A82A57D, 0xA57D5A82, 0x5A82A57D, 0x8275471C, 0x6A6D18F8, 0xE7079592,
    0xB8E37D8A, 0x89BE30FB, 0xCF047641, 0x7641CF04, 0x30FB89BE, 0xB8E318F8,
    0x82756A6D, 0x95927D8A, 0xE707471C};

#ifndef __psyz
static u32 D_800BF848[] = {0x150E7350, 0x0040AE9C};

static volatile u32* mdec_d0_madr = (volatile u32*)0x1F801080;
static volatile u32* mdec_d0_bcr = (volatile u32*)0x1F801084;
static volatile u32* mdec_d0_chcr = (volatile u32*)0x1F801088;
static volatile u32* mdec_d1_madr = (volatile u32*)0x1F801090;
static volatile u32* mdec_d1_bcr = (volatile u32*)0x1F801094;
static volatile u32* mdec_d1_chcr = (volatile u32*)0x1F801098;
static volatile u32* mdec_d2_madr = (volatile u32*)0x1F8010A0;
static volatile u32* mdec_d2_bcr = (volatile u32*)0x1F8010A4;
static volatile u32* mdec_d2_chcr = (volatile u32*)0x1F8010A8;
static volatile u32* mdec_d3_madr = (volatile u32*)0x1F8010B0;
static volatile u32* mdec_d3_bcr = (volatile u32*)0x1F8010B4;
static volatile u32* mdec_d3_chcr = (volatile u32*)0x1F8010B8;
static volatile u32* mdec0 = (volatile u32*)0x1F801820;
static volatile u32* mdec1 = (volatile u32*)0x1F801824;
static volatile u32* mdec_d_pcr = (volatile u32*)0x1F8010F0;
#endif

void DecDCTReset(int mode) {
    if (mode == 0)
        ResetCallback();
    MDEC_reset(mode);
}
DECDCTENV* DecDCTGetEnv(DECDCTENV* env) {
    int i;
    u32 *dst, *src;
    dst = (u32*)env->iq_y;
    src = &mdec_iq[1];
    for (i = 15; i != -1; i--)
        *dst++ = *src++;
    dst = (u32*)env->iq_c;
    src = &mdec_iq[17];
    for (i = 15; i != -1; i--)
        *dst++ = *src++;
    dst = (u32*)env->dct;
    src = &mdec_coef[1];
    for (i = 31; i != -1; i--)
        *dst++ = *src++;
    return env;
}
DECDCTENV* DecDCTPutEnv(DECDCTENV* env) {
    int i;
    u32 *dst1, *src1, *dst2, *src2;
    dst1 = &mdec_iq[1];
    src1 = (u32*)env->iq_y;
    for (i = 15; i != -1; i--)
        *dst1++ = *src1++;
    dst2 = &mdec_iq[17];
    src2 = (u32*)env->iq_c;
    for (i = 15; i != -1; i--)
        *dst2++ = *src2++;
    MDEC_in((u_long*)mdec_iq, 32);
    MDEC_in((u_long*)mdec_coef, 32);
    return env;
}
int DecDCTBufSize(u_long* bs) { return *(u_short*)bs; }
void DecDCTin(u_long* buf, int mode) {
    if (mode & 1)
        *(u32*)buf &= ~0x08000000;
    else
        *(u32*)buf |= 0x08000000;
    if (mode & 2)
        *(u32*)buf |= 0x02000000;
    else
        *(u32*)buf &= ~0x02000000;
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
#ifdef __psyz
DecDCCb DecDCTinCallback(DecDCCb cb) { return DMACallback(0, cb); }
DecDCCb DecDCToutCallback(DecDCCb cb) { return DMACallback(1, cb); }
#else
int DecDCTinCallback(void (*cb)()) { return DMACallback(0, cb); }
int DecDCToutCallback(void (*cb)()) { return DMACallback(1, cb); }
#endif
#ifndef __psyz
void MDEC_reset(int mode) {
    switch (mode) {
    case 0:
        *mdec1 = 0x80000000;
        *mdec_d0_chcr = 0;
        *mdec_d1_chcr = 0;
        *mdec1 = 0x60000000;
        MDEC_in((u_long*)mdec_iq, 32);
        MDEC_in((u_long*)mdec_coef, 32);
        return;
    case 1:
        *mdec1 = 0x80000000;
        *mdec_d0_chcr = 0;
        *mdec_d1_chcr = 0;
        *mdec_d1_chcr;
        *mdec1 = 0x60000000;
        return;
    default:
        printf("MDEC_rest:bad option(%d)\n", mode);
        return;
    }
}

void MDEC_in(u_long* buf, int size) {
    MDEC_in_sync();
    *mdec_d_pcr |= 0x88;
    *mdec_d0_madr = (u32)buf + 4;
    *mdec_d0_bcr = (((u32)size >> 5) << 16) | 0x20;
    *mdec0 = *(u32*)buf;
    *mdec_d0_chcr = 0x01000201;
}

void MDEC_out(u_long* buf, int size) {
    MDEC_out_sync();
    *mdec_d_pcr |= 0x88;
    *mdec_d1_chcr = 0;
    *mdec_d1_madr = (u32)buf;
    *mdec_d1_bcr = (((u32)size >> 5) << 16) | 0x20;
    *mdec_d1_chcr = 0x01000200;
}

int timeout(char* name);

int MDEC_in_sync(void) {
    volatile int retries = 0x100000;
    while (*mdec1 & 0x20000000) {
        if (--retries == -1) {
            timeout("MDEC_in_sync");
            return -1;
        }
    }
    return 0;
}

int MDEC_out_sync(void) {
    volatile int retries = 0x100000;
    while (*mdec_d1_chcr & 0x01000000) {
        if (--retries == -1) {
            timeout("MDEC_out_sync");
            return -1;
        }
    }
    return 0;
}

u_long MDEC_status(void) { return *mdec1; }

static const char timeout_dma[] = "\t DMA=(%d,%d), ADDR=(0x%08x->0x%08x)\n";
static const char timeout_fifo[] =
    "\t FIFO=(%d,%d),BUSY=%d,DREQ=(%d,%d),RGB24=%d,STP=%d\n";

int timeout(char* name) {
    u32 status;
    printf("%s timeout:\n", name);
    status = *mdec1;
    printf(timeout_dma, (*mdec_d0_chcr >> 24) & 1, (*mdec_d1_chcr >> 24) & 1,
           *mdec_d0_madr, *mdec_d1_madr);
    printf(timeout_fifo, (~status >> 31) & 1, (status >> 30) & 1,
           (status >> 29) & 1, (status >> 28) & 1, (status >> 27) & 1,
           (status >> 25) & 1, (status >> 23) & 1);
    *mdec1 = 0x80000000;
    *mdec_d0_chcr = 0;
    *mdec_d1_chcr = 0;
    *mdec_d1_chcr;
    *mdec1 = 0x60000000;
    return 0;
}
#endif
