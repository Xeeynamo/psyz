#include <common.h>
#include <libgte.h>
#include <libgpu.h>

typedef struct {
    OT_TYPE tag;
    u_long code[2];
} CntDrEnv;

extern CntDrEnv _CntDrEnv;

static volatile u32* d2_madr = (volatile u32*)0x1F8010A0;
static volatile u32* d2_bcr = (volatile u32*)0x1F8010A4;
static volatile u32* d2_chcr = (volatile u32*)0x1F8010A8;
static volatile u32* gp0 = (volatile u32*)0x1F801810;
static volatile u32* gp1 = (volatile u32*)0x1F801814;

u_long* BreakDraw(void) {
    volatile u_long temp;

    if (!(*d2_chcr & 0x1000000)) {
        return NULL;
    }

    if (((*d2_chcr & 0x700) >> 8) == 4) {
        *d2_chcr = *d2_chcr & 0xFEFFFFFF;

        temp = *d2_chcr;
        temp = *d2_madr;

        if ((temp & 0xFFFFFF) == 0xFFFFFF) {
            return NULL;
        }
        return (u_long*)temp;
    }
    return (u_long*)-1;
}

int IsIdleGPU(int max_count) {
    int i;

    for (i = 0; !(*gp1 & 0x4000000);) {
        if (i++ > max_count) {
            return -1;
        }
    }
    return 0;
}

void ContinueDraw(u_long* insaddr, u_long* contaddr) {
    u_long* prim;

    setlen(&_CntDrEnv, 2);
    _CntDrEnv.code[0] = (*gp1 & 0x7FF) | 0xE1000000;
    _CntDrEnv.code[1] = 0;

    prim = insaddr;
    while (!isendprim(nextPrim(prim))) {
        prim = nextPrim(prim);
    }

    if (contaddr) {
        setaddr(&_CntDrEnv, contaddr);
    } else {
        setaddr(&_CntDrEnv, getaddr(prim));
    }
    setaddr(prim, &_CntDrEnv);

    *gp1 = 0x04000002;
    *d2_madr = (u_long)insaddr;
    *d2_bcr = 0;
    *d2_chcr = 0x01000401;
}
