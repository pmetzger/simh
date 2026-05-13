/* vax440_sysdev.h: MicroVAX 4000 system-device interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-FileCopyrightText: 1998-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX440_SYSDEV_H_
#define VAX440_SYSDEV_H_ 1

#include <stdint.h>

#include "sim_defs.h"

int32_t Map_ReadB(uint32_t ba, int32_t bc, uint8_t *buf);
int32_t Map_ReadW(uint32_t ba, int32_t bc, uint16_t *buf);
int32_t Map_WriteB(uint32_t ba, int32_t bc, uint8_t *buf);
int32_t Map_WriteW(uint32_t ba, int32_t bc, uint16_t *buf);

int32_t ReadRegU(uint32_t pa, int32_t lnt);
void WriteRegU(uint32_t pa, int32_t val, int32_t lnt);

t_stat auto_config(const char *name, int32_t nctrl);

#endif /* VAX440_SYSDEV_H_ */
