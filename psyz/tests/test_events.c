#include <string.h>
#include "ztest.h"
#include <psyz.h>
#include <kernel.h>
#include <libapi.h>
#include <libetc.h>
#include <libcd.h>

long ChangeClearRCnt(long t, long flag);

#define EV_USER (DescUEV | 0x5A)
#define EV_OTHER (DescUEV | 0x5B)
#define EV_MAX_OPEN 300
#define AKAO_TARGET 0x43D1
#define RCNT2_HZ (33868800 / 8)
#define CARD_SLOT_1 0x00
#define CARD_SLOT_2 0x10

enum { CARD_IOE, CARD_ERROR, CARD_TIMOUT, CARD_NEW, CARD_SPECS };
static const long card_specs[CARD_SPECS] = {
    EvSpIOE, EvSpERROR, EvSpTIMOUT, EvSpNEW};

static long ev_opened[EV_MAX_OPEN];
static int ev_opened_len;
static long card_sw[CARD_SPECS];
static long card_hw[CARD_SPECS];
static int card_started;
static volatile int handler_calls;
static volatile char handler_log[8];
static volatile int handler_log_len;

static long ev_open(unsigned long desc, long spec, long mode, long (*func)()) {
    long ev;
    EnterCriticalSection();
    ev = OpenEvent(desc, spec, mode, func);
    ExitCriticalSection();
    if (ev != -1 && ev_opened_len < EV_MAX_OPEN) {
        ev_opened[ev_opened_len++] = ev;
    }
    return ev;
}

static long ev_close(long ev) {
    long ret;
    for (int i = 0; i < ev_opened_len; i++) {
        if (ev_opened[i] == ev) {
            ev_opened[i] = ev_opened[--ev_opened_len];
            break;
        }
    }
    EnterCriticalSection();
    ret = CloseEvent(ev);
    ExitCriticalSection();
    return ret;
}

static void ev_deliver(unsigned long desc, unsigned long spec) {
    EnterCriticalSection();
    DeliverEvent(desc, spec);
    ExitCriticalSection();
}

static void ev_undeliver(unsigned long desc, unsigned long spec) {
    EnterCriticalSection();
    UnDeliverEvent(desc, spec);
    ExitCriticalSection();
}

static long cb_increase_handler_calls(void) {
    handler_calls++;
    return 0;
}

static void log_handler(char id) {
    if (handler_log_len < (int)sizeof(handler_log) - 1) {
        handler_log[handler_log_len++] = id;
    }
}
static long log_handler_a(void) {
    log_handler('a');
    return 0;
}
static long log_handler_b(void) {
    log_handler('b');
    return 0;
}
static long log_handler_c(void) {
    log_handler('c');
    return 0;
}

static long chain_handler(void) {
    handler_calls++;
    DeliverEvent(EV_OTHER, EvSpINT);
    return 0;
}

static PsyzVsyncMode saved_vsync_mode;
ZTEST_SETUP(events) {
#if !defined(__psx__) && !defined(__PSP__)
    // All RCNT tests need a frame limiter to pass
    saved_vsync_mode = Psyz_VideoGetVsyncMode();
    Psyz_VideoSetVsyncMode(PSYZ_VSYNC_OFF);
#endif
    ResetCallback();
    ev_opened_len = 0;
    handler_calls = 0;
    handler_log_len = 0;
    memset((void*)handler_log, 0, sizeof(handler_log));
}

ZTEST_TEARDOWN(events) {
    StopRCnt(RCntCNT2);
    if (card_started) {
        // reset memcard use after the tests
        card_started = 0;
        StopCARD();
        ChangeClearRCnt(3, 0);
    }
    while (ev_opened_len > 0) {
        ev_close(ev_opened[0]);
    }
#if !defined(__psx__) && !defined(__PSP__)
    Psyz_VideoSetVsyncMode(saved_vsync_mode);
#endif
}

static int wait_handler_calls(int calls, int max_vblanks) {
    while (handler_calls < calls && max_vblanks-- > 0) {
        VSync(0);
    }
    return handler_calls >= calls;
}

static int poll_event(long ev, int max_vblanks) {
    for (;;) {
        if (TestEvent(ev)) {
            return 1;
        }
        if (max_vblanks-- <= 0) {
            return 0;
        }
        VSync(0);
    }
}

