#include <stdlib.h>
#include <psyz.h>
#include <libetc.h>
#include <libgpu.h>
#include "ztest.h"
#include "../src/video_layout.h"

static const PsyzSize DISPLAY_PSP = {480, 272};
static const PsyzSize DISPLAY_NDS = {256, 192};
static const PsyzSize DISPLAY_3DS = {400, 240};
static const PsyzSize DISPLAY_WIIU_GAMEPAD = {854, 480};
static const PsyzSize DISPLAY_SWITCH = {1280, 720};
static const PsyzSize DISPLAY_SWITCH_DOCKED = {1920, 1080};
static const PsyzSize DISPLAY_STEAM_DECK = {1280, 800};
static const PsyzSize DISPLAY_DESKTOP_WINDOWED = {1280, 960};

#define zexpect_rect_eq(act, SX, SY, EX, EY)                                   \
    zexpect_rect_eq_(__LINE__, act, SX, SY, EX, EY)
static int zexpect_rect_eq_(
    int line, PsyzRect act, int sx, int sy, int ex, int ey) {
    if (act.sx == sx && act.sy == sy && act.ex == ex && act.ey == ey) {
        return 1;
    }
    ztest__fail(line, 0, "Rect mismatch");
    zprintf("  Expected: %4d, %4d, %4d, %4d\n", sx, sy, ex, ey);
    zprintf("    Actual: %4d, %4d, %4d, %4d\n", act.sx, act.sy, act.ex, act.ey);
    return 0;
}

static void SetFramebuffer(int w, int h, int range_w, int range_h) {
    DISPENV disp;
    SetDefDispEnv(&disp, 0, 0, w, h);
    disp.screen.w = range_w;
    disp.screen.h = range_h;
    PutDispEnv(&disp);
}

static void ResetVideo(void) {
    Psyz_VideoSimulateDisplay((PsyzSize){0, 0});
    Psyz_VideoSetResolution(PSYZ_DEFAULT_RESOLUTION);
    Psyz_VideoSetAnchor(PSYZ_ANCHOR_CENTER);
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_DISPLAY);
    SetVideoMode(MODE_NTSC);
    ResetGraph(0);
}

ZTEST_SETUP(video) {
    zskip_targets("psp;ps1"); // they can't simulate a display size
    ResetVideo();
    SetFramebuffer(320, 240, 0, 0);
}

ZTEST_TEARDOWN(video) { ResetVideo(); }

ZTEST(video, default_for_psp) {
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, 0, 320, 240);
}

ZTEST(video, native_res_for_psp) {
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -80, -16, 400, 256);
}

ZTEST(video, native_res_for_nds) {
    Psyz_VideoSimulateDisplay(DISPLAY_NDS);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 32, 24, 288, 216);
}

ZTEST(video, native_res_for_3ds) {
    Psyz_VideoSimulateDisplay(DISPLAY_3DS);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -40, 0, 360, 240);
}

ZTEST(video, native_res_for_wiiu_gamepad) {
    Psyz_VideoSimulateDisplay(DISPLAY_WIIU_GAMEPAD);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -56, 0, 376, 240);
}

ZTEST(video, native_res_for_720) {
    Psyz_VideoSimulateDisplay(DISPLAY_SWITCH);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -56, 0, 376, 240);
}

ZTEST(video, native_res_for_1080) {
    Psyz_VideoSimulateDisplay(DISPLAY_SWITCH_DOCKED);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -80, -16, 400, 256);
}

ZTEST(video, native_res_for_steam_deck) {
    Psyz_VideoSimulateDisplay(DISPLAY_STEAM_DECK);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -56, -16, 376, 256);
}

ZTEST(video, native_res_for_desktop_windowed) {
    Psyz_VideoSimulateDisplay(DISPLAY_DESKTOP_WINDOWED);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, 0, 320, 240);
}

ZTEST(video, for_256x240_games_with_correct_aspect_ratio_on_psp) {
    SetFramebuffer(256, 240, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -64, -16, 320, 256);
}

ZTEST(video, for_256x240_games_with_square_pixels_on_psp) {
    SetFramebuffer(256, 240, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -112, -16, 368, 256);
}

ZTEST(video, for_384x240_games_with_square_pixels_on_psp) {
    SetFramebuffer(384, 240, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -48, -16, 432, 256);
}

