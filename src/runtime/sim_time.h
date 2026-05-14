/* sim_time.h: Time API wrappers for ZIMH */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_TIME_H_
#define SIM_TIME_H_ 1

#include <time.h>

#include "sim_types.h"

#if defined(_WIN32) && !defined(CLOCK_REALTIME)
#define CLOCK_REALTIME 1
#endif

/* Test hook for the clock source used by sim_clock_gettime(). */
typedef int (*sim_clock_gettime_fn)(int clock_id, struct timespec *tp);

/* Test hook for the sleep primitive used by sim_nanosleep(). */
typedef int (*sim_nanosleep_fn)(const struct timespec *req,
                                struct timespec *rem);

/* Thin wrapper around clock_gettime(). */
int sim_clock_gettime(int clock_id, struct timespec *tp);

/* Thin wrapper around nanosleep(). */
int sim_nanosleep(const struct timespec *req, struct timespec *rem);

/* Return the current CLOCK_REALTIME seconds value. */
time_t sim_time(time_t *now);

/* Sleep for a whole number of seconds through sim_nanosleep(). */
void sim_sleep(uint_t sec);

/* Sleep for a millisecond interval through sim_nanosleep(). */
void sim_sleep_msec(uint_t msec);

/* Install test hooks for deterministic clock and sleep behavior. */
void sim_time_set_test_hooks(sim_clock_gettime_fn clock_hook,
                             sim_nanosleep_fn sleep_hook);

/* Restore the default host-backed clock and sleep hooks. */
void sim_time_reset_test_hooks(void);

#endif
