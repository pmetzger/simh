/* vax420_sysdev_internal.h: MicroVAX 3100 system-device test seams */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX420_SYSDEV_INTERNAL_H_
#define VAX420_SYSDEV_INTERNAL_H_ 1

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"

extern uint32_t *ddb;
extern int32_t int_mask;
extern bool tmr_inst;
extern uint32_t tmr_tir;
extern UNIT sysd_unit;

#endif /* VAX420_SYSDEV_INTERNAL_H_ */