ZTEST(video, square_pixels_crop_wide_framebuffer) {
    SetFramebuffer(512, 240, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 16, -16, 496, 256);
}

ZTEST(video, display_aspect_fills_psp_in_every_mode) {
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());

    SetFramebuffer(320, 240, 0, 0);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -80, -16, 400, 256);

    SetFramebuffer(384, 240, 0, 0);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -96, -16, 480, 256);

    SetFramebuffer(512, 240, 0, 0);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -128, -16, 640, 256);

    SetFramebuffer(640, 240, 0, 0);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -160, -16, 800, 256);
}

ZTEST(video, smaller_area_leaves_black_bars) {
    SetFramebuffer(256, 240, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution((PsyzSize){480, 240});
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -64, 0, 320, 240);
}

ZTEST(video, display_aspect_quadruples_rows_on_docked_switch) {
    SetFramebuffer(256, 240, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_SWITCH_DOCKED);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -64, -16, 320, 256);
}

ZTEST(video, small_area_scales_up_on_large_display) {
    SetFramebuffer(256, 240, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_SWITCH_DOCKED);
    Psyz_VideoSetResolution((PsyzSize){320, 240});
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, 0, 256, 240);
}

ZTEST(video, large_area_scales_rows) {
    Psyz_VideoSimulateDisplay(DISPLAY_DESKTOP_WINDOWED);
    Psyz_VideoSetResolution((PsyzSize){640, 480});
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, 0, 320, 240);
}

ZTEST(video, display_aspect_follows_display_ranges) {
    SetFramebuffer(320, 240, 200, 240);
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -148, -16, 468, 256);
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -80, -16, 400, 256);
}

ZTEST(video, display_aspect_ignores_pal) {
    SetVideoMode(MODE_PAL);
    SetFramebuffer(320, 256, 256, 256);
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -80, -16, 400, 256);
}

ZTEST(video, anchors_with_wide_pixels) {
    SetFramebuffer(256, 240, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());

    Psyz_VideoSetAnchor(PSYZ_ANCHOR_CENTER);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -64, -16, 320, 256);

    Psyz_VideoSetAnchor(PSYZ_ANCHOR_LEFT);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, -16, 384, 256);

    Psyz_VideoSetAnchor(PSYZ_ANCHOR_TOP);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -64, 0, 320, 272);

    Psyz_VideoSetAnchor(PSYZ_ANCHOR_TOP_LEFT);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, 0, 384, 272);
}

ZTEST(video, display_policy_caps_canvas_size) {
    SetFramebuffer(640, 480, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_SWITCH);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -192, -16, 832, 496);
}

ZTEST(video, anchors_move_the_framebuffer) {
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize());

    Psyz_VideoSetAnchor(PSYZ_ANCHOR_CENTER);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -80, -16, 400, 256);

    Psyz_VideoSetAnchor(PSYZ_ANCHOR_LEFT);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, -16, 480, 256);

    Psyz_VideoSetAnchor(PSYZ_ANCHOR_TOP);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -80, 0, 400, 272);

    Psyz_VideoSetAnchor(PSYZ_ANCHOR_TOP_LEFT);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, 0, 480, 272);
}

ZTEST(video, ps1_mode_keeps_the_framebuffer) {
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetAnchor(PSYZ_ANCHOR_TOP_LEFT);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, 0, 320, 240);
}

ZTEST(video, fixed_resolution_is_centered_on_display) {
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution((PsyzSize){400, 240});
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -40, 0, 360, 240);
}

ZTEST(video, fixed_resolution_larger_than_display_keeps_the_framebuffer) {
    SetFramebuffer(640, 480, 0, 0);
    Psyz_VideoSimulateDisplay(DISPLAY_PSP);
    Psyz_VideoSetResolution((PsyzSize){640, 480});
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, 0, 640, 480);
}

ZTEST(video, display_size_follows_simulated_display) {
    PsyzSize size;
    zexpect_s32_eq(0, Psyz_VideoSimulateDisplay(DISPLAY_NDS));
    size = Psyz_VideoGetDisplaySize();
    zexpect_s32_eq(DISPLAY_NDS.w, size.w);
    zexpect_s32_eq(DISPLAY_NDS.h, size.h);
    zexpect_s32_eq(-1, Psyz_VideoSimulateDisplay((PsyzSize){-1, 192}));
    zexpect_s32_eq(-1, Psyz_VideoSimulateDisplay((PsyzSize){256, 0}));
}

