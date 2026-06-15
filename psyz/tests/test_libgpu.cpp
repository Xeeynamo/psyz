#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <GLES2/gl2.h>
extern "C" {
#include <psyz.h>
#include <kernel.h>
#include <libetc.h>
#include <libgpu.h>
void Psyz_SetWindowScale(int scale);
}

#include "res/4bpp.h"
#include "res/16bpp.h"
#include "res/uv4bpp.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_PNG
#define STBI_SUPPORT_ZLIB
#define STBI_MAX_DIMENSIONS 1024
#include "stb_image.h"
#include "stb_image_write.h"

class gpu_Test : public testing::Test {
    static float img_eq(
        const unsigned char* a, const unsigned char* b, const size_t len) {
        size_t matches = 0;
        for (size_t i = 0; i < len; ++i) {
            // normalize both images to RGB5551
            const int l = static_cast<int>(a[i]) & 0xF8;
            const int r = static_cast<int>(b[i]) & 0xF8;
            if (std::abs(l - r) == 0)
                matches++;
        }
        return static_cast<float>(matches) / static_cast<float>(len);
    }

  protected:
    static const int OT_LENGTH = 1;
    static const int OTSIZE = 1 << OT_LENGTH;
    static const int SCREEN_WIDTH = 256;
    static const int SCREEN_HEIGHT = 240;
    typedef struct DB {
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
        DR_MODE drmode[1];
    } DB;
    DB db[2];
    DB* cdb;

