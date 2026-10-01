#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <psyz.h>
#include <kernel.h>
#include <libetc.h>
#include <libgpu.h>

#include "ztest.h"

#include "res/4bpp.h"
#include "res/16bpp.h"
#include "res/uv4bpp.h"

#ifndef LEN
#define LEN(x) ((s32)(sizeof(x) / sizeof(*(x))))
#endif

enum {
    OT_LENGTH = 1,
    OTSIZE = 1 << OT_LENGTH,
    SCREEN_WIDTH = 256,
    SCREEN_HEIGHT = 240,
};

typedef struct DBuf {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OTSIZE];
    POLY_F4 f4[8];
    POLY_FT4 ft4[8];
    POLY_G4 g4[4];
    POLY_GT4 gt4[4];
    LINE_G2 lineg2[4];
    LINE_G3 lineg3[2];
    LINE_G4 lineg4[2];
    SPRT sprt[4];
    TILE tile[4];
    DR_MODE drmode[2];
    DR_TWIN twin[4];
} DBuf;

static DBuf db[2];
static DBuf* cdb;

static void gpu_setup(void) {
    RECT clearRect = {0, 0, 0x7FFF, 0x7FFF};
    Psyz_VideoSetDitheringMode(PSYZ_DITHER_OFF);
    Psyz_VideoSetInternalResolution(1);
    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDrawEnv(&db[1].draw, SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[1].disp, SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    db[0].draw.dtd = db[1].draw.dtd = 0; // disable dithering by default
    ResetGraph(0);
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);
    ClearOTagR(db[0].ot, OTSIZE);
    ClearOTagR(db[1].ot, OTSIZE);
    SetDispMask(1);
    ClearImage(&clearRect, 0, 0, 0);
    DrawSync(0);
    cdb = &db[0];
}

static void gpu_teardown(void) { ResetGraph(0); }

static const zimage_cmp* frame_cmp(int tol, float prec) {
    static zimage_cmp cmp;
#ifdef __PSP__
    // GU_COLOR_5551 is slightly brighter than PS1, account for error margin
    if (tol < 2) {
        tol = 2;
    }
#endif
#ifdef __psx__
    // real hardware is the reference, its frames must be identical
    tol = 0;
    prec = 1.0f;
#endif
    cmp = *zimage_r5g5b5_with_tol_prec(tol, prec);
    return &cmp;
}

#define ASSERT_FRAME(name, tol, prec)                                          \
    zexpect_image_eq(name, zimage_frontbuffer(), frame_cmp(tol, prec))

static void Present(const char* golden) {
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME(golden, 0, 1.0f);
}

static int LoadTim(void* data, u_short* outTpage, u_short* outClut) {
    TIM_IMAGE tim;
    if (OpenTIM((u_long*)data)) {
        return 1;
    }
    if (!ReadTIM(&tim)) {
        return 1;
    }
    LoadImage(tim.prect, tim.paddr);
    if (outTpage) {
        *outTpage = GetTPage((int)tim.mode, 0, tim.prect->x, tim.prect->y);
    }
    if (tim.caddr) {
        LoadImage(tim.crect, tim.caddr);
        if (outClut) {
            *outClut = GetClut(tim.crect->x, tim.crect->y);
        }
    }
    return 0;
}

static void SetPolyF4Img(POLY_FT4* poly, int x, int y, int w, int h, int u,
                         int v, u_short tpage, u_short clut, int semitrans) {
    SetPolyFT4(poly);
    setXYWH(poly, x, y, w, h);
    setRGB0(poly, 255, 128, 128);
    setUVWH(poly, u, v, w, h);
    setSemiTrans(poly, semitrans);
    poly->tpage = tpage;
    poly->clut = clut;
}

ZTEST_SETUP(gpu) { gpu_setup(); }
ZTEST_TEARDOWN(gpu) { gpu_teardown(); }

ZTEST(gpu, fnt_print) {
    FntLoad(960, 256);
    SetDumpFnt(FntOpen(4, 4, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 512));
    ClearImage(&cdb->draw.clip, 60, 120, 120);
    FntPrint("hello psyz!");
    FntFlush(-1);
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&cdb->draw);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME("fnt_print", 0, 1.0f);
}

ZTEST(gpu, draw_ft4) {
    u_short tpage, clut;
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }
    SetPolyFT4(&cdb->ft4[0]);
    setXYWH(&cdb->ft4[0], 16, 16, 64, 64);
    setRGB0(&cdb->ft4[0], 128, 128, 128);
    setUVWH(&cdb->ft4[0], 0, 0, 64, 64);
    setSemiTrans(&cdb->ft4[0], 0);
    cdb->ft4[0].tpage = tpage;
    cdb->ft4[0].clut = clut;

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].ft4[0]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME("draw_ft4", 0, 1.0f);
}

ZTEST(gpu, draw_ft4_colored) {
    u_short tpage, clut;
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }
    SetPolyFT4(&cdb->ft4[0]);
    setXYWH(&cdb->ft4[0], 16, 16, 64, 64);
    setRGB0(&cdb->ft4[0], 255, 128, 128);
    setUVWH(&cdb->ft4[0], 0, 0, 64, 64);
    setSemiTrans(&cdb->ft4[0], 0);
    cdb->ft4[0].tpage = tpage;
    cdb->ft4[0].clut = clut;

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].ft4[0]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME("draw_ft4_colored", 0, 1.0f);
}

ZTEST(gpu, draw_gt4) {
    zskip_targets("pcsx-redux"); // differs from real hardware
    u_short tpage, clut;
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }
    SetPolyGT4(&cdb->gt4[0]);
    setXYWH(&cdb->gt4[0], 16, 16, 64, 64);
    setRGB0(&cdb->gt4[0], 128, 0, 0);
    setRGB1(&cdb->gt4[0], 0, 128, 0);
    setRGB2(&cdb->gt4[0], 0, 0, 128);
    setRGB3(&cdb->gt4[0], 128, 128, 0);
    setUVWH(&cdb->gt4[0], 0, 0, 64, 64);
    setSemiTrans(&cdb->gt4[0], 0);
    cdb->gt4[0].tpage = tpage;
    cdb->gt4[0].clut = clut;

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].gt4[0]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME("draw_gt4", 1, 1.0f);
}

ZTEST(gpu, draw_sprt_8bpp) {
    TIM_IMAGE tim;
    zassert_s32_eq(0, OpenTIM((u_long*)img_4bpp));
    zassert_ptr_ne(NULL, ReadTIM(&tim));

    static u_short img8bpp[128 * 256];
    RECT rect = *tim.prect;
    u_char* src = (u_char*)tim.paddr;
    u_char* dst = (u_char*)img8bpp;
    int i;
    for (i = 0; i < rect.w * rect.h * 2; i++) {
        dst[i * 2 + 0] = src[i] & 15;
        dst[i * 2 + 1] = src[i] >> 4;
    }
    rect.w *= 2;
    LoadImage(&rect, (u_long*)img8bpp);
    LoadImage(tim.crect, tim.caddr);

    u_short tpage, clut;
    tpage = GetTPage(1, 0, rect.x, rect.y);
    clut = GetClut(tim.crect->x, tim.crect->y);

    SetSprt(&cdb->sprt[0]);
    setShadeTex(&cdb->sprt[0], 1);
    setXY0(&cdb->sprt[0], 16, 16);
    setWH(&cdb->sprt[0], rect.w * 2, rect.h);
    setUV0(&cdb->sprt[0], 0, 0);
    cdb->sprt[0].clut = clut;
    SetDrawMode(&cdb->drmode[0], 0, 0, tpage, NULL);
    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &cdb->sprt[0]);
    ClearImage(&cdb->draw.clip, 60, 120, 120);
    AddPrim(cdb->ot, &cdb->drmode[0]);
    Present("draw_sprt_8bpp");
}

