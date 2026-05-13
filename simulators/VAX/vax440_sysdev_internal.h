/* vax440_sysdev_internal.h: MicroVAX 4000 system-device test seams */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX440_SYSDEV_INTERNAL_H_
#define VAX440_SYSDEV_INTERNAL_H_ 1

#include "vax_defs.h"

extern int32_t CADR;
extern int32_t SCCR;
extern int32_t conisp;
extern int32_t conpc;
extern int32_t conpsl;
extern int32_t int_mask;
extern int32_t ka_hltcod;
extern int32_t mem_cnfg;
extern DEVICE sysd_dev;

t_stat sysd_reset(DEVICE *dptr);

#endif /* VAX440_SYSDEV_INTERNAL_H_ */
