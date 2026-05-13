/* vax_sysdev_internal.h: internal VAX 3900 system-device test seams */
// SPDX-FileCopyrightText: 1998-2019 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_SYSDEV_INTERNAL_H_
#define VAX_SYSDEV_INTERNAL_H_ 1

#include <stdint.h>

extern uint32_t *rom;
extern uint32_t *nvr;
extern int32_t cmctl_reg[];
extern int32_t cdg_dat[];
extern uint32_t tmr_tnir[];

#endif /* VAX_SYSDEV_INTERNAL_H_ */
