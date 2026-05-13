/* vax780_mem.h: VAX 11/780 memory interfaces */
// SPDX-FileCopyrightText: 2004-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX780_MEM_H_
#define VAX780_MEM_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

t_stat cpu_show_memory(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX780_MEM_H_ */