    void SetUp() override {
        Psyz_SetWindowScale(1);
        Psyz_VideoSetDitheringMode(PSYZ_DITHER_OFF);
        SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        SetDefDispEnv(&db[0].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        SetDefDrawEnv(
            &db[1].draw, SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        SetDefDispEnv(
            &db[1].disp, SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        SetVideoMode(MODE_NTSC);
        ResetGraph(0);
        PutDrawEnv(&db[0].draw);
        PutDispEnv(&db[0].disp);
        ClearOTagR(db[0].ot, OTSIZE);
        ClearOTagR(db[1].ot, OTSIZE);
        SetDispMask(1);
        RECT clearRect = {0, 0, 0x7FFF, 0x7FFF};
        ClearImage(&clearRect, 0, 0, 0);
        DrawSync(0);
        cdb = &db[0];
    }
    void TearDown() override { ResetGraph(0); }

    static void WriteToFile(const char* filename, void* data, size_t len) {
        FILE* f = fopen(filename, "wb");
        ASSERT_TRUE(f != nullptr);
        fwrite(data, 1, len, f);
        fclose(f);
    }
    static void AssertFrame(const char* png_path, float precision = 1.0f) {
        char filename[FILENAME_MAX];
        char filenameAct[FILENAME_MAX];
        int exp_w, exp_h, act_w, act_h, ch;
        snprintf(filename, sizeof(filename), "../expected/%s.png", png_path);
        unsigned char* exp_d = stbi_load(filename, &exp_w, &exp_h, &ch, 3);
        ch = 3;
        ASSERT_NE(exp_d, nullptr) << "for " << png_path;
        unsigned char* act_d = Psyz_VideoAllocCapturedFrame(&act_w, &act_h);
        ASSERT_NE(act_d, nullptr) << "for " << png_path;
        ASSERT_EQ(exp_w, act_w) << "for " << png_path;
        ASSERT_EQ(exp_h, act_h) << "for " << png_path;
        auto eq = img_eq(exp_d, act_d, exp_w * exp_h * ch);
        snprintf(filenameAct, sizeof(filenameAct), "../expected/%s.actual.png",
                 png_path);
        if (eq < precision) {
            stbi_write_png(filenameAct, act_w, act_h, ch, act_d, act_w * ch);
        } else {
            remove(filenameAct);
        }
        EXPECT_GE(eq, precision) << "for " << png_path;
        stbi_image_free(exp_d);
        free(act_d);
    }

    static int LoadTim(void* data, u_short* outTpage, u_short* outClut) {
        if (OpenTIM((u_long*)data)) {
            return 1;
        }
        TIM_IMAGE tim;
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

    static void SetPolyF4Img(
        POLY_FT4* poly, int x, int y, int w, int h, int u, int v, u_short tpage,
        u_short clut, int semitrans) {
        SetPolyFT4(poly);
        setXYWH(poly, x, y, w, h);
        setRGB0(poly, 255, 128, 128);
        setUVWH(poly, u, v, w, h);
        setSemiTrans(poly, semitrans);
        poly->tpage = tpage;
        poly->clut = clut;
    }
};

TEST_F(gpu_Test, fnt_print) {
    FntLoad(960, 256);
    SetDumpFnt(FntOpen(4, 4, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 512));
    ClearImage(&cdb->draw.clip, 60, 120, 120);
    FntPrint("hello psyz!");
    FntFlush(-1);
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&cdb->draw);
    PutDispEnv(&cdb->disp);
    AssertFrame("fnt_print");
}

TEST_F(gpu_Test, draw_ft4) {
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
    AssertFrame("draw_ft4");
}

TEST_F(gpu_Test, draw_ft4_colored) {
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
    AssertFrame("draw_ft4_colored");
}

TEST_F(gpu_Test, draw_gt4) {
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
    AssertFrame("draw_gt4", 0.9875);
}

// Reproduces SOTN MenuDrawLine: a 1px rectangle border drawn as four
// LINE_G2 (GP0 0x50) segments. Dumps the captured frame so the corners can
// be inspected for endpoint gaps.
static void SetBorderLine(LINE_G2* l, int x0, int y0, int x1, int y1, int c) {
    SetLineG2(l);
    setRGB0(l, c, c, c);
    l->r1 = l->g1 = l->b1 = (u_char)c;
    l->x0 = (short)x0;
    l->y0 = (short)y0;
    l->x1 = (short)x1;
    l->y1 = (short)y1;
}

TEST_F(gpu_Test, gouraud_line_after_flush) {
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

    int w, h;
    unsigned char* d = Psyz_VideoAllocCapturedFrame(&w, &h);
    ASSERT_NE(d, nullptr);

    // Linux and Windows renders the line at y:87, macOS does it at y:88
    const unsigned char* p87 = d + 3 * (87 * w + 48);
    const unsigned char* p88 = d + 3 * (88 * w + 48);

    const unsigned char* p = (p87[0] + p87[1] > p87[2]) ? p87 : p88;
    EXPECT_GT(p[0] + p[1], p[2]) << "line should be red/green mix, not blue";
    EXPECT_GT(p[0] + p[1], 128) << "line color lost after flush";
    EXPECT_LT(p[2], 64) << "line B should be near zero";
    free(d);
}

TEST_F(gpu_Test, draw_lines) {
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
    AssertFrame("draw_lines", 0.9999);
}

TEST_F(gpu_Test, set_draw_area) {
    u_short tpage, clut;
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

    DR_AREA drArea;
    AddPrim(cdb->ot, &drArea);
    RECT area = {4, 8, 56, 48};
    SetDrawArea(&drArea, &area);

    setRECT(&cdb->draw.clip, 32, 24, 160, 128);
    ClearImage(&cdb->draw.clip, 60, 120, 120);
    setRECT(&cdb->draw.clip, 128, 128, 64, 64);
    ClearImage(&cdb->draw.clip, 120, 120, 60);

    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
    AssertFrame("set_draw_area");
}

TEST_F(gpu_Test, swap_buffer) {
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
        AssertFrame((fbidx & 1) ? "swap_buffer_fb2" : "swap_buffer_fb1");
    }
}

TEST_F(gpu_Test, dithering) {
    Psyz_VideoSetDitheringMode(PSYZ_DITHER_AUTO);
    PutDrawEnv(&cdb->draw);
    PutDispEnv(&cdb->disp);

    SetPolyG4(&cdb->g4[0]);
    setXYWH(&cdb->g4[0], 16, 16, 224, 208);
    setRGB0(&cdb->g4[0], 8, 0, 0);
    setRGB1(&cdb->g4[0], 56, 0, 0);
    setRGB2(&cdb->g4[0], 8, 0, 0);
    setRGB3(&cdb->g4[0], 56, 0, 0);
    setSemiTrans(&cdb->g4[0], 0);

    SetDrawMode(&cdb->drmode[0], 0, 1, 0, nullptr);

    ClearImage(&cdb->draw.clip, 0, 0, 0);
    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &cdb->g4[0]);
    AddPrim(cdb->ot, &cdb->drmode[0]);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    AssertFrame("dithering", 0.995);
}

TEST_F(gpu_Test, drawenv_clear_vram) {
    const char* ci = getenv("CI");
    const char* os = getenv("OS");
    if (ci && strcmp(ci, "1") == 0 && os && strcmp(os, "linux") == 0) {
        GTEST_SKIP() << "Skipped on Linux CI";
    }
    u_short tpage, clut;
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }

    DRAWENV drawEnv = {};
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
    AssertFrame("drawenv_clear_vram");
}

// N.B. this test fails on pcsx-redux, I tested output accuracy with Duckstation
TEST_F(gpu_Test, moveimage) {
    u_short tpage, clut;
    if (LoadTim(img_4bpp, &tpage, &clut)) {
        return;
    }

    RECT rect = {960, 0, 16, 64};
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
    AssertFrame("moveimage");
}

