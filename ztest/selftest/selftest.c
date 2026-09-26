#include "ztest.h"

#include <string.h>

static int after_zassert;
static int setup_calls;
static int teardown_calls;

ZTEST(pass, scalars) {
    zassert_s32_eq(-5, -5);
    zassert_s32_ne(1, 2);
    zassert_s32_gt(1, 2);
    zassert_s32_ge(2, 2);
    zassert_s32_lt(2, 1);
    zassert_s32_le(2, 2);
    zassert_u32_eq(0xFFFFFFFF, 0xFFFFFFFF);
    zassert_u32_gt(1, 0x80000000);
    zassert_s16_eq(-1, 0xFFFF);
    zassert_u16_eq(0x1FFFF, 0xFFFF);
    zassert_s8_lt(0, 0x80);
    zassert_u8_gt(0, 0x80);
    zassert_char_eq('a', 'a');
    zassert_char_lt('b', 'a');
    zassert_uintptr_eq(0x1234, 0x1234);
    zassert_ptr_eq(NULL, NULL);
}

ZTEST(pass, strings_and_arrays) {
    static const unsigned char a[] = {1, 2, 3, 4};
    static const unsigned char b[] = {1, 2, 3, 5};
    zassert_str_eq("hello", "hello");
    zassert_str_ne("hello", "world");
    zassert_str_lt("b", "a");
    zassert_str_eq(NULL, NULL);
    zassert_u8array_eq(a, a, sizeof(a));
    zassert_u8array_ne(a, b, sizeof(a));
    zassert_u8array_gt(a, b, sizeof(a));
}

ZTEST(pass, expect_returns_result) { zassert_s32_eq(1, zexpect_s32_eq(3, 3)); }

ZTEST(fail, scalars) {
    zexpect_s32_eq(123, 456);
    zexpect_s32_ne(7, 7);
    zexpect_s32_gt(5, 3);
    zexpect_s32_ge(5, 3);
    zexpect_s32_lt(3, 5);
    zexpect_s32_le(3, 5);
    zexpect_u32_eq(0xDEADBEEF, 3);
    zexpect_s16_eq(0, 0x18000);
    zexpect_u16_eq(1, 0x18000);
    zexpect_s8_eq(0, 0x1FF);
    zexpect_u8_eq(0, 0x1FF);
    zexpect_char_eq('A', '\n');
    zexpect_ptr_eq(NULL, (void*)0x1234);
    zexpect_uintptr_ne(0xCAFE, 0xCAFE);
    zexpect_str_eq("short", "shirt");
    zexpect_str_eq(NULL, "x");
}

ZTEST(fail, u8array_single_byte) {
    static const unsigned char e[] = {
        0, 0, 0xDE, 0xAD, 0xBE, 0xEF, 0xC1, 0xBA, 0xDC, 0x0F, 0xFE, 0, 0};
    static const unsigned char a[] = {
        0, 0, 0xDE, 0xAD, 0xBE, 0xEF, 0xC2, 0xBA, 0xDC, 0x0F, 0xFE, 0, 0};
    zexpect_u8array_eq(e, a, sizeof(e));
}

ZTEST(fail, u8array_long_run) {
    static const unsigned char e[] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xC1, 0xBA, 0xDC, 0x0F, 0xFE, 0x00};
    static const unsigned char a[] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0x00};
    zexpect_u8array_eq(e, a, sizeof(e));
}

ZTEST(fail, u8array_short_run_at_start) {
    static const unsigned char e[] = {1, 2, 3, 4, 5, 6, 7, 8};
    static const unsigned char a[] = {9, 9, 3, 4, 5, 6, 7, 8};
    zexpect_u8array_eq(e, a, sizeof(e));
}

ZTEST(fail, u8array_at_end) {
    static const unsigned char e[] = {1, 2, 3, 4, 5, 6, 7, 8};
    static const unsigned char a[] = {1, 2, 3, 4, 5, 6, 7, 9};
    zexpect_u8array_eq(e, a, sizeof(e));
}

ZTEST(fail, u8array_all_different) {
    static const unsigned char e[] = {1, 2, 3, 4, 5, 6};
    static const unsigned char a[] = {9, 9, 9, 9, 9, 9};
    static const unsigned char tiny_e[] = {1, 2};
    static const unsigned char tiny_a[] = {3, 4};
    zexpect_u8array_eq(e, a, sizeof(e));
    zexpect_u8array_eq(tiny_e, tiny_a, sizeof(tiny_e));
    zexpect_u8array_ne(e, e, sizeof(e));
}

