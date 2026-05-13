/* vax750_cmi.h: VAX 11/750 CMI interfaces */
// SPDX-FileCopyrightText: 2010-2011 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX750_CMI_H_
#define VAX750_CMI_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

extern uint32_t nexus_req[];

void cmi_set_tmo(void);
t_stat show_nexus(FILE *st, UNIT *uptr, int32_t val, const void *desc);
t_stat vax750_set_bootdev(UNIT *uptr, int32_t val, const char *cptr,
                          void *desc);
t_stat vax750_show_bootdev(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX750_CMI_H_ */
