/* vax820_bi.h: VAX 8200 BI interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX820_BI_H_
#define VAX820_BI_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

extern uint32_t nexus_req[];
extern int32_t ipir;
extern int32_t rxcd_int;

void init_nexus_tab(void);
t_stat build_nexus_tab(DEVICE *dptr, DIB *dibp);
t_stat show_nexus(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX820_BI_H_ */