ZTEST(video, request_validation) {
    zexpect_s32_eq(0, Psyz_VideoSetResolution(PSYZ_DEFAULT_RESOLUTION));
    zexpect_s32_eq(-1, Psyz_VideoSetResolution((PsyzSize){-1, -1}));
    zexpect_s32_eq(-1, Psyz_VideoSetResolution((PsyzSize){0, -1}));
    zexpect_s32_eq(-1, Psyz_VideoSetResolution((PsyzSize){-1, 240}));
    zexpect_s32_eq(-1, Psyz_VideoSetResolution((PsyzSize){320, 0}));
    zexpect_s32_eq(-1, Psyz_VideoSetResolution((PsyzSize){-2, -2}));
    zexpect_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){8192, 8192}));
    zexpect_s32_eq(-1, Psyz_VideoSetResolution((PsyzSize){8193, 240}));
}

ZTEST(video, anchor_validation) {
    zexpect_s32_eq(0, Psyz_VideoSetAnchor(PSYZ_ANCHOR_CENTER));
    zexpect_s32_eq(0, Psyz_VideoSetAnchor(PSYZ_ANCHOR_TOP_LEFT));
    zexpect_s32_eq(PSYZ_ANCHOR_TOP_LEFT, Psyz_VideoGetAnchor());
    zexpect_s32_eq(-1, Psyz_VideoSetAnchor((PsyzAnchor)4));
    zexpect_s32_eq(-1, Psyz_VideoSetAnchor((PsyzAnchor)-1));
    zexpect_s32_eq(PSYZ_ANCHOR_TOP_LEFT, Psyz_VideoGetAnchor());
}

static VideoLayout ResolveDisplay(int disp_w, int disp_h) {
    VideoLayoutIn in = {0};
    VideoLayout out;
    in.display.w = disp_w;
    in.display.h = disp_h;
    in.fb.w = 320;
    in.fb.h = 240;
    in.request.w = disp_w;
    in.request.h = disp_h;
    VideoLayout_Resolve(&in, &out);
    return out;
}

ZTEST(video_layout, map_area_extends_touching_edges) {
    VideoLayout l = ResolveDisplay(480, 272);
    PsyzRect full = {0, 240, 320, 480};
    PsyzRect top = {0, 240, 320, 360};
    PsyzRect vram = {0, 0, 1024, 512};
    zexpect_rect_eq(VideoLayout_MapArea(&l, 0, 240, full, 1), 0, 0, 480, 272);
    zexpect_rect_eq(VideoLayout_MapArea(&l, 0, 240, top, 1), 0, 0, 480, 136);
    zexpect_rect_eq(VideoLayout_MapArea(&l, 0, 240, vram, 1), 0, 0, 480, 272);
}

ZTEST(video_layout, map_area_keeps_inner_edges) {
    VideoLayout l = ResolveDisplay(480, 272);
    PsyzRect inner = {10, 250, 110, 300};
    PsyzRect full = {0, 240, 320, 480};
    zexpect_rect_eq(VideoLayout_MapArea(&l, 0, 240, inner, 1), 90, 26, 190, 76);
    zexpect_rect_eq(VideoLayout_MapArea(&l, 0, 240, full, 0), 80, 16, 400, 256);
}

ZTEST(video_layout, map_area_ignores_other_regions) {
    VideoLayout l = ResolveDisplay(480, 272);
    PsyzRect right = {320, 240, 384, 304};
    PsyzRect above = {0, 0, 320, 240};
    zexpect_rect_eq(VideoLayout_MapArea(&l, 0, 240, right, 1), 0, 0, 0, 0);
    zexpect_rect_eq(VideoLayout_MapArea(&l, 0, 240, above, 1), 0, 0, 0, 0);
}

ZTEST(video_layout, map_area_crops_small_canvas) {
    VideoLayout l = ResolveDisplay(256, 192);
    PsyzRect full = {0, 0, 320, 240};
    zexpect_rect_eq(VideoLayout_MapArea(&l, 0, 0, full, 1), 0, 0, 256, 192);
    zexpect_rect_eq(VideoLayout_MapArea(&l, 0, 0, full, 0), 0, 0, 256, 192);
}