static int handler_calls_over_vblanks(int vblanks) {
    int start;
    VSync(0);
    start = handler_calls;
    while (vblanks-- > 0) {
        VSync(0);
    }
    return handler_calls - start;
}

static int handler_calls_at_vblank(int vblank, int* calls) {
    while (VSync(-1) < vblank) {
    }
    *calls = handler_calls;
    return VSync(-1) == vblank;
}

static int handler_calls_over_vblank_count(int vblanks) {
    int start, end;
    int retries = 4;
    for (;;) {
        int first = VSync(-1) + 1;
        int ok = handler_calls_at_vblank(first, &start);
        ok &= handler_calls_at_vblank(first + vblanks, &end);
        if (ok || retries-- <= 0) {
            return end - start;
        }
    }
}

static void rcnt2_start(unsigned short target, long mode) {
    zassert_s32_eq(1, SetRCnt(RCntCNT2, target, mode));
    zassert_s32_eq(1, StartRCnt(RCntCNT2));
}

static void cd_start(void) {
    static int initialized;
    if (!initialized) {
        CdInit();
        initialized = 1;
    }
}

static void card_start(void) {
    for (int i = 0; i < CARD_SPECS; i++) {
        card_sw[i] = ev_open(SwCARD, card_specs[i], EvMdNOINTR, NULL);
        card_hw[i] = ev_open(HwCARD, card_specs[i], EvMdNOINTR, NULL);
        zassert_s32_ne(-1, card_sw[i]);
        zassert_s32_ne(-1, card_hw[i]);
    }
    card_started = 1;
    EnterCriticalSection();
    InitCARD(0);
    StartCARD();
    ChangeClearPAD(0);
    _bu_init();
    _card_auto(0);
    for (int i = 0; i < CARD_SPECS; i++) {
        EnableEvent(card_sw[i]);
        EnableEvent(card_hw[i]);
    }
    ExitCriticalSection();
}

static int card_poll(const long* events, int max_vblanks) {
    for (;;) {
        for (int i = 0; i < CARD_SPECS; i++) {
            if (TestEvent(events[i])) {
                return i;
            }
        }
        if (max_vblanks-- <= 0) {
            return -1;
        }
        VSync(0);
    }
}

static int card_info(long chan) {
    zassert_s32_eq(1, _card_info(chan));
    return card_poll(card_sw, 180);
}

ZTEST(events, open_event_returns_valid_descriptor) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zassert_s32_ne(-1, ev);
    zexpect_u32_eq(DescEV, (unsigned long)ev & 0xFFFF0000);
}

ZTEST(events, open_event_returns_distinct_descriptors) {
    long a = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    long b = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    long c = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_ne(-1, a);
    zexpect_s32_ne(-1, b);
    zexpect_s32_ne(-1, c);
    zexpect_s32_ne(a, b);
    zexpect_s32_ne(a, c);
    zexpect_s32_ne(b, c);
}

ZTEST(events, open_event_reuses_lowest_closed_descriptor) {
    long a = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    long b = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    long c = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    ev_close(c);
    ev_close(a);
    zexpect_s32_eq(a, ev_open(EV_OTHER, EvSpIOE, EvMdNOINTR, NULL));
    zexpect_s32_eq(c, ev_open(EV_OTHER, EvSpIOE, EvMdNOINTR, NULL));
    zexpect_s32_ne(b, ev_open(EV_OTHER, EvSpIOE, EvMdNOINTR, NULL));
}

ZTEST(events, open_event_returns_invalid_slot_when_table_is_full) {
    int n = 0;
    long last = -1;
    while (n < EV_MAX_OPEN) {
        long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
        if (ev == -1) {
            break;
        }
        last = ev;
        n++;
    }
    zprintf("opened %d events before the table was full\n", n);
    zassert_s32_gt(0, n);
    zassert_s32_lt(EV_MAX_OPEN, n);
    zexpect_s32_eq(-1, ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL));
    ev_close(last);
    zexpect_s32_eq(last, ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL));
}

ZTEST(events, open_event_starts_disabled) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, close_event_returns_one) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, ev_close(ev));
}

ZTEST(events, closed_event_ignores_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, cb_increase_handler_calls);
    EnableEvent(ev);
    ev_close(ev);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev));
    zexpect_s32_eq(0, handler_calls);
}

