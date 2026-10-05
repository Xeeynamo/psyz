// PS1 kernel events, critical sections, root counters and the interrupt
// controller behind libetc's callbacks.
#include <psyz.h>
#include <psyz/log.h>
#include <kernel.h>
#include <libapi.h>
#include <libetc.h>
#include <string.h>
#include "os.h"
#include "../internal.h"

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
typedef volatile long AtomicWord;
static inline long AtomicLoad(AtomicWord* p) { return _InterlockedOr(p, 0); }
static inline void AtomicStore(AtomicWord* p, long v) {
    _InterlockedExchange(p, v);
}
static inline int AtomicCas(AtomicWord* p, long expected, long desired) {
    return _InterlockedCompareExchange(p, desired, expected) == expected;
}
static inline void AtomicAdd(AtomicWord* p, long v) {
    _InterlockedExchangeAdd(p, v);
}
#else
typedef long AtomicWord;
static inline long AtomicLoad(AtomicWord* p) {
    return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}
static inline void AtomicStore(AtomicWord* p, long v) {
    __atomic_store_n(p, v, __ATOMIC_SEQ_CST);
}
static inline int AtomicCas(AtomicWord* p, long expected, long desired) {
    return __atomic_compare_exchange_n(
        p, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
static inline void AtomicAdd(AtomicWord* p, long v) {
    __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST);
}
#endif

#define EVENT_COUNT 16 // retail kernel default, see SetConf
#define EVENT_SLOT_MASK 0xFFFF

#define SYSTEM_CLOCK_HZ 33868800
#define DOT_CLOCK_HZ 6711647 // GPU clock / 8, the 320 pixel wide modes
#define HBLANK_NTSC_HZ 15734
#define HBLANK_PAL_HZ 15625
#define MIN_PERIOD_US 32      // faster timers would only spin the host
#define MAX_CATCHUP_US 200000 // ticks owed past a longer host stall are lost

enum {
    IRQ_VBLANK,
    IRQ_GPU,
    IRQ_CDROM,
    IRQ_DMA,
    IRQ_RCNT0,
    IRQ_RCNT1,
    IRQ_RCNT2,
    IRQ_SIO0,
    IRQ_SIO,
    IRQ_SPU,
    IRQ_PIO,
    IRQ_COUNT,
};
#define IRQ_ALL ((1u << IRQ_COUNT) - 1)

typedef struct {
    AtomicWord status; // EvStUNUSED, EvStWAIT, EvStACTIVE or EvStALREADY
    unsigned int desc;
    unsigned int spec;
    long mode;
    long (*handler)();
} Event;

typedef struct {
    uint32_t last_us;   // when the count last restarted from 0
    uint32_t next_us;   // when the count reaches the target
    uint32_t period_us; // 0 until SetRCnt
    uint32_t period_frac;
    uint32_t frac_acc;
    uint32_t clock_hz;
    uint32_t ticks_per_us_q15;
    uint32_t target;
    int irq;
} RootCounter;

typedef struct {
    unsigned int desc;
    unsigned int spec;
} PendingEvent;

static Event events[EVENT_COUNT];
static AtomicWord event_waiters;

static void* kernel_lock;
static volatile uintptr_t lock_owner;
static int lock_depth;
static int in_critical_section;
static int in_interrupt;

static void* bell_events;
static void* bell_irq;
static int irq_thread_state; // 0: not started, 1: running, -1: polled

static unsigned irq_mask = 1u << IRQ_VBLANK;
static unsigned irq_latched;
static unsigned irq_saved_mask;
static int callbacks_running = 1;
static long rcnt_auto_ack[4] = {1, 1, 1, 1};
static RootCounter counters[3];
static AtomicWord poll_deadline_us;

static PendingEvent pending[32];
static int pending_len;

static void* post_lock;
static void (*posted[16])(void);
static int posted_len;

static void (*vsync_callbacks[8])(void);
static void VSyncDispatch(void);
static void (*irq_callbacks[IRQ_COUNT])(void) = {VSyncDispatch};

static void KernelInit(void) {
    if (kernel_lock) {
        return;
    }
    post_lock = Psyz_OsLockCreate();
    bell_events = Psyz_OsBellCreate();
    bell_irq = Psyz_OsBellCreate();
    kernel_lock = Psyz_OsLockCreate();
    if (!kernel_lock || !post_lock || !bell_events || !bell_irq) {
        ERRORF("cannot create the kernel synchronisation objects");
    }
}

static int OwnsKernel(void) { return lock_owner == Psyz_OsThreadSelf(); }

static void KernelLock(void) {
    uintptr_t self = Psyz_OsThreadSelf();
    KernelInit();
    if (lock_owner == self) {
        lock_depth++;
        return;
    }
    Psyz_OsLock(kernel_lock);
    lock_owner = self;
    lock_depth = 1;
}

static void KernelUnlock(void) {
    if (--lock_depth > 0) {
        return;
    }
    lock_owner = 0;
    Psyz_OsUnlock(kernel_lock);
}

static Event* EventAt(long event) {
    unsigned long slot = (unsigned long)event & EVENT_SLOT_MASK;
    return slot < EVENT_COUNT ? &events[slot] : NULL;
}

static void DeliverLocked(unsigned int desc, unsigned int spec) {
    int became_pending = 0;
    for (int i = 0; i < EVENT_COUNT; i++) {
        Event* e = &events[i];
        if (AtomicLoad(&e->status) != EvStACTIVE || e->desc != desc ||
            e->spec != spec) {
            continue;
        }
        if (e->mode == EvMdNOINTR) {
            became_pending |= AtomicCas(&e->status, EvStACTIVE, EvStALREADY);
        } else if (e->mode == EvMdINTR && e->handler) {
            e->handler();
        }
    }
    if (became_pending && AtomicLoad(&event_waiters)) {
        Psyz_OsBellRing(bell_events);
    }
}

static void ServiceLine(int line) {
    irq_latched &= ~(1u << line);
    switch (line) {
    case IRQ_VBLANK:
        DeliverLocked(RCntCNT3, EvSpINT);
        DeliverLocked(HwVBLANK, EvSpTRAP);
        break;
    case IRQ_RCNT0:
    case IRQ_RCNT1:
    case IRQ_RCNT2:
        DeliverLocked(RCntCNT0 + (line - IRQ_RCNT0), EvSpINT);
        break;
    }
    if (irq_callbacks[line]) {
        irq_callbacks[line]();
    }
}

static void DrainPosted(void) {
    for (;;) {
        void (*fn)(void) = NULL;
        Psyz_OsLock(post_lock);
        if (posted_len > 0) {
            fn = posted[0];
            posted_len--;
            memmove(posted, posted + 1, posted_len * sizeof(*posted));
        }
        Psyz_OsUnlock(post_lock);
        if (!fn) {
            return;
        }
        fn();
    }
}

// Runs every unmasked latched line, queued event and posted callback.
// Needs the kernel lock outside of a critical section.
static void ServicePending(void) {
    if (in_interrupt || in_critical_section) {
        return;
    }
    in_interrupt = 1;
    for (;;) {
        unsigned ready = irq_latched & irq_mask;
        if (ready) {
            int line = 0;
            while (!(ready & (1u << line))) {
                line++;
            }
            ServiceLine(line);
        } else if (pending_len > 0) {
            PendingEvent ev = pending[0];
            pending_len--;
            memmove(pending, pending + 1, pending_len * sizeof(*pending));
            DeliverLocked(ev.desc, ev.spec);
        } else if (posted_len > 0) {
            DrainPosted();
        } else {
            break;
        }
    }
    in_interrupt = 0;
}

static void LatchLine(int line) {
    KernelLock();
    irq_latched |= 1u << line;
    ServicePending();
    KernelUnlock();
}

void Psyz_KernelRaise(unsigned int desc, unsigned int spec) {
    KernelLock();
    if (pending_len < LEN(pending)) {
        pending[pending_len].desc = desc;
        pending[pending_len].spec = spec;
        pending_len++;
    } else {
        WARNF("dropped event %08X/%04X", desc, spec);
    }
    ServicePending();
    KernelUnlock();
}

static void StartIrqThread(void);
void Psyz_KernelPost(void (*fn)(void)) {
    KernelInit();
    Psyz_OsLock(post_lock);
    if (posted_len < LEN(posted)) {
        posted[posted_len++] = fn;
    } else {
        WARNF("dropped a posted interrupt");
    }
    Psyz_OsUnlock(post_lock);
    StartIrqThread();
    Psyz_OsBellRing(bell_irq);
}

void Psyz_KernelVBlank(void) {
    Psyz_KernelPoll();
    LatchLine(IRQ_VBLANK);
}

void PS1_EnterCriticalSection(void) {
    if (OwnsKernel()) {
        return; // already in one, or inside an interrupt handler
    }
    KernelLock();
    in_critical_section = 1;
}

void PS1_ExitCriticalSection(void) {
    if (!OwnsKernel() || !in_critical_section) {
        return;
    }
    in_critical_section = 0;
    ServicePending();
    KernelUnlock();
}

long OpenEvent(unsigned long desc, long spec, long mode, long (*func)()) {
    long id = -1;
    KernelLock();
    for (int i = 0; i < EVENT_COUNT; i++) {
        Event* e = &events[i];
        if (AtomicLoad(&e->status) == EvStUNUSED) {
            e->desc = (unsigned int)desc;
            e->spec = (unsigned int)spec;
            e->mode = mode;
            e->handler = func;
            AtomicStore(&e->status, EvStWAIT);
            id = (long)(DescEV | (unsigned)i);
            break;
        }
    }
    KernelUnlock();
    return id;
}

long CloseEvent(long event) {
    Event* e = EventAt(event);
    if (!e) {
        return 0;
    }
    KernelLock();
    AtomicStore(&e->status, EvStUNUSED);
    KernelUnlock();
    return 1;
}

static long SetEnabled(long event, long status) {
    Event* e = EventAt(event);
    if (!e) {
        return 0;
    }
    KernelLock();
    if (AtomicLoad(&e->status) != EvStUNUSED) {
        AtomicStore(&e->status, status);
    }
    KernelUnlock();
    return 1;
}

long EnableEvent(long event) { return SetEnabled(event, EvStACTIVE); }

long DisableEvent(long event) { return SetEnabled(event, EvStWAIT); }

long TestEvent(long event) {
    Event* e = EventAt(event);
    if (!e) {
        return 0;
    }
    if (AtomicCas(&e->status, EvStALREADY, EvStACTIVE)) {
        return 1;
    }
    if (irq_thread_state < 0) {
        Psyz_KernelPoll();
    }
    return 0;
}

long WaitEvent(long event) {
    Event* e = EventAt(event);
    long ret;
    int resume_critical_section;
    if (!e) {
        return 0;
    }
    if (AtomicCas(&e->status, EvStALREADY, EvStACTIVE)) {
        return 1;
    }
    if (AtomicLoad(&e->status) != EvStACTIVE) {
        return 0;
    }
    // interrupts cannot be delivered to a thread sleeping in a critical section
    resume_critical_section = OwnsKernel() && in_critical_section;
    if (resume_critical_section) {
        PS1_ExitCriticalSection();
    }
    KernelInit();
    AtomicAdd(&event_waiters, 1);
    for (;;) {
        if (AtomicCas(&e->status, EvStALREADY, EvStACTIVE)) {
            ret = 1;
            break;
        }
        if (AtomicLoad(&e->status) != EvStACTIVE) {
            ret = 0;
            break;
        }
        if (irq_thread_state < 0) {
            Psyz_KernelPoll();
            Psyz_OsBellWait(bell_events, 1000);
        } else {
            Psyz_OsBellWait(bell_events, 100000);
        }
    }
    AtomicAdd(&event_waiters, -1);
    if (resume_critical_section) {
        PS1_EnterCriticalSection();
    }
    return ret;
}

void DeliverEvent(unsigned ev1, unsigned ev2) {
    KernelLock();
    DeliverLocked(ev1, ev2);
    KernelUnlock();
}

void UnDeliverEvent(unsigned ev1, unsigned ev2) {
    KernelLock();
    for (int i = 0; i < EVENT_COUNT; i++) {
        Event* e = &events[i];
        if (e->desc == ev1 && e->spec == ev2 && e->mode == EvMdNOINTR) {
            AtomicCas(&e->status, EvStALREADY, EvStACTIVE);
        }
    }
    KernelUnlock();
}

static int CounterLine(long spec) {
    unsigned i = (unsigned)spec & 0xFFFF;
    return i < 3 ? IRQ_RCNT0 + (int)i : i == 3 ? IRQ_VBLANK : -1;
}

static unsigned CounterClock(int i, long mode) {
    if (mode & RCntMdSC) {
        return SYSTEM_CLOCK_HZ;
    }
    switch (i) {
    case 0:
        return DOT_CLOCK_HZ;
    case 1:
        return GetVideoMode() == MODE_PAL ? HBLANK_PAL_HZ : HBLANK_NTSC_HZ;
    default:
        return SYSTEM_CLOCK_HZ / 8;
    }
}

static int IsDue(uint32_t deadline_us, uint32_t now_us) {
    return (int32_t)(now_us - deadline_us) >= 0;
}

static void CounterStep(RootCounter* c) {
    c->last_us = c->next_us;
    c->next_us += c->period_us;
    c->frac_acc += c->period_frac;
    if (c->frac_acc >= c->clock_hz) {
        c->frac_acc -= c->clock_hz;
        c->next_us++;
    }
}

static void CounterRestart(RootCounter* c, uint32_t now_us) {
    c->last_us = now_us;
    c->next_us = now_us + c->period_us;
    c->frac_acc = 0;
}

// Fires every timer interrupt that came due. Returns 0 when no timer is
// triggered, otherwise 1 with the earliest deadline in *deadline_us.
static int ServiceTimers(uint32_t* deadline_us) {
    int triggered = 0;
    KernelLock();
    uint32_t now = Psyz_OsNowUs();
    in_interrupt = 1;
    for (int i = 0; i < LEN(counters); i++) {
        RootCounter* c = &counters[i];
        unsigned bit = 1u << (IRQ_RCNT0 + i);
        if (!c->period_us || !c->irq) {
            continue;
        }
        if ((int32_t)(now - c->next_us) > MAX_CATCHUP_US) {
            CounterRestart(c, now);
        }
        while (IsDue(c->next_us, now)) {
            CounterStep(c);
            if (irq_mask & bit) {
                ServiceLine(IRQ_RCNT0 + i);
            } else {
                irq_latched |= bit;
            }
        }
        if (!triggered || (int32_t)(c->next_us - *deadline_us) < 0) {
            *deadline_us = c->next_us;
            triggered = 1;
        }
    }
    in_interrupt = 0;
    AtomicStore(
        &poll_deadline_us, (long)(triggered ? *deadline_us : now + INT32_MAX));
    ServicePending();
    KernelUnlock();
    return triggered;
}

static void IrqThread(void) {
    for (;;) {
        uint32_t deadline;
        if (!ServiceTimers(&deadline)) {
            Psyz_OsBellWait(bell_irq, PSYZ_OS_FOREVER);
            continue;
        }
        int32_t left = (int32_t)(deadline - Psyz_OsNowUs());
        if (left > 0) {
            Psyz_OsBellWait(bell_irq, (uint32_t)left);
        }
    }
}

static void StartIrqThread(void) {
    if (irq_thread_state) {
        return;
    }
    KernelInit();
    irq_thread_state = Psyz_OsThreadStart(IrqThread, "psyz_irq") ? -1 : 1;
}

void Psyz_KernelPoll(void) {
    uint32_t deadline;
    if (!irq_thread_state || OwnsKernel() ||
        !IsDue((uint32_t)AtomicLoad(&poll_deadline_us), Psyz_OsNowUs())) {
        return;
    }
    ServiceTimers(&deadline);
}

long SetRCnt(long spec, unsigned short target, long mode) {
    int i = (int)(spec & 0xFFFF);
    if (i >= 3) {
        return 0;
    }
    StartIrqThread();
    KernelLock();
    RootCounter* c = &counters[i];
    uint32_t clock = CounterClock(i, mode);
    // target * 1000000 / clock split in two steps to stay within 32 bits
    uint32_t scaled = (target ? target : 0x10000) * 15625u;
    uint32_t rem = scaled % clock * 64;
    c->clock_hz = clock;
    c->target = target ? target : 0x10000;
    c->period_us = scaled / clock * 64 + rem / clock;
    c->period_frac = rem % clock;
    if (c->period_us < MIN_PERIOD_US) {
        c->period_us = MIN_PERIOD_US;
        c->period_frac = 0;
    }
    c->ticks_per_us_q15 = clock / 15625 * 512 + clock % 15625 * 512 / 15625;
    c->irq = (mode & RCntMdINTR) != 0;
    CounterRestart(c, Psyz_OsNowUs());
    AtomicStore(&poll_deadline_us, (long)c->last_us);
    KernelUnlock();
    Psyz_OsBellRing(bell_irq);
    return 1;
}

long GetRCnt(long spec) {
    int i = (int)(spec & 0xFFFF);
    if (i >= 3) {
        return 0;
    }
    KernelLock();
    RootCounter* c = &counters[i];
    long value = 0;
    if (c->period_us) {
        uint32_t phase = (Psyz_OsNowUs() - c->last_us) % c->period_us;
        uint32_t ticks = phase * c->ticks_per_us_q15 >> 15;
        value = (long)(ticks < c->target ? ticks : c->target - 1);
    }
    KernelUnlock();
    return value;
}

long ResetRCnt(long spec) {
    int i = (int)(spec & 0xFFFF);
    if (i >= 3) {
        return 0;
    }
    KernelLock();
    CounterRestart(&counters[i], Psyz_OsNowUs());
    AtomicStore(&poll_deadline_us, (long)counters[i].last_us);
    KernelUnlock();
    Psyz_OsBellRing(bell_irq);
    return 1;
}

long StartRCnt(long spec) {
    int line = CounterLine(spec);
    if (line < 0) {
        return 0;
    }
    KernelLock();
    irq_mask |= 1u << line;
    ServicePending();
    KernelUnlock();
    Psyz_OsBellRing(bell_irq);
    return line != IRQ_VBLANK;
}

long StopRCnt(long spec) {
    int line = CounterLine(spec);
    if (line < 0) {
        return 0;
    }
    KernelLock();
    irq_mask &= ~(1u << line);
    KernelUnlock();
    return 1;
}

long ChangeClearRCnt(long t, long flag) {
    long prev;
    if (t < 0 || t >= LEN(rcnt_auto_ack)) {
        return 0;
    }
    prev = rcnt_auto_ack[t];
    rcnt_auto_ack[t] = flag;
    return prev;
}

static void VSyncDispatch(void) {
    for (int i = 0; i < LEN(vsync_callbacks); i++) {
        if (vsync_callbacks[i]) {
            vsync_callbacks[i]();
        }
    }
}

int VSyncCallbacks(int ch, void (*f)()) {
    void (*prev)(void);
    if (ch < 0 || ch >= LEN(vsync_callbacks)) {
        return 0;
    }
    KernelLock();
    prev = vsync_callbacks[ch];
    vsync_callbacks[ch] = f;
    KernelUnlock();
    return (int)(intptr_t)prev;
}

int VSyncCallback(void (*f)()) { return VSyncCallbacks(4, f); }

void* InterruptCallback(int irq, void (*cb)()) {
    void (*prev)(void);
    if (irq < 0 || irq >= IRQ_COUNT) {
        return NULL;
    }
    if (cb && irq >= IRQ_RCNT0 && irq <= IRQ_RCNT2) {
        StartIrqThread();
    }
    KernelLock();
    prev = irq_callbacks[irq];
    irq_callbacks[irq] = cb;
    if (cb) {
        irq_mask |= 1u << irq;
    } else {
        irq_mask &= ~(1u << irq);
    }
    ServicePending();
    KernelUnlock();
    Psyz_OsBellRing(bell_irq);
    return (void*)prev;
}

int SetIntrMask(int mask) {
    int prev;
    KernelLock();
    prev = (int)irq_mask;
    irq_mask = (unsigned)mask & IRQ_ALL;
    ServicePending();
    KernelUnlock();
    return prev;
}

int GetIntrMask(void) { return (int)irq_mask; }

int ResetCallback(void) {
    KernelLock();
    if (callbacks_running) {
        KernelUnlock();
        return 0;
    }
    memset(irq_callbacks, 0, sizeof(irq_callbacks));
    memset(vsync_callbacks, 0, sizeof(vsync_callbacks));
    irq_callbacks[IRQ_VBLANK] = VSyncDispatch;
    irq_mask = 1u << IRQ_VBLANK;
    irq_latched = 0;
    callbacks_running = 1;
    KernelUnlock();
    return 1;
}

int StopCallback(void) {
    KernelLock();
    if (!callbacks_running) {
        KernelUnlock();
        return 0;
    }
    irq_saved_mask = irq_mask;
    irq_mask = 0;
    irq_latched = 0;
    callbacks_running = 0;
    KernelUnlock();
    return 1;
}

int RestartCallback(void) {
    KernelLock();
    if (callbacks_running) {
        KernelUnlock();
        return 0;
    }
    irq_mask = irq_saved_mask;
    callbacks_running = 1;
    ServicePending();
    KernelUnlock();
    return 1;
}

int CheckCallback(void) { return OwnsKernel() && in_interrupt; }
