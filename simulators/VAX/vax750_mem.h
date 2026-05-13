/* vax750_mem.h: VAX 11/750 memory controller interfaces */
// SPDX-FileCopyrightText: 2010-2012 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX750_MEM_H_
#define VAX750_MEM_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

extern uint32_t rom[];

t_stat cpu_show_memory(FILE *st, UNIT *uptr, int32_t val, const void *desc);
t_stat mctl_populate_rom(const char *rom_filename);

#endif /* VAX750_MEM_H_ */