enum {
    PS1_FB_W = 320, // typical PS1 framebuffer size
    PS1_FB_H = 240, // typical PS1 framebuffer size
    MAX_NUM_TILES = 32,
    OT_LEN = 2,
};

typedef struct {
    DRAWENV draw;
    DISPENV disp;
    OT_TYPE ot[OT_LEN];
    TILE tiles[MAX_NUM_TILES];
    SPRT sprt;
    DR_MODE mode;
    int count;
} Db;

static Db db[2];

static const zimage_cmp* NvCmp(void) {
    static zimage_cmp cmp;
#ifdef __PSP__
    cmp = *zimage_r5g5b5_with_tol_prec(2, 1.0f);
#else
    cmp = *zimage_r5g5b5_with_tol_prec(0, 1.0f);
#endif
    return &cmp;
}

#define NV_EXPECT_RGB(img, x, y, r, g, b)                                      \
    do {                                                                       \
        unsigned char px_[3];                                                  \
        zimage_get_rgb((img), (x), (y), px_);                                  \
        zexpect_s32_lt(24, abs(px_[0] - (r)));                                 \
        zexpect_s32_lt(24, abs(px_[1] - (g)));                                 \
        zexpect_s32_lt(24, abs(px_[2] - (b)));                                 \
    } while (0)

#define ASSERT_FRAME(name) zexpect_image_eq(name, zimage_frontbuffer(), NvCmp())

ZTEST_SETUP(native) {
    RECT vram = {0, 0, 1024, 512};
    int i;
    zskip_targets("ps1");
    Psyz_VideoSetDitheringMode(PSYZ_DITHER_OFF);
    Psyz_VideoSetInternalResolution(1);
    ResetGraph(0);
    for (i = 0; i < 2; i++) {
        SetDefDrawEnv(&db[i].draw, 0, i * PS1_FB_H, PS1_FB_W, PS1_FB_H);
        SetDefDispEnv(&db[i].disp, 0, i * PS1_FB_H, PS1_FB_W, PS1_FB_H);
        db[i].draw.dtd = 0;
        db[i].draw.isbg = 1;
        setRGB0(&db[i].draw, 0, 0, 96);
        ClearOTag(db[i].ot, OT_LEN);
        db[i].count = 0;
    }
    ClearImage(&vram, 0, 0, 0);
    DrawSync(0);
    PutDispEnv(&db[1].disp);
    PutDispEnv(&db[0].disp);
    SetDispMask(1);
}

ZTEST_TEARDOWN(native) {
    Psyz_VideoSetResolution(PSYZ_DEFAULT_RESOLUTION);
    Psyz_VideoSetAnchor(PSYZ_ANCHOR_CENTER);
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_DISPLAY);
    Psyz_VideoSimulateDisplay((PsyzSize){0, 0});
    Psyz_VideoSetInternalResolution(1);
    ResetGraph(0);
}

ZTEST(native, ps1_mode_screen_is_the_framebuffer) {
    PsyzRect s = Psyz_VideoGetDrawArea();
    zexpect_s32_eq(0, s.sx);
    zexpect_s32_eq(0, s.sy);
    zexpect_s32_eq(PS1_FB_W, s.ex);
    zexpect_s32_eq(PS1_FB_H, s.ey);
}

ZTEST(native, invalid_arguments_are_rejected) {
    zexpect_s32_eq(-1, Psyz_VideoSetResolution((PsyzSize){0, 272}));
    zexpect_s32_eq(-1, Psyz_VideoSetResolution((PsyzSize){-1, 240}));
    zexpect_s32_eq(-1, Psyz_VideoSetResolution((PsyzSize){8193, 240}));
    zexpect_s32_eq(-1, Psyz_VideoSetAnchor((PsyzAnchor)4));
    zexpect_s32_eq(PSYZ_ANCHOR_CENTER, Psyz_VideoGetAnchor());
    zexpect_s32_eq(0, Psyz_VideoSetInternalResolution(PSYZ_INTERNAL_RES_AUTO));
    zexpect_s32_eq(1, Psyz_VideoGetInternalResolution());
}

ZTEST(native, resolution_survives_reset_graph) {
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){400, 240}));
    ResetGraph(0);
    zexpect_s32_eq(360, Psyz_VideoGetDrawArea().ex);
    zexpect_s32_eq(-40, Psyz_VideoGetDrawArea().sx);
}

