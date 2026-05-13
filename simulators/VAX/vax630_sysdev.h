/* vax630_sysdev.h: MicroVAX II system device interfaces */
// SPDX-FileCopyrightText: 2009-2012 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX630_SYSDEV_H_
#define VAX630_SYSDEV_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

extern int32_t ka_mser;

int32_t ReadRegU(uint32_t pa, int32_t lnt);
void WriteRegU(uint32_t pa, int32_t val, int32_t lnt);
t_stat cpu_show_memory(FILE *st, UNIT *uptr, int32_t val, const void *desc);
int32_t rom_rd(int32_t pa, int32_t lnt);
t_stat sysd_set_diag(UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat sysd_show_diag(FILE *st, UNIT *uptr, int32_t val, const void *desc);
t_stat sysd_set_halt(UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat sysd_show_halt(FILE *st, UNIT *uptr, int32_t val, const void *desc);
int32_t sysd_hlt_enb(void);
t_stat sysd_show_leds(FILE *st, UNIT *uptr, int32_t val, const void *desc);

#endif /* VAX630_SYSDEV_H_ */