ZTEST(fail, long_strings) {
    const char* fox =
        "The quick brown fox jumps over the lazy dog, again and again";
    const char* fox2 =
        "The quick brown fox jumped over the lazy dog, again and again";
    const char* longer =
        "A string that is long enough to need the window layout here";
    zexpect_str_eq(fox, fox2);
    zexpect_str_eq(longer, "A string that is long enough");
}

ZTEST(fail, log_is_interleaved) {
    zprintf("before the failure\n");
    zexpect_s32_eq(1, 2);
    zerrorf("an error line\n");
    zprintf("multi\nline");
    zexpect_s32_eq(3, 4);
}

ZTEST(fail, formatter) {
    zprintf("[%d] [%5d] [%-5d] [%05d] [%+d] [%i]\n", -42, 42, 42, 42, 42, 7);
    zprintf("[%x] [%X] [%08X] [%o] [%u] [%lu]\n", 255u, 255u, 0xBEEFu, 8u,
            4000000000u, 123456UL);
    zprintf("[%lld] [%llu] [%zu] [%hd] [%hhu]\n", -9000000000LL, 18000000000ULL,
            (size_t)77, (short)-3, (unsigned char)250);
    zprintf("[%s] [%.3s] [%8s] [%-8s] [%c] [%%] [%p]\n", "str", "abcdef", "r",
            "l", 'z', (void*)0);
    zprintf("[%f] [%.2f] [%.0f] [%8.3f] [%.1f]\n", 3.14159, -2.005, 2.5,
            1.0 / 3.0, 99.96);
    zfail();
}

ZTEST(fail, zfail_and_zabort) {
    zfail("custom message %d", 42);
    zfail();
    zabort("stop here");
    zfail("never logged");
}

ZTEST(control, zassert_stops_the_test) {
    after_zassert = 0;
    zassert_s32_eq(1, 2);
    after_zassert = 1;
}

ZTEST(control, zexpect_keeps_going) { zassert_s32_eq(0, after_zassert); }

ZTEST(control, skip) {
    zskip();
    zfail("not reached");
}

ZTEST(control, skip_with_reason) {
    zprintf("printed only with --verbose\n");
    zskip("reason %d", 7);
}

ZTEST(control, skip_targets_matches_current) {
    zskip_targets("nothing;" ZTEST_TARGET);
    zfail("not reached");
}

ZTEST(control, skip_targets_other) {
    zskip_targets("not-a-target;neither");
    zassert_s32_eq(0, ztest_is_target("not-a-target"));
    zassert_s32_eq(1, ztest_is_target(ZTEST_TARGET));
}

ZTEST(control, tags) {
    ztest_add_tag("selftest-tag");
    zassert_s32_eq(1, ztest_is_target("a;selftest-tag"));
}

ZTEST(control, fail_then_skip_is_a_failure) {
    zexpect_s32_eq(1, 0);
    zskip("skipped after a failure");
}

ZTEST_SETUP(fixture) { setup_calls++; }

ZTEST_TEARDOWN(fixture) { teardown_calls++; }

ZTEST(fixture, first) {
    zassert_s32_eq(1, setup_calls);
    zassert_s32_eq(0, teardown_calls);
    zassert_s32_eq(1, 0);
}

ZTEST(fixture, teardown_ran_after_abort) {
    zassert_s32_eq(2, setup_calls);
    zassert_s32_eq(1, teardown_calls);
}

ZTEST_SETUP(fixture_abort) { zabort("setup failed"); }

ZTEST_TEARDOWN(fixture_abort) { zprintf("teardown still runs\n"); }

ZTEST(fixture_abort, body_is_not_run) { zfail("not reached"); }

ZTEST_SETUP(fixture_skip) { zskip("whole group skipped"); }

ZTEST(fixture_skip, body_is_not_run) { zfail("not reached"); }

static zimage gradient(zimage_format format, int w, int h) {
    zimage img = zimage_alloc(w, h, format);
    int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            unsigned char rgb[3];
            rgb[0] = (unsigned char)(x * 255 / (w - 1));
            rgb[1] = (unsigned char)(y * 255 / (h - 1));
            rgb[2] = (unsigned char)((x + y) * 8);
            zimage_set_rgb(&img, x, y, rgb);
        }
    }
    return img;
}

static zimage nudged(zimage img, int x, int y, int amount) {
    unsigned char rgb[3];
    zimage_get_rgb(&img, x, y, rgb);
    rgb[0] = (unsigned char)(rgb[0] - amount * 8);
    zimage_set_rgb(&img, x, y, rgb);
    return img;
}

