/* vax4xx_va.h: KA4xx VA color video interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX4XX_VA_H_
#define VAX4XX_VA_H_ 1

#include <stdint.h>

#include "sim_defs.h"

extern DEVICE va_dev;

int32_t va_rd(int32_t pa);
void va_wr(int32_t pa, int32_t val, int32_t lnt);

#endif /* VAX4XX_VA_H_ */
