#include <psyz.h>
#include <libetc.h>
#include <libgpu.h>
#include "ztest.h"

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
    zexpect_rect_eq(Psyz_VideoGetDrawArea(), -192, -120, 832, 600);
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
