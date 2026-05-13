/* vax730_sys.h: VAX 11/730 system interfaces */
// SPDX-FileCopyrightText: 2010-2011 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX730_SYS_H_
#define VAX730_SYS_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

t_stat show_nexus(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX730_SYS_H_ */
