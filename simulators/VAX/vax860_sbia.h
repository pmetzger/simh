/* vax860_sbia.h: VAX 8600 SBIA interfaces */
// SPDX-FileCopyrightText: 2011-2012 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX860_SBIA_H_
#define VAX860_SBIA_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

extern uint32_t sbi_er;

int32_t sbia_rd(int32_t pa, int32_t lnt);
void sbia_wr(int32_t pa, int32_t val, int32_t lnt);
t_stat sbi_rd(int32_t pa, int32_t *val, int32_t lnt);
t_stat sbi_wr(int32_t pa, int32_t val, int32_t lnt);
void sbi_set_tmo(int32_t pa);
void sbi_set_errcnf(void);
t_stat show_nexus(FILE *st, UNIT *uptr, int32_t val, const void *desc);
void init_nexus_tab(void);
t_stat build_nexus_tab(DEVICE *dptr, DIB *dibp);

#endif /* VAX860_SBIA_H_ */
