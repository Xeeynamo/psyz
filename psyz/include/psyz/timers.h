#ifndef PSYZ_TIMERS_H
#define PSYZ_TIMERS_H

/**
 * @file timers.h
 * @brief Time source for root counters.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Root counters follow the host clock */
#define PSYZ_CLOCK_HOST 0
/** Root counters advance only as the SPU produces samples */
#define PSYZ_CLOCK_SPU 1

void Psyz_KernelSetClock(int source);

#ifdef __cplusplus
}
#endif

#endif