ZTEST(gpu, draw_sprt_16bpp) {
    u_short tpage, clut;
    zassert_s32_eq(0, LoadTim(img_16bpp, &tpage, &clut));
    SetSprt(&cdb->sprt[0]);
    setShadeTex(&cdb->sprt[0], 1);
    setXY0(&cdb->sprt[0], 16, 16);
    setWH(&cdb->sprt[0], 64, 64);
    setUV0(&cdb->sprt[0], 0, 0);
    SetDrawMode(&cdb->drmode[0], 0, 0, tpage, NULL);
    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &cdb->sprt[0]);
    ClearImage(&cdb->draw.clip, 60, 120, 120);
    AddPrim(cdb->ot, &cdb->drmode[0]);
    Present("draw_sprt_16bpp");
}

ZTEST(gpu, draw_sprt_16bpp_page_past_vram_edge) {
    TIM_IMAGE tim;
    zassert_s32_eq(0, OpenTIM((u_long*)img_16bpp));
    zassert_ptr_ne(NULL, ReadTIM(&tim));
    RECT rect = *tim.prect;
    rect.x = 960;
    rect.y = 0;
    LoadImage(&rect, tim.paddr);
    SetSprt(&cdb->sprt[0]);
    setShadeTex(&cdb->sprt[0], 1);
    setXY0(&cdb->sprt[0], 16, 16);
    setWH(&cdb->sprt[0], 64, 64);
    setUV0(&cdb->sprt[0], 128, 0);
    SetDrawMode(&cdb->drmode[0], 0, 0, GetTPage(2, 0, 832, 0), NULL);
    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &cdb->sprt[0]);
    ClearImage(&cdb->draw.clip, 60, 120, 120);
    AddPrim(cdb->ot, &cdb->drmode[0]);
    Present("draw_sprt_16bpp");
}

ZTEST(gpu, gouraud_line_after_flush) {
    int w, h;
    unsigned char* d;
    const unsigned char* p87;
    const unsigned char* p88;
    const unsigned char* p;

    SetPolyF4(&cdb->f4[0]);
    setXYWH(&cdb->f4[0], 16, 16, 64, 64);
    setRGB0(&cdb->f4[0], 0, 0, 255);
    setSemiTrans(&cdb->f4[0], 0);

    SetLineG2(&cdb->lineg2[0]);
    setXY2(&cdb->lineg2[0], 16, 88, 80, 88);
    setRGB0(&cdb->lineg2[0], 255, 0, 0);
    setRGB1(&cdb->lineg2[0], 0, 255, 0);
    setSemiTrans(&cdb->f4[0], 0);

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].lineg2[0]);
    AddPrim(cdb->ot, &db[0].f4[0]);

    ClearImage(&cdb->draw.clip, 0, 0, 0);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    d = Psyz_VideoAllocCapturedFrame(&w, &h);
    zassert_ptr_ne(NULL, d);

    // Linux and Windows renders the line at y:87, macOS does it at y:88
    p87 = d + 3 * (87 * w + 48);
    p88 = d + 3 * (88 * w + 48);

    p = (p87[0] + p87[1] > p87[2]) ? p87 : p88;
    zprintf("line should be red/green mix, not blue\n");
    zexpect_s32_gt(p[2], p[0] + p[1]);
    zprintf("line color lost after flush\n");
    zexpect_s32_gt(128, p[0] + p[1]);
    zprintf("line B should be near zero\n");
    zexpect_s32_lt(64, p[2]);
    free(d);
}

ZTEST(gpu, draw_lines) {
    zskip_targets("pcsx-redux"); // differs from real hardware
    zskip_targets("ppsspp");
    ClearImage(&cdb->draw.clip, 0, 0, 0);
    ClearOTag(cdb->ot, OTSIZE);

    SetLineG2(&cdb->lineg2[0]);
    setXY2(&cdb->lineg2[0], 16, 40, 112, 40);
    setRGB0(&cdb->lineg2[0], 255, 0, 0);
    setRGB1(&cdb->lineg2[0], 0, 255, 0);
    AddPrim(cdb->ot, &cdb->lineg2[0]);

    SetLineG3(&cdb->lineg3[0]);
    setXY3(&cdb->lineg3[0], 16, 80, 64, 120, 112, 80);
    setRGB0(&cdb->lineg3[0], 255, 0, 0);
    setRGB1(&cdb->lineg3[0], 0, 255, 0);
    setRGB2(&cdb->lineg3[0], 0, 0, 255);
    AddPrim(cdb->ot, &cdb->lineg3[0]);

    SetLineG4(&cdb->lineg4[0]);
    setXY4(&cdb->lineg4[0], 16, 150, 16, 200, 112, 200, 112, 150);
    setRGB0(&cdb->lineg4[0], 255, 0, 0);
    setRGB1(&cdb->lineg4[0], 0, 255, 0);
    setRGB2(&cdb->lineg4[0], 0, 0, 255);
    setRGB3(&cdb->lineg4[0], 255, 255, 0);
    AddPrim(cdb->ot, &cdb->lineg4[0]);

    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    // PSP has its own golden: native line geometry has a different
    // rasterization algorithm, and using triangles for lines is too expensive
    // for the GE.
    ASSERT_FRAME("draw_lines", 1, 0.9993f);
}

ZTEST(gpu, set_draw_area) {
    u_short tpage, clut;
    DR_AREA drArea;
    RECT area = {4, 8, 56, 48};
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }

    SetPolyFT4(&cdb->ft4[0]);
    AddPrim(cdb->ot, &cdb->ft4[0]);
    setXYWH(&cdb->ft4[0], 0, 0, 64, 64);
    setRGB0(&cdb->ft4[0], 128, 128, 128);
    setUVWH(&cdb->ft4[0], 0, 0, 64, 64);
    setSemiTrans(&cdb->ft4[0], 0);
    cdb->ft4[0].tpage = tpage;
    cdb->ft4[0].clut = clut;

    AddPrim(cdb->ot, &drArea);
    SetDrawArea(&drArea, &area);

    setRECT(&cdb->draw.clip, 32, 24, 160, 128);
    ClearImage(&cdb->draw.clip, 60, 120, 120);
    setRECT(&cdb->draw.clip, 128, 128, 64, 64);
    ClearImage(&cdb->draw.clip, 120, 120, 60);

    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME("set_draw_area", 0, 1.0f);
}

ZTEST(gpu, swap_buffer) {
    u_short tpage, clut;
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }

    for (int fbidx = 0; fbidx < 2; fbidx++) {
        cdb = &db[fbidx & 1];
        cdb->draw.isbg = 1;
        cdb->draw.tpage = tpage;
        setRGB0(&cdb->draw, 60, 120, 120);
        PutDrawEnv(&cdb->draw);
        PutDispEnv(&cdb->disp);

        SetSprt(cdb->sprt);
        SetSemiTrans(cdb->sprt, 0);
        SetShadeTex(cdb->sprt, 1);
        setXY0(cdb->sprt, 0, fbidx * 16);
        setWH(cdb->sprt, 64, 64);
        setUV0(cdb->sprt, 0, 0);
        cdb->sprt[0].clut = clut;
        ClearOTag(cdb->ot, OTSIZE);
        AddPrim(cdb->ot, cdb->sprt);
        DrawOTag(cdb->ot);

        DrawSync(0);
        VSync(0);
        ASSERT_FRAME(
            (fbidx & 1) ? "swap_buffer_fb2" : "swap_buffer_fb1", 0, 1.0f);
    }
}