static void AddTile(Db* cdb, int x, int y, int w, int h, int r, int g, int b) {
    zassert_s32_le(MAX_NUM_TILES, cdb->count);
    TILE* t = &cdb->tiles[cdb->count++];
    SetTile(t);
    setXY0(t, x, y);
    setWH(t, w, h);
    setRGB0(t, r, g, b);
    addPrim(cdb->ot, t);
}

static void AddScreenCorners(Db* b) {
    const int SIZE = 16;

    PsyzRect s = Psyz_VideoGetDrawArea();
    int right = s.ex - SIZE;
    int bottom = s.ey - SIZE;

    // The area a 320x240 game would typically draw to
    AddTile(b, PS1_FB_W - 1, 0, 1, PS1_FB_H, 255, 255, 255);
    AddTile(b, 0, 0, 1, PS1_FB_H, 255, 255, 255);
    AddTile(b, 0, PS1_FB_H - 1, PS1_FB_W, 1, 255, 255, 255);
    AddTile(b, 0, 0, PS1_FB_W, 1, 255, 255, 255);

    // Colored squares at the four native screen corners
    AddTile(b, right, bottom, SIZE, SIZE, 255, 255, 0);
    AddTile(b, s.sx, bottom, SIZE, SIZE, 0, 0, 255);
    AddTile(b, right, s.sy, SIZE, SIZE, 0, 255, 0);
    AddTile(b, s.sx, s.sy, SIZE, SIZE, 255, 0, 0);
}

static void PresentDrawBuffer(Db* cdb) {
    PutDrawEnv(&cdb->draw);
    DrawOTag(cdb->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&cdb->disp);
}

ZTEST(native, anchor_center_then_extends_around_framebuffer) {
    PsyzRect s;
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    s = Psyz_VideoGetDrawArea();
    zexpect_s32_eq(-80, s.sx);
    zexpect_s32_eq(-16, s.sy);
    zexpect_s32_eq(400, s.ex);
    zexpect_s32_eq(256, s.ey);
    AddScreenCorners(&db[0]);
    PresentDrawBuffer(&db[0]);
    ASSERT_FRAME("native_anchor_center");
}

ZTEST(native, anchor_left_then_extends_around_framebuffer) {
    PsyzRect s;
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    zassert_s32_eq(0, Psyz_VideoSetAnchor(PSYZ_ANCHOR_LEFT));
    s = Psyz_VideoGetDrawArea();
    zexpect_s32_eq(0, s.sx);
    zexpect_s32_eq(-16, s.sy);
    zexpect_s32_eq(480, s.ex);
    zexpect_s32_eq(256, s.ey);
    AddScreenCorners(&db[0]);
    PresentDrawBuffer(&db[0]);
    ASSERT_FRAME("native_anchor_left");
}

ZTEST(native, anchor_top_then_extends_around_framebuffer) {
    PsyzRect s;
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    zassert_s32_eq(0, Psyz_VideoSetAnchor(PSYZ_ANCHOR_TOP));
    s = Psyz_VideoGetDrawArea();
    zexpect_s32_eq(-80, s.sx);
    zexpect_s32_eq(0, s.sy);
    zexpect_s32_eq(400, s.ex);
    zexpect_s32_eq(272, s.ey);
    AddScreenCorners(&db[0]);
    PresentDrawBuffer(&db[0]);
    ASSERT_FRAME("native_anchor_top");
}

ZTEST(native, anchor_topleft_then_extends_right_and_down) {
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    zassert_s32_eq(0, Psyz_VideoSetAnchor(PSYZ_ANCHOR_TOP_LEFT));
    AddScreenCorners(&db[0]);
    PresentDrawBuffer(&db[0]);
    ASSERT_FRAME("native_anchor_topleft");
}

ZTEST(native, smaller_canvas_keeps_its_corners) {
    zimage img;
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){400, 240}));
    AddScreenCorners(&db[0]);
    PresentDrawBuffer(&db[0]);
    img = zimage_frontbuffer();
    zassert_s32_eq(400, img.width);
    zassert_s32_eq(240, img.height);
    NV_EXPECT_RGB(&img, 0, 0, 255, 0, 0);
    NV_EXPECT_RGB(&img, 399, 0, 0, 255, 0);
    NV_EXPECT_RGB(&img, 0, 239, 0, 0, 255);
    NV_EXPECT_RGB(&img, 399, 239, 255, 255, 0);
    NV_EXPECT_RGB(&img, 40, 120, 255, 255, 255);
    NV_EXPECT_RGB(&img, 20, 120, 0, 0, 96);
    zimage_free(&img);
}

