/* vax610_sysdev.h: MicroVAX I system device interfaces */
// SPDX-FileCopyrightText: 2011-2012 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX610_SYSDEV_H_
#define VAX610_SYSDEV_H_ 1

#include <stdint.h>
#include <stdio.h>

#include "sim_defs.h"

extern int32_t sys_model;

int32_t ReadRegU(uint32_t pa, int32_t lnt);
void WriteRegU(uint32_t pa, int32_t val, int32_t lnt);
int32_t sysd_hlt_enb(void);
t_stat vax610_set_instruction_set(UNIT *uptr, int32_t val, const char *cptr,
                                  void *desc);

#endif /* VAX610_SYSDEV_H_ */
