/* vax780_sbi.h: VAX 11/780 SBI interfaces */
// SPDX-FileCopyrightText: 2004-2015 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX780_SBI_H_
#define VAX780_SBI_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

extern uint32_t nexus_req[];

void init_nexus_tab(void);
t_stat build_nexus_tab(DEVICE *dptr, DIB *dibp);
t_stat show_nexus(FILE *st, UNIT *uptr, int32_t val, const void *desc);
void sbi_set_errcnf(void);

#endif /* VAX780_SBI_H_ */