ZTEST(gpu, drawenv_clear_vram) {
    const char* ci = getenv("CI");
    const char* os = getenv("OS");
    u_short tpage, clut;
    DRAWENV drawEnv;
    if (ci && strcmp(ci, "1") == 0 && os && strcmp(os, "linux") == 0) {
        zskip("Skipped on Linux CI");
    }
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }

    memset(&drawEnv, 0, sizeof(drawEnv));
    drawEnv.clip.x = 964;
    drawEnv.clip.y = 16;
    drawEnv.clip.w = 8;
    drawEnv.clip.h = 32;
    drawEnv.ofs[0] = drawEnv.clip.x;
    drawEnv.ofs[1] = drawEnv.clip.y;
    drawEnv.r0 = 255;
    drawEnv.g0 = drawEnv.b0 = 0;
    drawEnv.isbg = 1;
    PutDrawEnv(&drawEnv);

    cdb->draw.tpage = tpage;
    PutDrawEnv(&cdb->draw);

    SetPolyFT4(&cdb->ft4[0]);
    setXYWH(&cdb->ft4[0], 16, 16, 64, 64);
    setRGB0(&cdb->ft4[0], 128, 128, 128);
    setUVWH(&cdb->ft4[0], 0, 0, 64, 64);
    setSemiTrans(&cdb->ft4[0], 0);
    cdb->ft4[0].tpage = tpage;
    cdb->ft4[0].clut = clut;

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].ft4[0]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    if (ztest_is_target("psp")) {
        ASSERT_FRAME("drawenv_clear_vram", 2, 0.995f);
    } else {
        ASSERT_FRAME("drawenv_clear_vram", 0, 1.0f);
    }
}

ZTEST(gpu, move_image) {
    u_short tpage, clut;
    RECT rect = {16, 16, 64, 64};
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }

    SetPolyFT4(&cdb->ft4[0]);
    setXYWH(&cdb->ft4[0], 16, 16, 64, 64);
    setRGB0(&cdb->ft4[0], 128, 128, 128);
    setUVWH(&cdb->ft4[0], 0, 0, 64, 64);
    setSemiTrans(&cdb->ft4[0], 0);
    cdb->ft4[0].tpage = tpage;
    cdb->ft4[0].clut = clut;

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].ft4[0]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);

    MoveImage(&rect, 144, 144);
    DrawSync(0);

    VSync(0);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME("move_image", 0, 1.0f);
}

ZTEST(gpu, move_image_overlap) {
    u_short tpage, clut;
    RECT rect = {960, 0, 16, 64};
    zskip_targets("psp");
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }

    MoveImage(&rect, 962, 8);
    rect.x = 962;
    rect.y = 8;
    MoveImage(&rect, 960, 0);

    SetPolyFT4(&cdb->ft4[0]);
    setXYWH(&cdb->ft4[0], 16, 16, 64, 64);
    setRGB0(&cdb->ft4[0], 128, 128, 128);
    setUVWH(&cdb->ft4[0], 0, 0, 64, 64);
    setSemiTrans(&cdb->ft4[0], 0);
    cdb->ft4[0].tpage = tpage;
    cdb->ft4[0].clut = clut;

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].ft4[0]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME("move_image_overlap", 0, 1.0f);
}

ZTEST(gpu, move_image_internal_res) {
    u_short tpage, clut;
    RECT rect = {16, 16, 64, 64};
    u_short pattern[16 * 16];
    u_short readback[16 * 16];
    RECT rectStore = {704, 320, 16, 16};
    zskip_targets("psp;ps1");
    zassert_s32_eq(-1, Psyz_VideoSetInternalResolution(0));
    zassert_s32_eq(0, Psyz_VideoSetInternalResolution(2));
    zassert_s32_eq(2, Psyz_VideoGetInternalResolution());

    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }

    SetPolyFT4(&cdb->ft4[0]);
    setXYWH(&cdb->ft4[0], 16, 16, 64, 64);
    setRGB0(&cdb->ft4[0], 128, 128, 128);
    setUVWH(&cdb->ft4[0], 0, 0, 64, 64);
    setSemiTrans(&cdb->ft4[0], 0);
    cdb->ft4[0].tpage = tpage;
    cdb->ft4[0].clut = clut;

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].ft4[0]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);

    MoveImage(&rect, 144, 144);
    DrawSync(0);

    VSync(0);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME("move_image", 0, 1.0f);

    for (int i = 0; i < 16 * 16; i++) {
        pattern[i] = (u_short)(i * 0x1235);
    }
    LoadImage(&rectStore, (u_long*)pattern);
    DrawSync(0);
    StoreImage(&rectStore, (u_long*)readback);
    DrawSync(0);
    zexpect_u8array_eq(pattern, readback, sizeof(pattern));

    zassert_s32_eq(0, Psyz_VideoSetInternalResolution(1));
    zassert_s32_eq(1, Psyz_VideoGetInternalResolution());
    VSync(0);
    ASSERT_FRAME("move_image", 0, 1.0f);
}

ZTEST(gpu, disp_mask_preserves_vram) {
    u_short pattern[16 * 16];
    u_short readback[16 * 16];
    RECT rect = {704, 320, 16, 16};
    for (int i = 0; i < 16 * 16; i++) {
        pattern[i] = (u_short)(i * 0x1235);
    }
    LoadImage(&rect, (u_long*)pattern);
    DrawSync(0);

    SetDispMask(0);
    VSync(0);
    DrawSync(0);
    memset(readback, 0, sizeof(readback));
    StoreImage(&rect, (u_long*)readback);
    DrawSync(0);
    zprintf("SetDispMask(0) must not alter VRAM\n");
    zexpect_u8array_eq(pattern, readback, sizeof(pattern));

    SetDispMask(1);
    VSync(0);
    memset(readback, 0, sizeof(readback));
    StoreImage(&rect, (u_long*)readback);
    DrawSync(0);
    zprintf("VRAM must survive the display being re-enabled\n");
    zexpect_u8array_eq(pattern, readback, sizeof(pattern));
}

ZTEST(gpu, blit) {
    TIM_IMAGE tim;
    RECT rect = {16, 16, 64, 64};
    OpenTIM((u_long*)img_16bpp);
    ReadTIM(&tim);
    LoadImage(&rect, tim.paddr);
    VSync(0);
    ASSERT_FRAME("blit", 0, 1.0f);
}

