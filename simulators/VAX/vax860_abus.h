/* vax860_abus.h: VAX 8600 A-Bus interfaces */
// SPDX-FileCopyrightText: 2011-2012 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX860_ABUS_H_
#define VAX860_ABUS_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

extern uint32_t nexus_req[];

t_stat cpu_show_memory(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX860_ABUS_H_ */
