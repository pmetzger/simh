/* sim_time_internal.h: Internal time helpers for ZIMH */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_TIME_INTERNAL_H_
#define SIM_TIME_INTERNAL_H_ 1

#include <stdint.h>
#include <time.h>

/* Convert Windows FILETIME 100ns ticks to a Unix timespec. */
void sim_time_windows_filetime_to_timespec(uint64_t filetime_ticks,
                                           struct timespec *tp);

#endif
