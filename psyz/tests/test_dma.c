#include "ztest.h"
#include <libapi.h>
#include <libetc.h>
#include <psyz.h>
#include "../src/internal.h"

static int calls, depth, maximum_depth;

static void completed(void) { ++calls; }
static void chained(void) {
    ++depth;
    if (depth > maximum_depth) {
        maximum_depth = depth;
    }
    if (++calls < 1024) {
        Psyz_KernelDmaComplete(DMA_CHANNEL_MDEC_OUT);
    }
    --depth;
}

ZTEST_SETUP(dma) {
    StopCallback();
    ResetCallback();
    calls = depth = maximum_depth = 0;
}

ZTEST_TEARDOWN(dma) {
    StopCallback();
    ResetCallback();
    DMACallback(DMA_CHANNEL_MDEC_OUT, NULL);
}

ZTEST(dma, callback_replacement_and_removal) {
    zassert_ptr_eq(NULL, DMACallback(DMA_CHANNEL_MDEC_OUT, completed));
    zassert_ptr_eq(completed, DMACallback(DMA_CHANNEL_MDEC_OUT, chained));
    zassert_ptr_eq(chained, DMACallback(DMA_CHANNEL_MDEC_OUT, NULL));
    Psyz_KernelDmaComplete(DMA_CHANNEL_MDEC_OUT);
    zassert_s32_eq(0, calls);
}

ZTEST(dma, completion_waits_for_critical_section) {
    DMACallback(DMA_CHANNEL_MDEC_OUT, completed);
    EnterCriticalSection();
    Psyz_KernelDmaComplete(DMA_CHANNEL_MDEC_OUT);
    zassert_s32_eq(0, calls);
    ExitCriticalSection();
    zassert_s32_eq(1, calls);
}

ZTEST(dma, callback_can_submit_another_transfer_without_recursion) {
    DMACallback(DMA_CHANNEL_MDEC_OUT, chained);
    Psyz_KernelDmaComplete(DMA_CHANNEL_MDEC_OUT);
    zassert_s32_eq(1024, calls);
    zassert_s32_eq(1, maximum_depth);
}

ZTEST(dma, removed_callback_discards_pending_completion) {
    DMACallback(DMA_CHANNEL_MDEC_OUT, completed);
    EnterCriticalSection();
    Psyz_KernelDmaComplete(DMA_CHANNEL_MDEC_OUT);
    DMACallback(DMA_CHANNEL_MDEC_OUT, NULL);
    ExitCriticalSection();
    zassert_s32_eq(0, calls);
}

ZTEST(dma, stop_and_restart_preserve_registration) {
    DMACallback(DMA_CHANNEL_MDEC_OUT, completed);
    StopCallback();
    Psyz_KernelDmaComplete(DMA_CHANNEL_MDEC_OUT);
    zassert_s32_eq(0, calls);
    RestartCallback();
    zassert_s32_eq(1, calls);
}

ZTEST(dma, reset_discards_pending_completion_and_registration) {
    DMACallback(DMA_CHANNEL_MDEC_OUT, completed);
    EnterCriticalSection();
    Psyz_KernelDmaComplete(DMA_CHANNEL_MDEC_OUT);
    StopCallback();
    ResetCallback();
    ExitCriticalSection();
    Psyz_KernelDmaComplete(DMA_CHANNEL_MDEC_OUT);
    zassert_s32_eq(0, calls);
}

ZTEST(dma, invalid_channels_do_not_change_registration) {
    DMACallback(DMA_CHANNEL_MDEC_OUT, completed);
    zassert_ptr_eq(NULL, DMACallback(-1, chained));
    zassert_ptr_eq(NULL, DMACallback(DMA_CHANNEL_OTC + 1, chained));
    Psyz_KernelDmaComplete(-1);
    Psyz_KernelDmaComplete(DMA_CHANNEL_OTC + 1);
    zassert_s32_eq(0, calls);
    Psyz_KernelDmaComplete(DMA_CHANNEL_MDEC_OUT);
    zassert_s32_eq(1, calls);
}