ZTEST(gpu, draw_disp_env) {
    // Can't draw on the same buffer that is also displayed
    zskip_targets("psp");

    // Set different buffers for draw and disp
    SetDefDrawEnv(&db[0].draw, 0, 0, 256, 240);
    SetDefDispEnv(&db[0].disp, 256, 0, 256, 240);
    SetDefDrawEnv(&db[1].draw, 256, 0, 256, 240);
    SetDefDispEnv(&db[1].disp, 0, 0, 256, 240);

    // Ensure buffer 0 is clear
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);
    ClearImage(&db[0].draw.clip, 0, 0, 0);
    DrawSync(0);
    VSync(0);

    // Ensure buffer 1 is filled with color red
    PutDrawEnv(&db[1].draw);
    PutDispEnv(&db[1].disp);
    ClearImage(&db[1].draw.clip, 0, 0xFF, 0);
    DrawSync(0);
    VSync(0);

    // Back buffer filled with color red, front displays green
    PutDrawEnv(&db[0].draw);
    PutDispEnv(&db[0].disp);
    ClearImage(&db[0].draw.clip, 0xFF, 0, 0);
    DrawSync(0);
    VSync(0);
    ASSERT_FRAME("draw_disp_env_0", 0, 1.0f);

    // Back buffer now becomes front buffer, displays red
    PutDispEnv(&db[1].disp);
    DrawSync(0);
    VSync(0);
    ASSERT_FRAME("draw_disp_env_1", 0, 1.0f);

    // Front buffer is swapped again, display green
    PutDispEnv(&db[0].disp);
    DrawSync(0);
    VSync(0);
    ASSERT_FRAME("draw_disp_env_2", 0, 1.0f);

    // Now back buffer and front buffer are the same, display blue
    PutDispEnv(&db[0].disp);
    PutDrawEnv(&db[1].draw);
    ClearImage(&db[1].draw.clip, 0, 0, 0xFF);
    DrawSync(0);
    VSync(0);
    ASSERT_FRAME("draw_disp_env_3", 0, 1.0f);
}

ZTEST(gpu, clear_screen_draw_offset_bugfix) {
    DRAWENV draw;
    SetTile(&cdb->tile[0]);
    setRGB0(&cdb->tile[0], 255, 0, 0);
    setXY0(&cdb->tile[0], 0, 0);
    setWH(&cdb->tile[0], 128, 128);

    draw = cdb->draw;
    draw.ofs[0] = 128;
    draw.ofs[1] = 128;
    PutDrawEnv(&draw);
    PutDispEnv(&cdb->disp);

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].tile[0]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);

    ASSERT_FRAME("clear_screen_draw_offset_bugfix", 0, 1.0f);
}

// TODO: test is actually failing
ZTEST(gpu, load_move_image_priority) {
    TIM_IMAGE tim;
    RECT rectMoveNull = {16, 16, 64, 64};
    RECT rectBlit = {16, 16, 64, 64};
    RECT rectMoveImage = {16, 16, 64, 64};
    OpenTIM((u_long*)img_16bpp);
    ReadTIM(&tim);

    MoveImage(&rectMoveNull, 16, 80);

    SetTile(&cdb->tile[0]);
    setRGB0(&cdb->tile[0], 255, 0, 0);
    setXY0(&cdb->tile[0], 32, 32);
    setWH(&cdb->tile[0], 32, 32);
    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].tile[0]);

    LoadImage(&rectBlit, tim.paddr);

    DrawOTag(cdb->ot);

    MoveImage(&rectMoveImage, 80, 16);

    DrawSync(0);
    VSync(0);
    cdb->disp.disp.x = 0;
    cdb->disp.disp.y = 0;
    PutDispEnv(&cdb->disp);

    ASSERT_FRAME("load_move_image_priority", 0, 0.9825f);
}