ZTEST(events, closing_pending_event_drops_the_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    ev_close(ev);
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, event_calls_use_low_16_bits_of_descriptor) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    long slot = ev & 0xFFFF;
    zexpect_s32_eq(1, EnableEvent(slot));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(slot));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, DisableEvent(slot));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, enable_event_returns_one) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, EnableEvent(ev));
    zexpect_s32_eq(1, EnableEvent(ev));
}

ZTEST(events, disable_event_returns_one) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, DisableEvent(ev));
    zexpect_s32_eq(1, EnableEvent(ev));
    zexpect_s32_eq(1, DisableEvent(ev));
}

ZTEST(events, delivery_before_enable_is_lost) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, EnableEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, disabled_event_ignores_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, EnableEvent(ev));
    zexpect_s32_eq(1, DisableEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev));
    zexpect_s32_eq(1, EnableEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, disable_event_discards_pending_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, EnableEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, DisableEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
    zexpect_s32_eq(1, EnableEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, enable_event_discards_pending_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, EnableEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, EnableEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, enable_event_on_closed_event_keeps_it_closed) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    ev_close(ev);
    zexpect_s32_eq(1, EnableEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev));
    zexpect_s32_eq(ev, ev_open(EV_OTHER, EvSpIOE, EvMdNOINTR, NULL));
}

ZTEST(events, reenabled_event_receives_new_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, EnableEvent(ev));
    zexpect_s32_eq(1, DisableEvent(ev));
    zexpect_s32_eq(1, EnableEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev));
}

ZTEST(events, test_event_returns_zero_without_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, EnableEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, test_event_returns_one_only_once) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, EnableEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, repeated_deliveries_collapse_into_one) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, EnableEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    ev_deliver(EV_USER, EvSpINT);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, test_event_rearms_for_next_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(1, EnableEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev));
}

ZTEST(events, test_event_leaves_other_events_pending) {
    long a = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(a);
    long b = ev_open(EV_USER, EvSpIOE, EvMdNOINTR, NULL);
    EnableEvent(b);
    ev_deliver(EV_USER, EvSpINT);
    ev_deliver(EV_USER, EvSpIOE);
    zexpect_s32_eq(1, TestEvent(a));
    zexpect_s32_eq(0, TestEvent(a));
    zexpect_s32_eq(1, TestEvent(b));
}

ZTEST(events, deliver_event_matches_spec_exactly_not_as_mask) {
    long ev_int = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev_int);
    long ev_ioe = ev_open(EV_USER, EvSpIOE, EvMdNOINTR, NULL);
    EnableEvent(ev_ioe);
    long ev_both = ev_open(EV_USER, EvSpINT | EvSpIOE, EvMdNOINTR, NULL);
    EnableEvent(ev_both);
    ev_deliver(EV_USER, EvSpINT | EvSpIOE);
    zexpect_s32_eq(0, TestEvent(ev_int));
    zexpect_s32_eq(0, TestEvent(ev_ioe));
    zexpect_s32_eq(1, TestEvent(ev_both));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev_int));
    zexpect_s32_eq(0, TestEvent(ev_ioe));
    zexpect_s32_eq(0, TestEvent(ev_both));
}

ZTEST(events, deliver_event_matches_desc_exactly) {
    long ev_user = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev_user);
    long ev_other = ev_open(EV_OTHER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev_other);
    ev_deliver(EV_OTHER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev_user));
    zexpect_s32_eq(1, TestEvent(ev_other));
    ev_deliver(EV_USER & 0xFFFF, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev_user));
}

ZTEST(events, deliver_event_reaches_every_matching_event) {
    long a = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(a);
    long b = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(b);
    long c = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(c);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(a));
    zexpect_s32_eq(1, TestEvent(b));
    zexpect_s32_eq(1, TestEvent(c));
}