ZTEST(native, canvas_follows_resolution_change) {
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    AddScreenCorners(&db[0]);
    PresentDrawBuffer(&db[0]);
    ASSERT_FRAME("native_resolution_change_480x272");

    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){400, 240}));
    ClearOTag(db[0].ot, OT_LEN);
    db[0].count = 0;
    AddScreenCorners(&db[0]);
    PresentDrawBuffer(&db[0]);
    ASSERT_FRAME("native_resolution_change_400x240");
}

ZTEST(native, split_clip_keeps_inner_edge) {
    DRAWENV top;
    PsyzRect s;
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    s = Psyz_VideoGetDrawArea();
    SetDefDrawEnv(&top, 0, 0, PS1_FB_W, PS1_FB_H / 2);
    top.dtd = 0;
    AddTile(&db[0], s.sx, s.sy, Psyz_GetRectWidth(s), Psyz_GetRectHeight(s), 0,
            160, 0);
    PutDrawEnv(&db[0].draw);
    PutDrawEnv(&top);
    DrawOTag(db[0].ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&db[0].disp);
    ASSERT_FRAME("native_split_clip");
}

ZTEST(native, clear_image_extends_touching_edges) {
    RECT fb = {0, 0, PS1_FB_W, PS1_FB_H};
    RECT inner = {100, 100, 40, 30};
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    db[0].draw.isbg = 0;
    ClearImage(&fb, 120, 40, 40);
    ClearImage(&inner, 40, 200, 40);
    PresentDrawBuffer(&db[0]);
    ASSERT_FRAME("native_clear_image");
}

ZTEST(native, load_image_lands_in_framebuffer) {
    static u_short pattern[16 * 16];
    RECT dst = {8, 8, 16, 16};
    int i;
    for (i = 0; i < 16 * 16; i++) {
        pattern[i] = ((i ^ (i >> 4)) & 1) ? 0x001F : 0x7FFF;
    }
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    PutDrawEnv(&db[0].draw);
    LoadImage(&dst, (u_long*)pattern);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&db[0].disp);
    ASSERT_FRAME("native_load_image");
}

ZTEST(native, load_image_overhang) {
    RECT dst = {-80, -16, 480, 272};

    static u_short pattern[480 * 272];
    for (int i = 0; i < 480 * 272; i++) {
        pattern[i] = (i % 480) < 240 ? 0x001F : 0x03E0;
    }

    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    PutDrawEnv(&db[0].draw);
    LoadImage(&dst, (u_long*)pattern);
    DrawSync(0);
    AddTile(&db[0], 152, 112, 16, 16, 255, 255, 255);
    DrawOTag(db[0].ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&db[0].disp);
    ASSERT_FRAME("native_load_image_overhang");
}

ZTEST(native, load_image_overhang_keeps_vram_outside_framebuffer) {
    static u_short pattern[480 * 272];
    static u_short px[4 * 4];
    RECT dst = {-80, -16, 480, 272};
    RECT right = {330, 100, 4, 4};
    RECT below = {100, 250, 4, 4};
    for (int i = 0; i < 480 * 272; i++) {
        pattern[i] = 0x7FFF;
    }
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    PutDrawEnv(&db[0].draw);
    LoadImage(&dst, (u_long*)pattern);
    DrawSync(0);
    StoreImage(&right, (u_long*)px);
    DrawSync(0);
    zexpect_s32_eq(0, px[0]);
    StoreImage(&below, (u_long*)px);
    DrawSync(0);
    zexpect_s32_eq(0, px[15]);
}

static void AddTextureSprite(Db* b) {
    SetSprt(&b->sprt);
    setXY0(&b->sprt, 100, 100);
    setWH(&b->sprt, 16, 16);
    setUV0(&b->sprt, 16, 0);
    setRGB0(&b->sprt, 128, 128, 128);
    addPrim(b->ot, &b->sprt);
    SetDrawMode(&b->mode, 0, 0, getTPage(2, 0, 384, 0), NULL);
    addPrim(b->ot, &b->mode);
}

