/* scp_internal.h: internal SCP helpers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SCP_INTERNAL_H_
#define SCP_INTERNAL_H_

#include "sim_defs.h"

/* Process commands read from stdin or a simulator-supplied command reader. */
t_stat process_stdin_commands(t_stat stat, char *argv[], bool do_called);

#endif