ZTEST(events, deliver_event_without_listeners_is_harmless) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_deliver(EV_OTHER, EvSpERROR);
    ev_deliver(EV_OTHER, 0);
    ev_deliver(0, 0);
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, deliver_event_works_for_every_descriptor_class) {
    static const struct {
        unsigned long desc;
        long spec;
    } cases[] = {
        {EV_USER, EvSpINT},   {HwCdRom, EvSpACK},   {HwCdRom, EvSpCOMP},
        {HwCdRom, EvSpDR},    {HwCdRom, EvSpDE},    {HwCdRom, EvSpERROR},
        {HwSPU, EvSpCOMP},    {HwCARD, EvSpIOE},    {HwCARD, EvSpERROR},
        {HwCARD, EvSpTIMOUT}, {HwCARD, EvSpNEW},    {SwCARD, EvSpIOE},
        {SwCARD, EvSpERROR},  {SwCARD, EvSpTIMOUT}, {SwCARD, EvSpNEW},
        {RCntCNT0, EvSpINT},  {RCntCNT1, EvSpINT},  {RCntCNT2, EvSpINT},
        {HwCPU, EvSpSYSCALL}, {SwMATH, EvSpEDOM},   {SwMATH, EvSpERANGE},
        {DescTH | 1, EvSpCZ}, {0x12345678, 0x4321}, {1, 0},
    };
    for (int i = 0; i < (int)(sizeof(cases) / sizeof(*cases)); i++) {
        long ev = ev_open(cases[i].desc, cases[i].spec, EvMdNOINTR, NULL);
        EnableEvent(ev);
        zassert_s32_ne(-1, ev);
        zexpect_s32_eq(0, TestEvent(ev));
        ev_deliver(cases[i].desc, cases[i].spec);
        zexpect_s32_eq(1, TestEvent(ev));
        ev_close(ev);
    }
}

ZTEST(events, sw_card_and_hw_card_do_not_cross) {
    long sw = ev_open(SwCARD, EvSpIOE, EvMdNOINTR, NULL);
    EnableEvent(sw);
    long hw = ev_open(HwCARD, EvSpIOE, EvMdNOINTR, NULL);
    EnableEvent(hw);
    ev_deliver(SwCARD, EvSpIOE);
    zexpect_s32_eq(1, TestEvent(sw));
    zexpect_s32_eq(0, TestEvent(hw));
    ev_deliver(HwCARD, EvSpIOE);
    zexpect_s32_eq(0, TestEvent(sw));
    zexpect_s32_eq(1, TestEvent(hw));
}

ZTEST(events, intr_event_calls_handler_before_deliver_returns) {
    long counted =
        ev_open(EV_USER, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, handler_calls);
}

ZTEST(events, intr_event_calls_handler_on_every_delivery) {
    long counted =
        ev_open(EV_USER, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    ev_deliver(EV_USER, EvSpINT);
    ev_deliver(EV_USER, EvSpINT);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(3, handler_calls);
}

ZTEST(events, intr_event_never_becomes_pending) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(2, handler_calls);
}

ZTEST(events, intr_event_handler_not_called_while_disabled) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, handler_calls);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, handler_calls);
    DisableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, handler_calls);
}

ZTEST(events, intr_event_handler_only_called_for_matching_delivery) {
    long counted =
        ev_open(EV_USER, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    ev_deliver(EV_USER, EvSpIOE);
    ev_deliver(EV_OTHER, EvSpINT);
    zexpect_s32_eq(0, handler_calls);
}

ZTEST(events, intr_handlers_run_in_slot_order) {
    long a = ev_open(EV_USER, EvSpINT, EvMdINTR, log_handler_a);
    EnableEvent(a);
    long b = ev_open(EV_USER, EvSpINT, EvMdINTR, log_handler_b);
    EnableEvent(b);
    long c = ev_open(EV_USER, EvSpINT, EvMdINTR, log_handler_c);
    EnableEvent(c);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_str_eq("abc", (const char*)handler_log);
    ev_close(a);
    long c2 = ev_open(EV_USER, EvSpINT, EvMdINTR, log_handler_c);
    EnableEvent(c2);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_str_eq("abccbc", (const char*)handler_log);
}

ZTEST(events, nointr_event_ignores_its_handler) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, cb_increase_handler_calls);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, handler_calls);
    zexpect_s32_eq(1, TestEvent(ev));
}

ZTEST(events, handler_can_deliver_another_event) {
    long other = ev_open(EV_OTHER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(other);
    long chained = ev_open(EV_USER, EvSpINT, EvMdINTR, chain_handler);
    EnableEvent(chained);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, handler_calls);
    zexpect_s32_eq(1, TestEvent(other));
}

ZTEST(events, mixed_intr_and_nointr_listeners_both_served) {
    long polled = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(polled);
    long counted =
        ev_open(EV_USER, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    ev_deliver(EV_USER, EvSpINT);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(2, handler_calls);
    zexpect_s32_eq(1, TestEvent(polled));
    zexpect_s32_eq(0, TestEvent(polled));
}

ZTEST(events, undeliver_event_cancels_pending_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    ev_undeliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, undeliver_event_keeps_event_enabled) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    ev_undeliver(EV_USER, EvSpINT);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev));
}

