/* sim_host_path_internal.h: Internal host path-name test hooks

   Keep private seams for testing host path discovery out of the public
   sim_host_path.h interface. Production code should include sim_host_path.h.
*/
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_HOST_PATH_INTERNAL_H_
#define SIM_HOST_PATH_INTERNAL_H_ 1

#include "sim_host_path.h"

#if defined(_WIN32)
typedef unsigned long (*sim_host_win32_temp_path_fn)(unsigned long, char *);

/* Install Windows temporary-directory API test hooks. */
void sim_host_path_set_test_win32_temp_hooks(
    sim_host_win32_temp_path_fn get_temp_path2,
    sim_host_win32_temp_path_fn get_temp_path);

/* Restore default host-backed path hooks. */
void sim_host_path_reset_test_hooks(void);
#endif

#endif
