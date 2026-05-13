/* vax730_mem.h: VAX 11/730 memory interfaces */
// SPDX-FileCopyrightText: 2010-2011 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX730_MEM_H_
#define VAX730_MEM_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

t_stat cpu_show_memory(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX730_MEM_H_ */
