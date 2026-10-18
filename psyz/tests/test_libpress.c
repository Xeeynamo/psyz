#include "ztest.h"
#include <psyz.h>
#include <libapi.h>
#include <libetc.h>
#include <libpress.h>
#include <stdint.h>
#include <string.h>
#include "../../decomp/src/libpress/libpress_private.h"

static uint32_t runlevel[33], output[256];
static DECDCTENV saved_env;
static int inputs, outputs, depth, maximum_depth;

static void input_complete(void) { ++inputs; }
static void output_complete(void) { ++outputs; }

static void next_slice(void) {
    ++depth;
    if (depth > maximum_depth)
        maximum_depth = depth;
    ++outputs;
    if (outputs < 6)
        DecDCTout((u_long*)(output + outputs * 32), 32);
    --depth;
}

ZTEST_SETUP(libpress) {
    StopCallback();
    ResetCallback();
    DecDCTGetEnv(&saved_env);
    DecDCTReset(0);
    runlevel[0] = 0x38000020;
    for (int i = 1; i <= 6; ++i)
        runlevel[i] = 0xFE000000;
    for (int i = 7; i < 33; ++i)
        runlevel[i] = 0xFE00FE00;
    memset(output, 0xA5, sizeof(output));
    inputs = outputs = depth = maximum_depth = 0;
}

ZTEST_TEARDOWN(libpress) {
    DecDCTinCallback(NULL);
    DecDCToutCallback(NULL);
    DecDCTReset(1);
    DecDCTPutEnv(&saved_env);
    DecDCTReset(1);
}

static void check_gray(int words) {
    for (int i = 0; i < words; ++i)
        zassert_u32_eq(0x80808080, output[i]);
}

ZTEST(libpress, reset_loads_sdk_tables_and_chains_six_rgb24_slices) {
    DecDCTinCallback(input_complete);
    DecDCToutCallback(next_slice);
    DecDCTin((u_long*)runlevel, 1);
    zassert_s32_eq(1, inputs);
    zassert_s32_eq(1, DecDCTinSync(1));
    zassert_u32_eq(2, (MDEC_status() >> 25) & 3);
    DecDCTout((u_long*)output, 32);
    zassert_s32_eq(6, outputs);
    zassert_s32_eq(1, maximum_depth);
    zassert_s32_eq(0, DecDCTinSync(0));
    zassert_s32_eq(0, DecDCToutSync(0));
    zassert_s32_eq(0, DecDCTinSync(1));
    zassert_s32_eq(0, DecDCToutSync(1));
    check_gray(192);
    zassert_u32_eq(0xA5A5A5A5, output[192]);
}

ZTEST(libpress, callbacks_wait_for_exit_from_critical_section) {
    DecDCTinCallback(input_complete);
    DecDCToutCallback(next_slice);
    EnterCriticalSection();
    DecDCTin((u_long*)runlevel, 1);
    DecDCTout((u_long*)output, 32);
    zassert_s32_eq(0, inputs);
    zassert_s32_eq(0, outputs);
    ExitCriticalSection();
    zassert_s32_eq(1, inputs);
    zassert_s32_eq(6, outputs);
    check_gray(192);
}

ZTEST(libpress, sdk_dma_sizes_use_whole_32_word_blocks) {
    runlevel[0] = 0x30000020;
    MDEC_in((u_long*)runlevel, 63);
    zassert_uintptr_eq(0x20, Psyz_DmaRead(DMA_CHANNEL_MDEC_IN, 1));
    zassert_uintptr_eq(
        (u_long)(runlevel + 33), Psyz_DmaRead(DMA_CHANNEL_MDEC_IN, 0));
    DecDCTout((u_long*)output, 63);
    check_gray(32);
    for (int i = 32; i < 256; ++i)
        zassert_u32_eq(0xA5A5A5A5, output[i]);
    zassert_uintptr_eq(
        (u_long)(output + 32), Psyz_DmaRead(DMA_CHANNEL_MDEC_OUT, 0));
    DecDCTout((u_long*)(output + 32), 160);
    check_gray(192);
    zassert_s32_eq(0, DecDCTinSync(0));
}

