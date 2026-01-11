#include <common.h>
#include <libetc.h>
#include <kernel.h>

volatile u32 D_800B70C8 = 0x15067350;
volatile u32 D_800B70CC = 0x0040889C;
volatile s32* D_800B70D0 = 0x1F801814;
volatile s32* D_800B70D4 = 0x1F801110;
volatile s32 Hcount = 0;
volatile s32 D_800B70DC = 0;
volatile extern s32 Vcount;

void v_wait(int, int);
int VSync(int mode) {
    int syncFlag;
    int elapsed;
    int timeout;
    int v;
    int n;

    syncFlag = *D_800B70D0;
    elapsed = (*D_800B70D4 - Hcount) & 0xFFFF;
    if (mode < 0) {
        return Vcount;
    }
    if (mode == 1) {
        return elapsed;
    }
    n = 1;
    v = mode > 0 ? D_800B70DC - n + mode : D_800B70DC;
    timeout = mode > 0 ? mode - n : 0;
    v_wait(v, timeout);
    syncFlag = *D_800B70D0;
    v_wait(Vcount + 1, 1);
    if (syncFlag & 0x400000 && (syncFlag ^ *D_800B70D0) >= 0) {
        do {
        } while (!((syncFlag ^ *D_800B70D0) & 0x80000000));
    }
    D_800B70DC = Vcount;
    Hcount = *D_800B70D4;
    return elapsed;
}

void v_wait(int v, int timeout) {
    volatile int t = timeout << 15;
    while (Vcount < v) {
        if (!t--) {
            puts("VSync: timeout\n");
            ChangeClearPAD(0);
            ChangeClearRCnt(3, 0);
            return;
        }
    }
}
