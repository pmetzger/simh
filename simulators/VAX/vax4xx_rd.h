/* vax4xx_rd.h: KA4xx RD disk controller interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX4XX_RD_H_
#define VAX4XX_RD_H_ 1

#include <stdint.h>

#include "sim_defs.h"

extern DEVICE rd_dev;

int32_t rd_rd(int32_t pa);
void rd_wr(int32_t pa, int32_t val, int32_t lnt);

#endif /* VAX4XX_RD_H_ */
