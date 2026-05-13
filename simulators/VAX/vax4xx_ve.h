/* vax4xx_ve.h: KA4xx SPX video interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX4XX_VE_H_
#define VAX4XX_VE_H_ 1

#include <stdint.h>

#include "sim_defs.h"

extern DEVICE ve_dev;

int32_t ve_rd(int32_t pa);
void ve_wr(int32_t pa, int32_t val, int32_t lnt);

#endif /* VAX4XX_VE_H_ */