ZTEST(native, load_image_overhang_keeps_textures_at_2x) {
    zskip_targets("psp;ps1"); // don't support Internal Resolution multiplier
    static u_short pattern[480 * 272];
    static u_short texture[16 * 16];
    RECT dst = {-80, -16, 480, 272};
    RECT texture_rect = {400, 0, 16, 16};
    Db* b = &db[0];
    zimage img;
    for (int i = 0; i < 480 * 272; i++) {
        pattern[i] = 0x03E0; // GREEN
    }
    for (int i = 0; i < 16 * 16; i++) {
        texture[i] = 0x001F; // RED
    }
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    zassert_s32_eq(0, Psyz_VideoSetInternalResolution(2));
    LoadImage(&texture_rect, (u_long*)texture);
    AddTextureSprite(b);
    PutDrawEnv(&b->draw);
    DrawOTag(b->ot);
    DrawSync(0);
    LoadImage(&dst, (u_long*)pattern);
    DrawSync(0);
    ClearOTag(b->ot, OT_LEN);
    AddTextureSprite(b);
    DrawOTag(b->ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&b->disp);
    ASSERT_FRAME("load_image_overhang_keeps_textures_at_2x");
}

ZTEST(native, store_image_reads_canvas) {
    static u_short px[4 * 4];
    RECT probe = {20, 20, 4, 4};
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    AddTile(&db[0], 10, 10, 32, 32, 255, 0, 0);
    PutDrawEnv(&db[0].draw);
    DrawOTag(db[0].ot);
    DrawSync(0);
    StoreImage(&probe, (u_long*)px);
    DrawSync(0);
    zexpect_s32_eq(0x001F, px[0] & 0x7FFF);
    zexpect_s32_eq(0x001F, px[15] & 0x7FFF);
}

ZTEST(native, textures_sample_canvas) {
    Db* b = &db[0];
    zskip_targets("psp");
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    SetSprt(&b->sprt);
    setXY0(&b->sprt, 200, 100);
    setWH(&b->sprt, 32, 32);
    setUV0(&b->sprt, 0, 0);
    setRGB0(&b->sprt, 128, 128, 128);
    addPrim(b->ot, &b->sprt);
    SetDrawMode(&b->mode, 0, 0, getTPage(2, 0, 0, 0), NULL);
    addPrim(b->ot, &b->mode);
    AddTile(b, 8, 8, 16, 16, 0, 0, 255);
    AddTile(b, 0, 0, 32, 32, 255, 0, 0);
    AddScreenCorners(&db[0]);
    PresentDrawBuffer(b);
    ASSERT_FRAME("native_textures_sample_canvas");
}

ZTEST(native, framebuffers_keep_their_canvas) {
    zskip_targets("psp");
    zassert_s32_eq(0, Psyz_VideoSetResolution((PsyzSize){480, 272}));
    AddTile(&db[0], 0, 0, 64, 64, 255, 0, 0);
    PresentDrawBuffer(&db[0]);
    AddTile(&db[1], 0, 0, 64, 64, 0, 255, 0);
    PutDrawEnv(&db[1].draw);
    DrawOTag(db[1].ot);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&db[0].disp);
    ASSERT_FRAME("native_framebuffers_keep_canvas");
}

ZTEST(native, internal_res_auto_matches_display_scale) {
    zimage img;
    zskip_targets("psp");
    zassert_s32_eq(0, Psyz_VideoSimulateDisplay((PsyzSize){960, 544}));
    zassert_s32_eq(0, Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize()));
    zassert_s32_eq(0, Psyz_VideoSetInternalResolution(PSYZ_INTERNAL_RES_AUTO));
    zexpect_s32_eq(2, Psyz_VideoGetInternalResolution());
    AddScreenCorners(&db[0]);
    PresentDrawBuffer(&db[0]);
    img = zimage_frontbuffer();
    zexpect_s32_eq(960, img.width);
    zexpect_s32_eq(544, img.height);
    zimage_free(&img);
}

static void SetFbWidth(Db* b, int w) {
    SetDefDrawEnv(&b->draw, 0, 0, w, PS1_FB_H);
    SetDefDispEnv(&b->disp, 0, 0, w, PS1_FB_H);
    b->draw.dtd = 0;
    b->draw.isbg = 1;
    setRGB0(&b->draw, 0, 0, 96);
    PutDispEnv(&b->disp);
}

