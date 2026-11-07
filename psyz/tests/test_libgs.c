#include <psyz.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgte.h>
#include <libgs.h>
#include "ztest.h"

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 240
#define OT_LENGTH 1

static GsOT ot[2];
static GsOT_TAG ot_tag[2][1 << OT_LENGTH];
static PACKET packets[2][64];
static TILE tile[2];

static const zimage_cmp* frame_cmp(int tol, float prec) {
    static zimage_cmp cmp;
#ifdef __PSP__
    if (tol < 2) {
        tol = 2;
    }
#endif
#ifdef __psx__
    tol = 0;
    prec = 1.0f;
#endif
    cmp = *zimage_r5g5b5_with_tol_prec(tol, prec);
    return &cmp;
}

#define ASSERT_FRAME(name, tol, prec)                                          \
    zexpect_image_eq(name, zimage_frontbuffer(), frame_cmp(tol, prec))

static void draw_frame(u_char r, u_char g, u_char b) {
    int buf = GsGetActiveBuff();
    GsSetWorkBase(packets[buf]);
    GsClearOt(0, 0, &ot[buf]);
    setTile(&tile[buf]);
    setRGB0(&tile[buf], 255, 0, 0);
    setXY0(&tile[buf], 32, 32);
    setWH(&tile[buf], 64, 64);
    AddPrim((OT_TYPE*)ot[buf].tag, &tile[buf]);
    GsSortClear(r, g, b, &ot[buf]);
    GsDrawOt(&ot[buf]);
    DrawSync(0);
    VSync(0);
    GsSwapDispBuff();
}

ZTEST_SETUP(gs) {
    Psyz_VideoSetDitheringMode(PSYZ_DITHER_OFF);
    Psyz_VideoSetInternalResolution(1);
    GsInitGraph(SCREEN_WIDTH, SCREEN_HEIGHT, GsNONINTER | GsOFSGPU, 0, 0);
    GsDefDispBuff(0, 0, 0, SCREEN_HEIGHT);
    for (int i = 0; i < 2; i++) {
        ot[i].length = OT_LENGTH;
        ot[i].org = ot_tag[i];
    }
}

ZTEST_TEARDOWN(gs) { ResetGraph(0); }

ZTEST(gs, double_buffer) {
    for (int i = 0; i < 3; i++) {
        draw_frame(64, 64, 64);
        draw_frame(0, 40, 120);
    }
    draw_frame(64, 64, 64);
    ASSERT_FRAME("gs_double_buffer_0", 0, 1.0f);
    draw_frame(0, 40, 120);
    ASSERT_FRAME("gs_double_buffer_1", 0, 1.0f);
}
