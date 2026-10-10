#include "ztest.h"
#include <psyz.h>
#include <libgpu.h>
#include <libetc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/internal.h"
#include "../src/draw.h"
#define PS1_RECT RECT
#include "../src/platform/rgb24.h"
#undef PS1_RECT

static unsigned char* capture;

ZTEST_SETUP(rgb24) {
    zskip_targets("psp");
    ResetGraph(0);
    RECT clear = {0, 0, 1024, 512};
    ClearImage(&clear, 0, 0, 0);
    DrawSync(0);
}

ZTEST_TEARDOWN(rgb24) {
    free(capture);
    capture = NULL;
    ResetGraph(0);
}

ZTEST(rgb24, invalidation_uses_packed_word_width_and_excludes_touching_edges) {
    static const int writes[][5] = {
        {3, 7, 1, 1, 1}, {7, 8, 1, 1, 1}, {8, 7, 1, 1, 0},
        {2, 7, 1, 1, 0}, {3, 6, 1, 1, 0}, {3, 9, 1, 1, 0},
        {2, 6, 2, 2, 1}, {3, 7, 0, 1, 0}, {3, 7, 1, 0, 0}};
    for (size_t i = 0; i < sizeof(writes) / sizeof(*writes); ++i) {
        const int* r = writes[i];
        zassert_s32_eq(
            r[4], Rgb24RegionOverlaps(3, 7, 3, 2, r[0], r[1], r[2], r[3]));
    }
    zassert_s32_eq(1, Rgb24RegionOverlaps(0, 0, 320, 240, 479, 0, 1, 1));
    zassert_s32_eq(0, Rgb24RegionOverlaps(0, 0, 320, 240, 480, 0, 1, 1));
}

ZTEST(rgb24, invalidation_excludes_clipped_and_empty_source_regions) {
    zassert_s32_eq(1, Rgb24RegionOverlaps(1023, 511, 3, 2, 1023, 511, 1, 1));
    zassert_s32_eq(0, Rgb24RegionOverlaps(1023, 511, 3, 2, 1024, 511, 1, 1));
    zassert_s32_eq(0, Rgb24RegionOverlaps(1023, 511, 3, 2, 1023, 512, 1, 1));
    zassert_s32_eq(1, Rgb24RegionOverlaps(0, -1, 3, 2, 0, 0, 1, 1));
    zassert_s32_eq(0, Rgb24RegionOverlaps(0, -1, 3, 2, 0, -1, 1, 1));
    zassert_s32_eq(0, Rgb24RegionOverlaps(-1, 0, 3, 2, 0, 0, 1, 1));
    zassert_s32_eq(0, Rgb24RegionOverlaps(0, -2, 3, 2, 0, 0, 1, 1));
    zassert_s32_eq(0, Rgb24RegionOverlaps(0, 0, 0, 2, 0, 0, 1, 1));
}

ZTEST(rgb24, odd_width_and_word_offset_preserve_row_boundaries) {
    static const uint16_t words[] = {0x0201, 0x0403, 0x0605, 0x0807, 0xff09,
                                     0x0b0a, 0x0d0c, 0x0f0e, 0x1110, 0xff12};
    static const uint8_t expected[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
    RECT area = {3, 7, 5, 2};
    LoadImage(&area, (u_long*)words);
    DrawSync(0);
    capture = AllocRgb24Region(3, 7, 3, 2);
    zassert_ptr_ne(NULL, capture);
    zassert_u8array_eq(expected, capture, sizeof(expected));
}

ZTEST(rgb24, pixels_outside_vram_are_black) {
    uint32_t words = 0x00000201;
    RECT area = {1023, 511, 1, 1};
    LoadImage(&area, (u_long*)&words);
    DrawSync(0);
    static const uint8_t expected[] = {1, 2, 0, 0, 0, 0};
    capture = AllocRgb24Region(1023, 511, 1, 2);
    zassert_ptr_ne(NULL, capture);
    zassert_u8array_eq(expected, capture, sizeof(expected));
}

ZTEST(rgb24, display_capture_tracks_upload_move_and_clear) {
    DISPENV display;
    SetDefDispEnv(&display, 2, 5, 256, 240);
    display.isrgb24 = 1;
    PutDispEnv(&display);
    uint32_t words[2] = {0x44332211, 0x00006655};
    RECT area = {2, 5, 3, 1};
    LoadImage(&area, (u_long*)words);
    DrawSync(0);
    VSync(0);
    int width, height;
    capture = Psyz_VideoAllocCapturedFrame(&width, &height);
    zassert_ptr_ne(NULL, capture);
    zassert_s32_eq(256, width);
    zassert_s32_eq(240, height);
    static const uint8_t expected[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    zassert_u8array_eq(expected, capture, sizeof(expected));
    free(capture);
    capture = NULL;
    MoveImage(&area, 2, 6);
    ClearImage(&area, 0, 0, 0);
    DrawSync(0);
    VSync(0);
    capture = Psyz_VideoAllocCapturedFrame(&width, &height);
    zassert_ptr_ne(NULL, capture);
    uint8_t zero[6] = {0};
    zassert_u8array_eq(zero, capture, sizeof(zero));
    zassert_u8array_eq(expected, capture + width * 3, sizeof(expected));
}

ZTEST(rgb24, display_start_ignores_low_x_bit) {
    DISPENV display;
    SetDefDispEnv(&display, 3, 7, 256, 240);
    display.isrgb24 = 1;
    PutDispEnv(&display);
    uint32_t words[2] = {0x44332211, 0x00006655};
    RECT area = {2, 7, 3, 1};
    LoadImage(&area, (u_long*)words);
    DrawSync(0);
    int width, height;
    capture = Psyz_VideoAllocCapturedFrame(&width, &height);
    zassert_ptr_ne(NULL, capture);
    static const uint8_t expected[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    zassert_u8array_eq(expected, capture, sizeof(expected));
}