static void UseWholeDisplay(void) {
    zassert_s32_eq(0, Psyz_VideoSimulateDisplay((PsyzSize){480, 272}));
    zassert_s32_eq(0, Psyz_VideoSetResolution(Psyz_VideoGetDisplaySize()));
}

ZTEST(native, draw_area_follows_display_mode) {
    UseWholeDisplay();
    SetFbWidth(&db[0], 256);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -64, -16, 320, 256);
    SetFbWidth(&db[0], 320);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -80, -16, 400, 256);
    SetFbWidth(&db[0], 384);
#ifdef __PSP__
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -32, -16, 416, 256);
#else
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -96, -16, 480, 256);
#endif
    SetFbWidth(&db[0], 512);
#ifdef __PSP__
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), 0, 0, 512, 240);
#else
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -128, -16, 640, 256);
#endif
}

ZTEST(native, aspect_mode_updates_draw_area) {
    UseWholeDisplay();
    SetFbWidth(&db[0], 256);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -64, -16, 320, 256);
    zassert_s32_eq(0, Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE));
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -112, -16, 368, 256);
    zassert_s32_eq(0, Psyz_VideoSetAspectMode(PSYZ_ASPECT_DISPLAY));
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -64, -16, 320, 256);
}

ZTEST(native, display_range_updates_draw_area) {
    UseWholeDisplay();
    db[0].disp.screen.w = 200;
    PutDispEnv(&db[0].disp);
#ifdef __PSP__
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -64, -16, 384, 256);
#else
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -148, -16, 468, 256);
#endif
    db[0].disp.screen.w = 0;
    PutDispEnv(&db[0].disp);
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -80, -16, 400, 256);
}

ZTEST(native, display_size_ignores_aspect) {
    PsyzSize d;
    zassert_s32_eq(0, Psyz_VideoSimulateDisplay((PsyzSize){480, 272}));
    d = Psyz_VideoGetDisplaySize();
    zexpect_s32_eq(480, d.w);
    zexpect_s32_eq(272, d.h);
    zassert_s32_eq(0, Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE));
    d = Psyz_VideoGetDisplaySize();
    zexpect_s32_eq(480, d.w);
    zexpect_s32_eq(272, d.h);
}

ZTEST(native, simulate_display_validates_size) {
    zexpect_s32_eq(-1, Psyz_VideoSimulateDisplay((PsyzSize){-1, 272}));
    zexpect_s32_eq(-1, Psyz_VideoSimulateDisplay((PsyzSize){480, 0}));
    zexpect_s32_eq(0, Psyz_VideoSimulateDisplay((PsyzSize){480, 272}));
    zexpect_s32_eq(0, Psyz_VideoSimulateDisplay((PsyzSize){0, 0}));
}

ZTEST(native, wide_pixels_fill_display) {
    const int SIZE = 16;
    PsyzRect s;
    zimage img;
    UseWholeDisplay();
    SetFbWidth(&db[0], 256);
    s = Psyz_VideoGetDrawArea();
    AddTile(&db[0], 0, 0, 256, 1, 255, 255, 255);
    AddTile(&db[0], 0, 239, 256, 1, 255, 255, 255);
    AddTile(&db[0], s.sx, s.sy, SIZE, SIZE, 255, 0, 0);
    AddTile(&db[0], s.ex - SIZE, s.sy, SIZE, SIZE, 0, 255, 0);
    AddTile(&db[0], s.sx, s.ey - SIZE, SIZE, SIZE, 0, 0, 255);
    AddTile(&db[0], s.ex - SIZE, s.ey - SIZE, SIZE, SIZE, 255, 255, 0);
    PresentDrawBuffer(&db[0]);
    ASSERT_FRAME("native_wide_pixels");
    img = zimage_frontbuffer();
#ifdef __PSP__
    zexpect_s32_eq(480, img.width);
#else
    zexpect_s32_eq(384, img.width);
#endif
    zexpect_s32_eq(272, img.height);
    NV_EXPECT_RGB(&img, 2, 2, 255, 0, 0);
    NV_EXPECT_RGB(&img, img.width - 3, 2, 0, 255, 0);
    NV_EXPECT_RGB(&img, 2, img.height - 3, 0, 0, 255);
    NV_EXPECT_RGB(&img, img.width - 3, img.height - 3, 255, 255, 0);
    NV_EXPECT_RGB(&img, img.width / 2, img.height / 2, 0, 0, 96);
    zimage_free(&img);
}