ZTEST(events, undeliver_event_matches_desc_and_spec_exactly) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    ev_undeliver(EV_USER, EvSpIOE);
    ev_undeliver(EV_USER, EvSpINT | EvSpIOE);
    ev_undeliver(EV_OTHER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev));
}

ZTEST(events, undeliver_event_cancels_every_matching_event) {
    long a = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(a);
    long b = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(b);
    ev_deliver(EV_USER, EvSpINT);
    ev_undeliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(a));
    zexpect_s32_eq(0, TestEvent(b));
}

ZTEST(events, undeliver_event_without_pending_is_harmless) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_undeliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev));
}

ZTEST(events, undeliver_event_keeps_disabled_event_disabled) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    ev_undeliver(EV_USER, EvSpINT);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, undeliver_event_does_not_stop_intr_handler) {
    long counted =
        ev_open(EV_USER, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    ev_deliver(EV_USER, EvSpINT);
    ev_undeliver(EV_USER, EvSpINT);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(2, handler_calls);
}

ZTEST(events, wait_event_returns_one_when_already_delivered) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, WaitEvent(ev));
}

ZTEST(events, wait_event_consumes_the_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    WaitEvent(ev);
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, wait_event_returns_zero_on_disabled_event) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    zexpect_s32_eq(0, WaitEvent(ev));
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    DisableEvent(ev);
    zexpect_s32_eq(0, WaitEvent(ev));
}

ZTEST(events, wait_event_returns_zero_on_closed_event) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    ev_close(ev);
    zexpect_s32_eq(0, WaitEvent(ev));
}

ZTEST(events, wait_event_rearms_for_next_delivery) {
    long ev = ev_open(EV_USER, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, WaitEvent(ev));
    ev_deliver(EV_USER, EvSpINT);
    zexpect_s32_eq(1, TestEvent(ev));
}