ZTEST(gpu, flipped_xy) {
    u_short tpage, clut;
    if (LoadTim(img_uv_4bpp, &tpage, &clut)) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        setPolyGT4(&cdb->gt4[i]);
        SetSemiTrans(&cdb->gt4[i], 0);
        SetShadeTex(&cdb->gt4[i], 0);
        setUVWH(&cdb->gt4[i], 0, 0, 64, 64);
        cdb->gt4[i].tpage = tpage;
        cdb->gt4[i].clut = clut;
        AddPrim(cdb->ot, &cdb->gt4[i]);
    }
    setRGB0(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setRGB1(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setRGB2(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setRGB3(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setXY4(&cdb->gt4[0], 0, 0, 64, 0, 0, 64, 64, 64);
    setRGB0(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setRGB1(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setRGB2(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setRGB3(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setXY4(&cdb->gt4[1], 128, 0, 64, 0, 128, 64, 64, 64);
    setRGB0(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setRGB1(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setRGB2(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setRGB3(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setXY4(&cdb->gt4[2], 0, 128, 64, 128, 0, 64, 64, 64);
    setRGB0(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setRGB1(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setRGB2(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setRGB3(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setXY4(&cdb->gt4[3], 128, 128, 64, 128, 128, 64, 64, 64);

    ClearImage(&cdb->draw.clip, 0, 0, 0);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    ASSERT_FRAME("flipped_xy", 1, 1.0f);
}

ZTEST(gpu, flipped_uv) {
    u_short tpage, clut;
    if (LoadTim(img_uv_4bpp, &tpage, &clut)) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        setPolyGT4(&cdb->gt4[i]);
        SetSemiTrans(&cdb->gt4[i], 0);
        SetShadeTex(&cdb->gt4[i], 0);
        cdb->gt4[i].tpage = tpage;
        cdb->gt4[i].clut = clut;
        AddPrim(cdb->ot, &cdb->gt4[i]);
    }
    setXYWH(&cdb->gt4[0], 0, 0, 64, 64);
    setRGB0(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setRGB1(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setRGB2(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setRGB3(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setUV4(&cdb->gt4[0], 0, 0, 64, 0, 0, 64, 64, 64);

    setXYWH(&cdb->gt4[1], 64, 0, 64, 64);
    setRGB0(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setRGB1(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setRGB2(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setRGB3(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setUV4(&cdb->gt4[1], 64, 0, 0, 0, 64, 64, 0, 64);

    setXYWH(&cdb->gt4[2], 0, 64, 64, 64);
    setRGB0(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setRGB1(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setRGB2(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setRGB3(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setUV4(&cdb->gt4[2], 0, 64, 64, 64, 0, 0, 64, 0);

    setXYWH(&cdb->gt4[3], 64, 64, 64, 64);
    setRGB0(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setRGB1(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setRGB2(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setRGB3(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setUV4(&cdb->gt4[3], 64, 64, 0, 64, 64, 0, 0, 0);

    ClearImage(&cdb->draw.clip, 0, 0, 0);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    ASSERT_FRAME("flipped_uv", 1, 1.0f);
}

ZTEST(gpu, flipped_xy_uv) {
    u_short tpage, clut;
    if (LoadTim(img_uv_4bpp, &tpage, &clut)) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        setPolyGT4(&cdb->gt4[i]);
        SetSemiTrans(&cdb->gt4[i], 0);
        SetShadeTex(&cdb->gt4[i], 0);
        cdb->gt4[i].tpage = tpage;
        cdb->gt4[i].clut = clut;
        AddPrim(cdb->ot, &cdb->gt4[i]);
    }
    setXY4(&cdb->gt4[0], 0, 0, 64, 0, 0, 64, 64, 64);
    setRGB0(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setRGB1(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setRGB2(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setRGB3(&cdb->gt4[0], 0xFF, 0xFF, 0xFF);
    setUV4(&cdb->gt4[0], 0, 0, 64, 0, 0, 64, 64, 64);

    setXY4(&cdb->gt4[1], 128, 0, 64, 0, 128, 64, 64, 64);
    setRGB0(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setRGB1(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setRGB2(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setRGB3(&cdb->gt4[1], 0xFF, 0x00, 0x00);
    setUV4(&cdb->gt4[1], 64, 0, 0, 0, 64, 64, 0, 64);

    setXY4(&cdb->gt4[2], 0, 128, 64, 128, 0, 64, 64, 64);
    setRGB0(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setRGB1(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setRGB2(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setRGB3(&cdb->gt4[2], 0x00, 0xFF, 0x00);
    setUV4(&cdb->gt4[2], 0, 64, 64, 64, 0, 0, 64, 0);

    setXY4(&cdb->gt4[3], 128, 128, 64, 128, 128, 64, 64, 64);
    setRGB0(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setRGB1(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setRGB2(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setRGB3(&cdb->gt4[3], 0x00, 0x00, 0xFF);
    setUV4(&cdb->gt4[3], 64, 64, 0, 64, 64, 0, 0, 0);

    ClearImage(&cdb->draw.clip, 0, 0, 0);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    ASSERT_FRAME("flipped_xy_uv", 1, 1.0f);
}

ZTEST(gpu, alpha_blend) {
    zskip_targets("pcsx-redux"); // differs from real hardware
    u_short tpage, clut;
    TIM_IMAGE tim;
    u_short* pal;
    if (OpenTIM((u_long*)img_4bpp)) {
        return;
    }
    if (!ReadTIM(&tim)) {
        return;
    }
    pal = (u_short*)tim.caddr;
    for (int i = 0; i < tim.crect->w * tim.crect->h; i++) {
        if (i == 2) // skip key color index
            continue;
        pal[i] |= 0x8000;
    }
    LoadImage(tim.prect, tim.paddr);
    LoadImage(tim.crect, tim.caddr);
    tpage = GetTPage((int)tim.mode, 0, tim.prect->x, tim.prect->y);
    clut = GetClut(tim.crect->x, tim.crect->y);

    SetPolyF4Img(&db[0].ft4[0], 8, 8, 64, 64, 0, 0, tpage, clut, 0);
    SetPolyF4Img(&db[0].ft4[1], 88, 8, 64, 64, 0, 0, tpage | 0x20, clut, 0);
    SetPolyF4Img(&db[0].ft4[2], 168, 8, 64, 64, 0, 0, tpage | 0x40, clut, 0);
    SetPolyF4Img(&db[0].ft4[3], 8, 88, 64, 64, 0, 0, tpage | 0x60, clut, 1);
    SetPolyF4Img(&db[0].ft4[4], 88, 88, 64, 64, 0, 0, tpage, clut, 1);
    SetPolyF4Img(&db[0].ft4[5], 168, 88, 64, 64, 0, 0, tpage | 0x20, clut, 1);
    SetPolyF4Img(&db[0].ft4[6], 8, 168, 64, 64, 0, 0, tpage | 0x40, clut, 1);

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].ft4[0]);
    AddPrim(cdb->ot, &db[0].ft4[1]);
    AddPrim(cdb->ot, &db[0].ft4[2]);
    AddPrim(cdb->ot, &db[0].ft4[3]);
    AddPrim(cdb->ot, &db[0].ft4[4]);
    AddPrim(cdb->ot, &db[0].ft4[5]);
    AddPrim(cdb->ot, &db[0].ft4[6]);

    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    ASSERT_FRAME("alpha_blend", 1, 1.0f);
}

ZTEST(gpu, s11_coord_truncation) {
    ClearImage(&cdb->draw.clip, 60, 120, 120);
    DrawSync(0);

    SetTile(&cdb->tile[0]);
    setRGB0(&cdb->tile[0], 255, 0, 0);
    setXY0(&cdb->tile[0], (short)0x8014, (short)0x8014);
    setWH(&cdb->tile[0], 32, 32);

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &cdb->tile[0]);

    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    ASSERT_FRAME("s11_coord_truncation", 0, 1.0f);
}

ZTEST(gpu, untextured_transp_poly_take_abr_from_drawenv) {
    const int16_t bx[4] = {8, 88, 8, 88};
    const int16_t by[4] = {8, 8, 88, 88};
    ClearImage(&cdb->draw.clip, 0x60, 0x60, 0x60);
    DrawSync(0);

    for (int i = 0; i < 4; i++) {
        POLY_F4 poly;
        unsigned* words;
        Psyz_GpuWriteGP0(_get_mode(1, 0, getTPage(0, i, 0, 0)));

        SetPolyF4(&poly);
        SetSemiTrans(&poly, 1);
        setRGB0(&poly, 0x80, 0x40, 0xC0);
        setXYWH(&poly, bx[i], by[i], 64, 64);

        words = (unsigned*)&poly + sizeof(OT_TYPE) / sizeof(unsigned);
        Psyz_GpuWriteGP0(*words++);
        Psyz_GpuWriteGP0(*words++);
        Psyz_GpuWriteGP0(*words++);
        Psyz_GpuWriteGP0(*words++);
        Psyz_GpuWriteGP0(*words++);
    }
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    ASSERT_FRAME("abr_untextured", 0, 1.0f);
}

ZTEST(gpu, uv_minification) {
    u_short tpage, clut;
    static const struct {
        short x, y, w, h;
    } cases[] = {// 64 texels into 32 pixels: 2x minified
                 {8, 8, 32, 32},
                 // 64 texels into 16 pixels: 4x minified
                 {88, 8, 16, 16},
                 // non-power-of-two step
                 {8, 88, 21, 21},
                 // 1:1 for reference
                 {88, 88, 64, 64}};
    zskip_targets("ppsspp");
    if (LoadTim(img_uv_4bpp, &tpage, &clut)) {
        return;
    }

    ClearOTag(cdb->ot, OTSIZE);
    for (int i = 0; i < LEN(cases); i++) {
        SetPolyFT4(&cdb->ft4[i]);
        setXYWH(&cdb->ft4[i], cases[i].x, cases[i].y, cases[i].w, cases[i].h);
        setRGB0(&cdb->ft4[i], 128, 128, 128);
        setUVWH(&cdb->ft4[i], 0, 0, 64, 64);
        setSemiTrans(&cdb->ft4[i], 0);
        cdb->ft4[i].tpage = tpage;
        cdb->ft4[i].clut = clut;
        AddPrim(cdb->ot, &cdb->ft4[i]);
    }

    ClearImage(&cdb->draw.clip, 0, 0, 0);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    // PSP has its own golden: a slightly different minification algorithm
    ASSERT_FRAME("uv_minification", 0, 0.9995f);
}

ZTEST(gpu, texture_window_tiling) {
    u_short tpage, clut;
    static const struct {
        short x, y;
        RECT win;
    } cases[] = {
        // full page: window disabled
        {8, 8, {0, 0, 256, 256}},
        // window matches the quad: still one copy
        {88, 8, {0, 0, 64, 64}},
        // 32x32 window: 2x2 repeats
        {8, 88, {0, 0, 32, 32}},
        // 16x16 window: 4x4 repeats
        {88, 88, {0, 0, 16, 16}},
    };
    zskip_targets("psp");
    if (LoadTim(img_uv_4bpp, &tpage, &clut)) {
        return;
    }

    ClearOTag(cdb->ot, OTSIZE);
    for (int i = 0; i < LEN(cases); i++) {
        RECT win;
        SetPolyFT4(&cdb->ft4[i]);
        setXYWH(&cdb->ft4[i], cases[i].x, cases[i].y, 64, 64);
        setRGB0(&cdb->ft4[i], 128, 128, 128);
        setUVWH(&cdb->ft4[i], 0, 0, 64, 64);
        setSemiTrans(&cdb->ft4[i], 0);
        cdb->ft4[i].tpage = tpage;
        cdb->ft4[i].clut = clut;

        win = cases[i].win;
        SetTexWindow(&cdb->twin[i], &win);

        AddPrim(cdb->ot, &cdb->ft4[i]);
        AddPrim(cdb->ot, &cdb->twin[i]);
    }

    ClearImage(&cdb->draw.clip, 0, 0, 0);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    ASSERT_FRAME("texture_window_tiling", 0, 1.0f);
}

ZTEST(gpu, texture_window_offset) {
    // Same 32x32 window size in every quadrant, moved around the page. The
    // offset bits replace the masked-off UV bits, so each quadrant tiles a
    // different 32x32 patch of the texture.
    u_short tpage, clut;
    static const struct {
        short x, y;
        RECT win;
    } cases[] = {
        {8, 8, {0, 0, 32, 32}},
        {88, 8, {32, 0, 32, 32}},
        {8, 88, {0, 32, 32, 32}},
        {88, 88, {32, 32, 32, 32}},
    };
    zskip_targets("psp");
    if (LoadTim(img_uv_4bpp, &tpage, &clut)) {
        return;
    }

    ClearOTag(cdb->ot, OTSIZE);
    for (int i = 0; i < LEN(cases); i++) {
        RECT win;
        SetPolyFT4(&cdb->ft4[i]);
        setXYWH(&cdb->ft4[i], cases[i].x, cases[i].y, 64, 64);
        setRGB0(&cdb->ft4[i], 128, 128, 128);
        setUVWH(&cdb->ft4[i], 0, 0, 64, 64);
        setSemiTrans(&cdb->ft4[i], 0);
        cdb->ft4[i].tpage = tpage;
        cdb->ft4[i].clut = clut;

        win = cases[i].win;
        SetTexWindow(&cdb->twin[i], &win);

        AddPrim(cdb->ot, &cdb->ft4[i]);
        AddPrim(cdb->ot, &cdb->twin[i]);
    }

    ClearImage(&cdb->draw.clip, 0, 0, 0);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    ASSERT_FRAME("texture_window_offset", 0, 1.0f);
}

ZTEST(gpu, texture_window_non_square) {
    u_short tpage, clut;
    static const struct {
        short x, y;
        u_char u, v;
        RECT win;
    } cases[] = {
        // wide and short
        {8, 8, 0, 0, {0, 0, 64, 16}},
        // narrow and tall
        {88, 8, 0, 0, {0, 0, 16, 64}},
        // non-zero starting UV
        {8, 88, 96, 96, {0, 0, 32, 32}},
        // window offset not aligned to window size
        {88, 88, 0, 0, {8, 8, 32, 32}},
    };
    zskip_targets("psp");
    if (LoadTim(img_uv_4bpp, &tpage, &clut)) {
        return;
    }

    ClearOTag(cdb->ot, OTSIZE);
    for (int i = 0; i < LEN(cases); i++) {
        RECT win;
        SetPolyFT4(&cdb->ft4[i]);
        setXYWH(&cdb->ft4[i], cases[i].x, cases[i].y, 64, 64);
        setRGB0(&cdb->ft4[i], 128, 128, 128);
        setUVWH(&cdb->ft4[i], cases[i].u, cases[i].v, 64, 64);
        setSemiTrans(&cdb->ft4[i], 0);
        cdb->ft4[i].tpage = tpage;
        cdb->ft4[i].clut = clut;

        win = cases[i].win;
        SetTexWindow(&cdb->twin[i], &win);

        AddPrim(cdb->ot, &cdb->ft4[i]);
        AddPrim(cdb->ot, &cdb->twin[i]);
    }

    ClearImage(&cdb->draw.clip, 0, 0, 0);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    ASSERT_FRAME("texture_window_non_square", 0, 1.0f);
}

ZTEST(gpu, marge_prim) {
    SetPolyF4(&cdb->f4[0]);
    setXYWH(&cdb->f4[0], 16, 16, 64, 64);
    setRGB0(&cdb->f4[0], 0, 0, 255);
    setSemiTrans(&cdb->f4[0], 0);

    SetPolyF4(&cdb->f4[1]);
    setXYWH(&cdb->f4[1], 32, 32, 32, 32);
    setRGB0(&cdb->f4[1], 0, 255, 0);
    setSemiTrans(&cdb->f4[1], 0);

    MargePrim(&cdb->f4[0], &cdb->f4[1]);

    SetPolyF4(&cdb->f4[2]);
    setXYWH(&cdb->f4[2], 40, 40, 16, 16);
    setRGB0(&cdb->f4[2], 255, 0, 0);
    setSemiTrans(&cdb->f4[2], 0);

    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &cdb->f4[2]);
    AddPrim(cdb->ot, &cdb->f4[0]);
    Present("marge_prim");
}

enum {
    DR = 47,
    DG = 123,
    DB = 239,
    DBG = 91,
    TEX_X = 512,
    TEX_Y = 256,
    TEX_SIZE = 64,
};

static u_short MakeFlatTPage(void) {
    RECT tex = {TEX_X, TEX_Y, TEX_SIZE, TEX_SIZE};
    ClearImage(&tex, 255, 255, 255);
    DrawSync(0);
    return GetTPage(2, 0, TEX_X, TEX_Y);
}

static u_short MakeFlatTPageSemiTrans(void) {
    static u_short texels[TEX_SIZE * TEX_SIZE];
    RECT tex = {TEX_X, TEX_Y, TEX_SIZE, TEX_SIZE};
    for (int i = 0; i < TEX_SIZE * TEX_SIZE; i++) {
        texels[i] = 0xFFFF; // STP | 31/31/31
    }
    LoadImage(&tex, (u_long*)texels);
    DrawSync(0);
    return GetTPage(2, 0, TEX_X, TEX_Y);
}

ZTEST_SETUP(dither) {
    gpu_setup();
    Psyz_VideoSetDitheringMode(PSYZ_DITHER_AUTO);
    cdb->draw.dtd = 1;
    PutDrawEnv(&cdb->draw);
    ClearOTag(cdb->ot, OTSIZE);
    ClearImage(&cdb->draw.clip, 0, 0, 0);
}

ZTEST_TEARDOWN(dither) { ResetGraph(0); }

ZTEST(dither, dithering_drawenv_disabled) {
    cdb->draw.dtd = 0;
    PutDrawEnv(&cdb->draw);

    SetPolyG4(&cdb->g4[0]);
    setXYWH(&cdb->g4[0], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    setRGB0(&cdb->g4[0], DR, DG, DB);
    setRGB1(&cdb->g4[0], DR, DG, DB);
    setRGB2(&cdb->g4[0], DR, DG, DB);
    setRGB3(&cdb->g4[0], DR, DG, DB);
    AddPrim(cdb->ot, &cdb->g4[0]);
    Present("dithering_drawenv_disabled");
}

ZTEST(dither, dithering_gouraud_on) {
    SetPolyG4(&cdb->g4[0]);
    setXYWH(&cdb->g4[0], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    setRGB0(&cdb->g4[0], DR, DG, DB);
    setRGB1(&cdb->g4[0], DR, DG, DB);
    setRGB2(&cdb->g4[0], DR, DG, DB);
    setRGB3(&cdb->g4[0], DR, DG, DB);
    AddPrim(cdb->ot, &cdb->g4[0]);
    Present("dithering_gouraud_on");
}

ZTEST(dither, dithering_flat_off) {
    SetPolyF4(&cdb->f4[0]);
    setXYWH(&cdb->f4[0], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    setRGB0(&cdb->f4[0], DR, DG, DB);
    AddPrim(cdb->ot, &cdb->f4[0]);
    Present("dithering_flat_off");
}

ZTEST(dither, dithering_flat_blending_on) {
    zskip_targets("pcsx-redux"); // differs from real hardware
    // PSP has its own golden: the dithering matrix is fixed in the GPU
    // pipeline, it can't be changed to reflect the exact identical look on PS1.
    u_short tpage = MakeFlatTPage();
    SetPolyFT4(&cdb->ft4[0]);
    setXYWH(&cdb->ft4[0], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    setRGB0(&cdb->ft4[0], DR, DG, DB);
    setUVWH(&cdb->ft4[0], 0, 0, TEX_SIZE - 1, TEX_SIZE - 1);
    setSemiTrans(&cdb->ft4[0], 0);
    cdb->ft4[0].tpage = tpage;
    cdb->ft4[0].clut = 0;
    AddPrim(cdb->ot, &cdb->ft4[0]);
    Present("dithering_flat_blending_on");
}

ZTEST(dither, dithering_lines_on) {
    zskip_targets("pcsx-redux"); // differs from real hardware
    static LINE_F2 flat[SCREEN_HEIGHT / 2];
    static LINE_G2 grad[SCREEN_HEIGHT / 2];
    int nf = 0, ng = 0;

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        if ((y / 8) & 1) {
            LINE_G2* l = &grad[ng++];
            SetLineG2(l);
            setXY2(l, 0, y, SCREEN_WIDTH, y);
            setRGB0(l, DR, DG, DB);
            setRGB1(l, DR, DG, DB);
            AddPrim(cdb->ot, l);
        } else {
            LINE_F2* l = &flat[nf++];
            SetLineF2(l);
            setXY2(l, 0, y, SCREEN_WIDTH, y);
            setRGB0(l, DR, DG, DB);
            AddPrim(cdb->ot, l);
        }
    }
    Present("dithering_lines_on");
}

ZTEST(dither, dithering_tile_off) {
    SetTile(&cdb->tile[0]);
    setXY0(&cdb->tile[0], 0, 0);
    setWH(&cdb->tile[0], SCREEN_WIDTH, SCREEN_HEIGHT);
    setRGB0(&cdb->tile[0], DR, DG, DB);
    AddPrim(cdb->ot, &cdb->tile[0]);
    Present("dithering_tile_off");
}

ZTEST(dither, dithering_tile_blending_off) {
    RECT bg = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    ClearImage(&bg, DBG, DBG, DBG);
    DrawSync(0);

    SetTile(&cdb->tile[0]);
    SetSemiTrans(&cdb->tile[0], 1);
    setXY0(&cdb->tile[0], 0, 0);
    setWH(&cdb->tile[0], SCREEN_WIDTH, SCREEN_HEIGHT);
    setRGB0(&cdb->tile[0], DR, DG, DB);
    AddPrim(cdb->ot, &cdb->tile[0]);

    SetDrawMode(&cdb->drmode[0], 0, 1, (int)getTPage(0, 0, 0, 0), NULL);
    AddPrim(cdb->ot, &cdb->drmode[0]);

    Present("dithering_tile_blending_off");
}

ZTEST(dither, dithering_sprite_off) {
    enum {
        COLS = (SCREEN_WIDTH + TEX_SIZE - 1) / TEX_SIZE,
        ROWS = (SCREEN_HEIGHT + TEX_SIZE - 1) / TEX_SIZE,
    };
    SPRT spr[COLS * ROWS];
    cdb->draw.tpage = MakeFlatTPage();
    PutDrawEnv(&cdb->draw);

    for (int i = COLS * ROWS - 1; i >= 0; i--) {
        SPRT* s = &spr[i];
        SetSprt(s);
        SetSemiTrans(s, 0);
        SetShadeTex(s, 0); // modulated
        setXY0(s, (i % COLS) * TEX_SIZE, (i / COLS) * TEX_SIZE);
        setWH(s, TEX_SIZE, TEX_SIZE);
        setUV0(s, 0, 0);
        setRGB0(s, DR, DG, DB);
        s->clut = 0;
        AddPrim(cdb->ot, s);
    }
    Present("dithering_sprite_off");
}

ZTEST(dither, dithering_sprite_blending_off) {
    zskip_targets("pcsx-redux"); // differs from real hardware
    enum {
        COLS = (SCREEN_WIDTH + TEX_SIZE - 1) / TEX_SIZE,
        ROWS = (SCREEN_HEIGHT + TEX_SIZE - 1) / TEX_SIZE,
    };
    RECT bg = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SPRT spr[COLS * ROWS];
    cdb->draw.tpage = MakeFlatTPageSemiTrans() | (0 << 5);
    PutDrawEnv(&cdb->draw);

    ClearImage(&bg, DBG, DBG, DBG);
    DrawSync(0);

    for (int i = COLS * ROWS - 1; i >= 0; i--) {
        SPRT* s = &spr[i];
        SetSprt(s);
        SetSemiTrans(s, 1); // blended
        SetShadeTex(s, 0);  // modulated
        setXY0(s, (i % COLS) * TEX_SIZE, (i / COLS) * TEX_SIZE);
        setWH(s, TEX_SIZE, TEX_SIZE);
        setUV0(s, 0, 0);
        setRGB0(s, DR, DG, DB);
        s->clut = 0;
        AddPrim(cdb->ot, s);
    }
    Present("dithering_sprite_blending_off");
}

ZTEST(dither, dithering_pattern_alignment) {
    enum {
        SIZE = 5, // 5x5 tile
        BAND_H = 60,
        GRID_COLS = (SCREEN_WIDTH + SIZE - 1) / SIZE,
        GRID_ROWS = (BAND_H + SIZE - 1) / SIZE,
    };
    // static: primitive arrays this size do not fit on the PS1 stack
    static POLY_G4 grid[GRID_COLS * GRID_ROWS];

    SetPolyG4(&cdb->g4[0]);
    setXYWH(&cdb->g4[0], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    setRGB0(&cdb->g4[0], DR, DG, DB);
    setRGB1(&cdb->g4[0], DR, DG, DB);
    setRGB2(&cdb->g4[0], DR, DG, DB);
    setRGB3(&cdb->g4[0], DR, DG, DB);

    for (int i = GRID_COLS * GRID_ROWS - 1; i >= 0; i--) {
        const int gx = (i % GRID_COLS) * SIZE;
        const int gy = (i / GRID_COLS) * SIZE;
        // clip the last row/column so the grid stays inside the band
        const int gw = (gx + SIZE > SCREEN_WIDTH) ? SCREEN_WIDTH - gx : SIZE;
        const int gh = (gy + SIZE > BAND_H) ? BAND_H - gy : SIZE;
        SetPolyG4(&grid[i]);
        setXYWH(&grid[i], gx, gy, gw, gh);
        setRGB0(&grid[i], DR, DG, DB);
        setRGB1(&grid[i], DR, DG, DB);
        setRGB2(&grid[i], DR, DG, DB);
        setRGB3(&grid[i], DR, DG, DB);
        AddPrim(cdb->ot, &grid[i]);
    }
    AddPrim(cdb->ot, &cdb->g4[0]);
    Present("dithering_pattern_alignment");
}

enum {
    GRID_OPCODE = 0x04,
    DISP_W = 320,
    DISP_H = 240,
    PSX_W = 256,
    PACK_W = 40,
    CELL = 8,
    BAND_H = 16,
    NATIVE_Y = 48,
    PSX_Y = 96,
    GUIDE_Y0 = 40,
    GUIDE_Y1 = 120,
};

typedef struct {
    O_TAG;
    u_long code[1];
} DR_GRID;

static TILE tiles[96];
static DR_GRID grids[8];
static int tile_count;
static int grid_count;
static void* tail;
static int handler_calls;

static int HandleGrid(const u_long* words, int available, void* userdata) {
    (void)userdata;
    if (available < 1) {
        return 0;
    }
    handler_calls++;
    if (Psyz_GpuSetHorizontalGrid((unsigned int)(words[0] & 0xFFFF), DISP_W) <
        0) {
        return 0;
    }
    return 1;
}

static void Append(void* prim) {
    if (tail) {
        setaddr(tail, prim);
    } else {
        setaddr(cdb->ot, prim);
    }
    tail = prim;
    termPrim(prim);
}

static void AddGrid(int source_width) {
    DR_GRID* grid = &grids[grid_count++];
    setlen(grid, 1);
    grid->code[0] =
        ((u_long)GRID_OPCODE << 24) | ((u_long)source_width & 0xFFFF);
    Append(grid);
}

static void AddRect(int x, int y, int w, int h, int r, int g, int b) {
    TILE* tile = &tiles[tile_count++];
    SetTile(tile);
    setXY0(tile, x, y);
    setWH(tile, w, h);
    setRGB0(tile, r, g, b);
    Append(tile);
}

static void DrawBands(void) {
    static const unsigned char native[5][3] = {
        {0x40, 0x50, 0xE0},
        {0x40, 0xB0, 0xE0},
        {0x40, 0xD0, 0x80},
        {0x80, 0xD0, 0x40},
        {0xD0, 0xC0, 0x40}};
    static const unsigned char psx[4][3] = {
        {0xE0, 0x80, 0x40},
        {0xD0, 0x40, 0x60},
        {0xB0, 0x40, 0xD0},
        {0x70, 0x40, 0xD0}};
    int i;

    AddGrid(DISP_W);
    for (i = 0; i < DISP_W / CELL; i++) {
        const unsigned char* c = native[i % 5];
        AddRect(i * CELL, NATIVE_Y, CELL, BAND_H, c[0], c[1], c[2]);
    }

    AddGrid(PSX_W);
    for (i = 0; i < PSX_W / CELL; i++) {
        const unsigned char* c = psx[i % 4];
        AddRect(i * CELL, PSX_Y, CELL, BAND_H, c[0], c[1], c[2]);
    }

    AddGrid(DISP_W);
    for (i = 0; i < DISP_W; i += PACK_W) {
        AddRect(i, GUIDE_Y0, 1, GUIDE_Y1 - GUIDE_Y0, 255, 255, 255);
    }
}

ZTEST_SETUP(horizontal_grid) {
    zskip_targets("psp;ps1");
    gpu_setup();
    SetDefDrawEnv(&cdb->draw, 0, 0, DISP_W, DISP_H);
    SetDefDispEnv(&cdb->disp, 0, 0, DISP_W, DISP_H);
    cdb->draw.dtd = 0;
    PutDrawEnv(&cdb->draw);
    PutDispEnv(&cdb->disp);
    ClearOTag(cdb->ot, OTSIZE);
    ClearImage(&cdb->draw.clip, 0, 0, 0);
    DrawSync(0);
    tile_count = 0;
    grid_count = 0;
    tail = NULL;
    handler_calls = 0;
    zassert_s32_eq(
        0, Psyz_GpuRegisterCommandHandler(GRID_OPCODE, HandleGrid, NULL));
}

ZTEST_TEARDOWN(horizontal_grid) {
    Psyz_GpuRegisterCommandHandler(GRID_OPCODE, NULL, NULL);
    Psyz_GpuSetHorizontalGrid(1, 1);
    Psyz_VideoSetInternalResolution(1);
    gpu_teardown();
}

ZTEST(horizontal_grid, horizontal_grid) {
    DrawBands();
    Present("horizontal_grid");
    zexpect_s32_eq(3, handler_calls);
}

// 2x and 4x reproduce the 1x golden
ZTEST(horizontal_grid, horizontal_grid_internal_res_2) {
    zassert_s32_eq(0, Psyz_VideoSetInternalResolution(2));
    DrawBands();
    Present("horizontal_grid");
}

ZTEST(horizontal_grid, horizontal_grid_internal_res_4) {
    zassert_s32_eq(0, Psyz_VideoSetInternalResolution(4));
    DrawBands();
    Present("horizontal_grid");
}

ZTEST(horizontal_grid, horizontal_grid_invalid_args) {
    zexpect_s32_eq(-1, Psyz_GpuSetHorizontalGrid(0, DISP_W));
    zexpect_s32_eq(-1, Psyz_GpuSetHorizontalGrid(DISP_W, 0));
    zexpect_s32_eq(0, Psyz_GpuSetHorizontalGrid(PSX_W, DISP_W));
    zexpect_s32_eq(0, Psyz_GpuSetHorizontalGrid(1, 1));
    zexpect_s32_eq(-1, Psyz_GpuRegisterCommandHandler(0x100, HandleGrid, NULL));
}

static volatile int vsync_callback_count;
static void CountVSync(void) { vsync_callback_count++; }

ZTEST(gpu, vsync_callback_runs_every_frame) {
    vsync_callback_count = 0;
    VSyncCallback(CountVSync);
    VSync(0);
    VSync(0);
    VSync(0);
    VSyncCallback(NULL);
    zexpect_s32_ge(2, vsync_callback_count);
    vsync_callback_count = 0;
    VSync(0);
    zexpect_s32_eq(0, vsync_callback_count);
}

static volatile int vsync_order[16];
static volatile int vsync_order_len;
static void RecordCh0(void) {
    if (vsync_order_len < 16) {
        vsync_order[vsync_order_len++] = 0;
    }
}
static void RecordCh3(void) {
    if (vsync_order_len < 16) {
        vsync_order[vsync_order_len++] = 3;
    }
}
static void RecordCh7(void) {
    if (vsync_order_len < 16) {
        vsync_order[vsync_order_len++] = 7;
    }
}

ZTEST(gpu, vsync_callback_uses_channel_4) {
    zexpect_s32_eq(0, VSyncCallback(CountVSync));
    zexpect_s32_eq((int)(intptr_t)CountVSync, VSyncCallbacks(4, NULL));
    zexpect_s32_eq(0, VSyncCallback(NULL));
}

ZTEST(gpu, vsync_callbacks_returns_previous) {
    zexpect_s32_eq(0, VSyncCallbacks(2, RecordCh0));
    zexpect_s32_eq((int)(intptr_t)RecordCh0, VSyncCallbacks(2, RecordCh3));
    zexpect_s32_eq((int)(intptr_t)RecordCh3, VSyncCallbacks(2, NULL));
    zexpect_s32_eq(0, VSyncCallbacks(2, NULL));
}

ZTEST(gpu, vsync_callbacks_run_in_channel_order) {
    VSyncCallbacks(7, RecordCh7);
    VSyncCallbacks(0, RecordCh0);
    VSyncCallbacks(3, RecordCh3);
    VSync(0);
    vsync_order_len = 0;
    VSync(0);
    VSyncCallbacks(7, NULL);
    VSyncCallbacks(0, NULL);
    VSyncCallbacks(3, NULL);
    zexpect_s32_ge(3, vsync_order_len);
    zexpect_s32_eq(0, vsync_order[0]);
    zexpect_s32_eq(3, vsync_order[1]);
    zexpect_s32_eq(7, vsync_order[2]);
}