TEST_F(gpu_Test, blit) {
    TIM_IMAGE tim;
    RECT rect = {16, 16, 64, 64};
    OpenTIM((u_long*)img_16bpp);
    ReadTIM(&tim);
    LoadImage(&rect, tim.paddr);
    VSync(0);
    AssertFrame("blit");
}

TEST_F(gpu_Test, draw_disp_env) {
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
    AssertFrame("draw_disp_env_0");

    // Back buffer now becomes front buffer, displays red
    PutDispEnv(&db[1].disp);
    DrawSync(0);
    VSync(0);
    AssertFrame("draw_disp_env_1");

    // Front buffer is swapped again, display green
    PutDispEnv(&db[0].disp);
    DrawSync(0);
    VSync(0);
    AssertFrame("draw_disp_env_2");

    // Now back buffer and front buffer are the same, display blue
    PutDispEnv(&db[0].disp);
    PutDrawEnv(&db[1].draw);
    ClearImage(&db[1].draw.clip, 0, 0, 0xFF);
    DrawSync(0);
    VSync(0);
    AssertFrame("draw_disp_env_3");
}

TEST_F(gpu_Test, clear_screen_draw_offset_bugfix) {
    SetTile(&cdb->tile[0]);
    setRGB0(&cdb->tile[0], 255, 0, 0);
    setXY0(&cdb->tile[0], 0, 0);
    setWH(&cdb->tile[0], 128, 128);

    DRAWENV draw = cdb->draw;
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

    AssertFrame("clear_screen_draw_offset_bugfix");
}

// TODO: test is actually failing
TEST_F(gpu_Test, load_move_image_priority) {
    TIM_IMAGE tim;
    OpenTIM((u_long*)img_16bpp);
    ReadTIM(&tim);

    RECT rectMoveNull = {16, 16, 64, 64};
    MoveImage(&rectMoveNull, 16, 80);

    SetTile(&cdb->tile[0]);
    setRGB0(&cdb->tile[0], 255, 0, 0);
    setXY0(&cdb->tile[0], 32, 32);
    setWH(&cdb->tile[0], 32, 32);
    ClearOTag(cdb->ot, OTSIZE);
    AddPrim(cdb->ot, &db[0].tile[0]);

    RECT rectBlit = {16, 16, 64, 64};
    LoadImage(&rectBlit, tim.paddr);

    DrawOTag(cdb->ot);

    RECT rectMoveImage = {16, 16, 64, 64};
    MoveImage(&rectMoveImage, 80, 16);

    DrawSync(0);
    VSync(0);
    cdb->disp.disp.x = 0;
    cdb->disp.disp.y = 0;
    PutDispEnv(&cdb->disp);

    AssertFrame("load_move_image_priority", 0.9825);
}

TEST_F(gpu_Test, flipped_xy) {
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

    AssertFrame("flipped_xy", 1);
}

TEST_F(gpu_Test, flipped_uv) {
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

    AssertFrame("flipped_uv", 1);
}

TEST_F(gpu_Test, flipped_xy_uv) {
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

    AssertFrame("flipped_xy_uv", 1);
}

TEST_F(gpu_Test, alpha_blend) {
    u_short tpage, clut;
    if (OpenTIM((u_long*)img_4bpp)) {
        return;
    }
    TIM_IMAGE tim;
    if (!ReadTIM(&tim)) {
        return;
    }
    u_short* pal = (u_short*)tim.caddr;
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

    AssertFrame("alpha_blend", 0.9585);
}

TEST_F(gpu_Test, s11_coord_truncation) {
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
    AssertFrame("s11_coord_truncation");
}

TEST_F(gpu_Test, untextured_transp_poly_take_abr_from_drawenv) {
    ClearImage(&cdb->draw.clip, 0x60, 0x60, 0x60);
    DrawSync(0);

    const int16_t bx[4] = {8, 88, 8, 88};
    const int16_t by[4] = {8, 8, 88, 88};
    for (int i = 0; i < 4; i++) {
        Psyz_GpuWriteGP0(_get_mode(1, 0, getTPage(0, i, 0, 0)));

        POLY_F4 poly;
        SetPolyF4(&poly);
        SetSemiTrans(&poly, 1);
        setRGB0(&poly, 0x80, 0x40, 0xC0);
        setXYWH(&poly, bx[i], by[i], 64, 64);

        unsigned* words = (unsigned*)&poly + sizeof(OT_TYPE) / sizeof(unsigned);
        Psyz_GpuWriteGP0(*words++);
        Psyz_GpuWriteGP0(*words++);
        Psyz_GpuWriteGP0(*words++);
        Psyz_GpuWriteGP0(*words++);
        Psyz_GpuWriteGP0(*words++);
    }
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);

    AssertFrame("abr_untextured", 0.99);
}