ZTEST(events, wait_event_blocks_until_rcnt2_interrupt) {
    long ev = ev_open(RCntCNT2, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    int calls;
    long counted =
        ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    zassert_s32_eq(1, poll_event(ev, 10));
    zexpect_s32_eq(1, WaitEvent(ev));
    calls = handler_calls;
    zexpect_s32_eq(1, WaitEvent(ev));
    zexpect_s32_gt(calls, handler_calls);
}

ZTEST(events, rcnt2_intr_event_calls_handler_from_interrupt) {
    long counted =
        ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    zexpect_s32_eq(1, wait_handler_calls(1, 10));
}

ZTEST(events, rcnt2_nointr_event_becomes_pending) {
    long ev = ev_open(RCntCNT2, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    zexpect_s32_eq(1, poll_event(ev, 10));
    zexpect_s32_eq(1, poll_event(ev, 10));
}

ZTEST(events, rcnt2_event_stays_clear_until_counter_is_started) {
    long ev = ev_open(RCntCNT2, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    zassert_s32_eq(1, SetRCnt(RCntCNT2, AKAO_TARGET, RCntMdINTR));
    zexpect_s32_eq(0, poll_event(ev, 5));
    zassert_s32_eq(1, StartRCnt(RCntCNT2));
    zexpect_s32_eq(1, poll_event(ev, 10));
}

ZTEST(events, rcnt2_nointr_counter_mode_never_delivers) {
    long ev = ev_open(RCntCNT2, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    rcnt2_start(AKAO_TARGET, RCntMdNOINTR);
    VSync(0);
    TestEvent(ev);
    zexpect_s32_eq(0, poll_event(ev, 5));
}

ZTEST(events, rcnt2_fires_at_one_eighth_system_clock) {
    const int vblank_hz = GetVideoMode() == MODE_PAL ? 50 : 60;
    const int expected = 30 * (RCNT2_HZ / AKAO_TARGET) / vblank_hz;
    int calls;
    long counted =
        ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    calls = handler_calls_over_vblank_count(30);
    zprintf("%d calls in 30 vblanks, expected %d\n", calls, expected);
    zexpect_s32_ge(expected - 4, calls);
    zexpect_s32_le(expected + 4, calls);
}

ZTEST(events, rcnt2_system_clock_mode_fires_eight_times_faster) {
    int slow, fast;
    long counted =
        ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    rcnt2_start(0x8000, RCntMdINTR);
    slow = handler_calls_over_vblank_count(30);
    StopRCnt(RCntCNT2);
    rcnt2_start(0x8000, RCntMdINTR | RCntMdSC);
    fast = handler_calls_over_vblank_count(30);
    zprintf("%d calls at clock/8, %d calls at clock\n", slow, fast);
    zexpect_s32_ge(slow * 8 - 12, fast);
    zexpect_s32_le(slow * 8 + 12, fast);
}

ZTEST(events, stop_rcnt_stops_event_delivery) {
    int calls;
    long counted =
        ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    zassert_s32_eq(1, wait_handler_calls(1, 10));
    zexpect_s32_eq(1, StopRCnt(RCntCNT2));
    calls = handler_calls;
    zexpect_s32_eq(0, handler_calls_over_vblanks(3));
    zexpect_s32_eq(calls, handler_calls);
}

ZTEST(events, start_rcnt_resumes_event_delivery) {
    int calls;
    long counted =
        ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    zassert_s32_eq(1, wait_handler_calls(1, 10));
    StopRCnt(RCntCNT2);
    calls = handler_calls;
    zexpect_s32_eq(1, StartRCnt(RCntCNT2));
    zexpect_s32_eq(1, wait_handler_calls(calls + 2, 10));
}

ZTEST(events, start_rcnt_delivers_interrupt_latched_while_stopped) {
    long counted =
        ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    zassert_s32_eq(1, wait_handler_calls(1, 10));
    StopRCnt(RCntCNT2);
    VSync(0);
    zassert_s32_eq(1, SetRCnt(RCntCNT2, AKAO_TARGET, RCntMdNOINTR));
    handler_calls = 0;
    StartRCnt(RCntCNT2);
    zexpect_s32_eq(0, handler_calls_over_vblanks(3));
    zexpect_s32_eq(1, handler_calls);
}

ZTEST(events, disable_event_silences_running_counter) {
    long ev = ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(ev);
    int calls;
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    zassert_s32_eq(1, wait_handler_calls(1, 10));
    DisableEvent(ev);
    calls = handler_calls;
    zexpect_s32_eq(0, handler_calls_over_vblanks(3));
    EnableEvent(ev);
    zexpect_s32_eq(1, wait_handler_calls(calls + 2, 10));
}

ZTEST(events, close_event_silences_running_counter) {
    long ev = ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(ev);
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    zassert_s32_eq(1, wait_handler_calls(1, 10));
    ev_close(ev);
    zexpect_s32_eq(0, handler_calls_over_vblanks(3));
}

ZTEST(events, akao_deinit_sequence_leaves_nothing_pending) {
    long akao = ev_open(RCntCNT2, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(akao);
    long polled = ev_open(RCntCNT2, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(polled);
    rcnt2_start(AKAO_TARGET, RCntMdINTR);
    zassert_s32_eq(1, wait_handler_calls(2, 10));
    zexpect_s32_eq(1, StopRCnt(RCntCNT2));
    ev_undeliver(RCntCNT2, EvSpINT);
    zexpect_s32_eq(0, TestEvent(polled));
    zexpect_s32_eq(1, DisableEvent(akao));
    zexpect_s32_eq(1, ev_close(akao));
    zexpect_s32_eq(0, handler_calls_over_vblanks(3));
    zexpect_s32_eq(0, TestEvent(polled));
}

ZTEST(events, rcnt3_event_fires_once_per_vblank) {
    long counted =
        ev_open(RCntCNT3, EvSpINT, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    zexpect_s32_eq(30, handler_calls_over_vblanks(30));
}

ZTEST(events, rcnt3_nointr_event_becomes_pending_every_vblank) {
    long ev = ev_open(RCntCNT3, EvSpINT, EvMdNOINTR, NULL);
    EnableEvent(ev);
    VSync(0);
    zexpect_s32_eq(1, TestEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
    VSync(0);
    VSync(0);
    zexpect_s32_eq(1, TestEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, hw_vblank_trap_event_fires_once_per_vblank) {
    long counted =
        ev_open(HwVBLANK, EvSpTRAP, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    zexpect_s32_eq(30, handler_calls_over_vblanks(30));
}

ZTEST(events, cdrom_command_interrupt_delivers_hw_cdrom_trap) {
    unsigned char result[8];
    long ev;
    cd_start();
    ev = ev_open(HwCdRom, EvSpTRAP, EvMdNOINTR, NULL);
    EnableEvent(ev);
    CdControlB(CdlNop, NULL, result);
    zexpect_s32_eq(1, TestEvent(ev));
    zexpect_s32_eq(0, TestEvent(ev));
}

ZTEST(events, cdrom_command_interrupt_calls_intr_handler) {
    unsigned char result[8];
    cd_start();
    long counted =
        ev_open(HwCdRom, EvSpTRAP, EvMdINTR, cb_increase_handler_calls);
    EnableEvent(counted);
    CdControlB(CdlNop, NULL, result);
    zexpect_s32_ge(1, handler_calls);
}

ZTEST(events, cdrom_trap_stays_clear_without_commands) {
    long ev;
    cd_start();
    ev = ev_open(HwCdRom, EvSpTRAP, EvMdNOINTR, NULL);
    EnableEvent(ev);
    zexpect_s32_eq(0, poll_event(ev, 5));
}

ZTEST(events, card_events_stay_clear_without_requests) {
    card_start();
    for (int i = 0; i < 10; i++) {
        VSync(0);
    }
    zexpect_s32_eq(-1, card_poll(card_sw, 0));
    zexpect_s32_eq(-1, card_poll(card_hw, 0));
}

ZTEST(events, card_info_raises_exactly_one_sw_card_event) {
    static const long slots[] = {CARD_SLOT_1, CARD_SLOT_2};
    card_start();
    for (int i = 0; i < 2; i++) {
        int status = card_info(slots[i]);
        zprintf("slot %d raised SwCARD spec index %d\n", i + 1, status);
        zexpect_s32_ne(-1, status);
        zexpect_s32_eq(-1, card_poll(card_sw, 10));
    }
}

ZTEST(events, card_info_never_raises_error_for_a_valid_slot) {
    card_start();
    zexpect_s32_ne(CARD_ERROR, card_info(CARD_SLOT_1));
    zexpect_s32_ne(CARD_ERROR, card_info(CARD_SLOT_2));
}

ZTEST(events, card_info_result_is_repeatable) {
    int first;
    card_start();
    first = card_info(CARD_SLOT_1);
    zexpect_s32_eq(first, card_info(CARD_SLOT_1));
    first = card_info(CARD_SLOT_2);
    zexpect_s32_eq(first, card_info(CARD_SLOT_2));
}

ZTEST(events, card_info_raises_the_same_spec_on_hw_card) {
    int sw;
    card_start();
    sw = card_info(CARD_SLOT_1);
    zassert_s32_ne(-1, sw);
    zexpect_s32_eq(sw, card_poll(card_hw, 10));
    zexpect_s32_eq(-1, card_poll(card_hw, 0));
}

ZTEST(events, card_info_on_empty_slot_raises_timeout) {
    int status;
    card_start();
    status = card_info(CARD_SLOT_2);
    if (status == CARD_IOE || status == CARD_NEW) {
        zskip("a card is plugged into slot 2");
    }
    zexpect_s32_eq(CARD_TIMOUT, status);
}

ZTEST(events, card_info_with_card_raises_ioe_or_new) {
    int status;
    card_start();
    status = card_info(CARD_SLOT_1);
    if (status == CARD_TIMOUT) {
        zskip("no card in slot 1");
    }
    zexpect_s32_ne(CARD_ERROR, status);
    zexpect_s32_ne(-1, status);
}

ZTEST(events, card_load_with_card_raises_ioe) {
    card_start();
    if (card_info(CARD_SLOT_1) != CARD_IOE) {
        zskip("slot 1 has no card ready for reading");
    }
    card_poll(card_hw, 0);
    zassert_s32_eq(1, _card_load(CARD_SLOT_1));
    zexpect_s32_eq(CARD_IOE, card_poll(card_sw, 180));
}

ZTEST(events, disabled_card_event_misses_card_info_result) {
    card_start();
    for (int i = 0; i < CARD_SPECS; i++) {
        DisableEvent(card_sw[i]);
    }
    zassert_s32_eq(1, _card_info(CARD_SLOT_1));
    zassert_s32_ne(-1, card_poll(card_hw, 180));
    for (int i = 0; i < CARD_SPECS; i++) {
        EnableEvent(card_sw[i]);
    }
    zexpect_s32_eq(-1, card_poll(card_sw, 10));
}
