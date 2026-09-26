#include "libspu_private.h"

long SpuSetNoiseClock(long n_clock) {
    long clamped;
    u16 controlRegister;

    if (n_clock < 0) {
        clamped = 0;
    } else if (n_clock > 0x3F) {
        clamped = 0x3F;
    } else {
        clamped = n_clock;
    }

    controlRegister = SPUR(spucnt);
    controlRegister &= ~0x3F00;
    controlRegister |= ((clamped & 0x3F) << 8);
    SPUW(spucnt, controlRegister);

    return clamped;
}
