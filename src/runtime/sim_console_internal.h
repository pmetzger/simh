/* sim_console_internal.h: internal console helpers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_CONSOLE_INTERNAL_H_
#define SIM_CONSOLE_INTERNAL_H_

#include <stdbool.h>

#include "sim_defs.h"

/*
 * Classify the result of a nonblocking terminal read.
 *
 * POSIX terminals configured with VMIN=0 and VTIME=0 may return zero
 * bytes when no input is available.  A zero-byte read is therefore only
 * treated as a lost console when readiness polling first reported that
 * input was available.
 */
t_stat sim_console_classify_terminal_read(bool input_ready, int read_status,
                                          int read_errno);

#endif
