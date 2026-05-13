/* vax610_mem.h: MicroVAX I memory interfaces */
// SPDX-FileCopyrightText: 2011-2012 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX610_MEM_H_
#define VAX610_MEM_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

t_stat cpu_show_memory(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX610_MEM_H_ */