static zimage g16(zimage_format format) { return gradient(format, 16, 16); }

static zimage nudge(int x, int y, int amount) {
    return nudged(g16(ZIMAGE_RGB888), x, y, amount);
}

ZTEST(image, exact_rgb888) {
    zassert_image_eq("gradient", g16(ZIMAGE_RGB888), zimage_r5g5b5_exact);
}

ZTEST(image, exact_other_formats) {
    zexpect_image_eq("gradient", g16(ZIMAGE_R5G5B5), zimage_r5g5b5_exact);
    zexpect_image_eq("gradient", g16(ZIMAGE_RGBA8888), zimage_r5g5b5_exact);
    zexpect_image_eq("gradient", g16(ZIMAGE_RGBA5551_BE), zimage_r5g5b5_exact);
    zexpect_image_eq("gradient", g16(ZIMAGE_R5G6B5), zimage_r5g5b5_exact);
}

ZTEST(image, tolerance_and_precision) {
    const zimage_cmp* tol = zimage_r5g5b5_with_tolerance(2);
    const zimage_cmp* prec = zimage_r5g5b5_with_precision(0.99f);
    const zimage_cmp* both = zimage_r5g5b5_with_tol_prec(1, 0.99f);
    const zimage_cmp* top_left = zimage_r5g5b5_region(0, 0, 8, 8, 0, 1.0f);
    zexpect_image_eq("gradient", nudge(8, 8, 2), tol);
    zexpect_image_eq("gradient", nudge(8, 8, 9), prec);
    zexpect_image_eq("gradient", nudge(8, 8, 9), both);
    zexpect_image_eq("gradient", nudge(15, 15, 9), top_left);
    zexpect_image_ne("gradient", nudge(3, 4, 1), zimage_r5g5b5_exact);
}

ZTEST(image, mismatch) {
    const zimage_cmp* tol = zimage_r5g5b5_with_tolerance(2);
    zexpect_image_eq("gradient", nudge(5, 6, 3), tol);
}

ZTEST(image, size_and_region_errors) {
    const zimage_cmp* outside = zimage_r5g5b5_region(8, 8, 16, 16, 0, 1.0f);
    zimage narrow = gradient(ZIMAGE_RGB888, 8, 16);
    zexpect_image_eq("gradient", narrow, zimage_r5g5b5_exact);
    zexpect_image_eq("gradient", g16(ZIMAGE_RGB888), outside);
    zexpect_image_ne("gradient", g16(ZIMAGE_RGB888), zimage_r5g5b5_exact);
}

ZTEST(image, missing_golden) {
    zimage small = gradient(ZIMAGE_RGB888, 4, 4);
    zexpect_image_eq("does_not_exist", small, zimage_r5g5b5_exact);
}

ZTEST(image, png_loader) {
    zimage img = zimage_png("expected/gradient.png");
    zimage missing = zimage_exp_png("does_not_exist");
    unsigned char rgb[3];
    zassert_ptr_ne(NULL, img.data);
    zassert_s32_eq(16, img.width);
    zassert_s32_eq(16, img.height);
    zassert_s32_eq(ZIMAGE_RGB888, img.format);
    zimage_get_rgb(&img, 15, 0, rgb);
    zassert_u8_eq(255, rgb[0]);
    zassert_ptr_eq(NULL, missing.data);
    zimage_free(&img);
    zassert_ptr_eq(NULL, img.data);
}

ZTEST(image, empty_actual) {
    zimage empty = {0, 0, ZIMAGE_NONE, 0, NULL};
    zexpect_image_eq("gradient", empty, zimage_r5g5b5_exact);
}

ZTEST(image, frontbuffer) {
    zimage fb = zimage_frontbuffer();
    if (ztest_is_target("psp;ps1;nds;n64")) {
        zassert_ptr_ne(NULL, fb.data);
        zassert_s32_gt(0, fb.width);
        zassert_s32_gt(0, fb.height);
    } else {
        zassert_ptr_eq(NULL, fb.data);
    }
}

ZTEST(terminate, stops_the_suite) {
    zterminate("fatal: %s", "giving up");
    zfail("not reached");
}

ZTEST(terminate, is_not_run) { zfail("not reached"); }

#ifdef __PSP__
#include <pspkernel.h>

PSP_MODULE_INFO("ztest_selftest", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-1024);
#endif

ZTEST_MAIN
