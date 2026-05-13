/* vax820_mem.h: VAX 8200 memory interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX820_MEM_H_
#define VAX820_MEM_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

t_stat cpu_show_memory(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX820_MEM_H_ */