ZTEST(libpress, unsupported_reset_mode_keeps_the_current_decode) {
    DecDCTin((u_long*)runlevel, 1);
    DecDCTout((u_long*)output, 32);
    MDEC_reset(2);
    DecDCTout((u_long*)(output + 32), 160);
    zassert_s32_eq(0, DecDCToutSync(0));
    check_gray(192);
}

ZTEST(libpress, output_dma_can_wait_for_input) {
    DecDCToutCallback(output_complete);
    DecDCTout((u_long*)output, 192);
    zassert_s32_eq(0, outputs);
    zassert_uintptr_eq(0x01000200, Psyz_DmaRead(DMA_CHANNEL_MDEC_OUT, 2));
    DecDCTin((u_long*)runlevel, 1);
    zassert_s32_eq(1, outputs);
    zassert_s32_eq(0, DecDCToutSync(0));
    check_gray(192);
}

ZTEST(libpress, abort_cancels_pending_dma_without_a_completion) {
    DecDCToutCallback(output_complete);
    DecDCTout((u_long*)output, 192);
    DecDCTReset(1);
    zassert_uintptr_eq(0, Psyz_DmaRead(DMA_CHANNEL_MDEC_IN, 2));
    zassert_uintptr_eq(0, Psyz_DmaRead(DMA_CHANNEL_MDEC_OUT, 2));
    DecDCTin((u_long*)runlevel, 1);
    zassert_s32_eq(0, outputs);
    zassert_u32_eq(0xA5A5A5A5, output[0]);
    DecDCTout((u_long*)output, 192);
    zassert_s32_eq(1, outputs);
    check_gray(192);
}

ZTEST(libpress, abort_preserves_tables_and_discards_partial_output) {
    uint32_t expected[192];
    for (int i = 1; i <= 6; ++i)
        runlevel[i] |= (4u << 10) | (i * 8);
    DecDCTin((u_long*)runlevel, 1);
    DecDCTout((u_long*)expected, 192);
    zassert_u32_ne(0x80808080, expected[0]);
    DecDCTin((u_long*)runlevel, 1);
    DecDCTout((u_long*)output, 32);
    DecDCTReset(1);
    DecDCTin((u_long*)runlevel, 1);
    DecDCTout((u_long*)output, 192);
    zassert_s32_eq(0, DecDCToutSync(0));
    zassert_u8array_eq(expected, output, sizeof(expected));
}

ZTEST(libpress, input_timeout_resets_dma_and_allows_the_next_decode) {
    runlevel[0] = 0x30000040;
    MDEC_in((u_long*)runlevel, 32);
    zassert_s32_eq(-1, DecDCTinSync(0));
    zassert_uintptr_eq(0, Psyz_DmaRead(DMA_CHANNEL_MDEC_IN, 2));
    zassert_s32_eq(0, DecDCTinSync(1));
    runlevel[0] = 0x30000020;
    DecDCTin((u_long*)runlevel, 1);
    DecDCTout((u_long*)output, 192);
    zassert_s32_eq(0, DecDCToutSync(0));
    check_gray(192);
}

ZTEST(libpress, output_timeout_resets_dma_without_a_false_completion) {
    DecDCToutCallback(output_complete);
    DecDCTin((u_long*)runlevel, 1);
    DecDCTout((u_long*)output, 256);
    zassert_s32_eq(0, outputs);
    zassert_s32_eq(-1, DecDCToutSync(0));
    zassert_uintptr_eq(0, Psyz_DmaRead(DMA_CHANNEL_MDEC_OUT, 2));
    DecDCTin((u_long*)runlevel, 1);
    DecDCTout((u_long*)output, 192);
    zassert_s32_eq(1, outputs);
    check_gray(192);
}
