/* vax4xx_dz.h: KA4xx DZ terminal multiplexor interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-FileCopyrightText: 2001-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX4XX_DZ_H_
#define VAX4XX_DZ_H_ 1

#include <stdint.h>

int32_t dz_rd(int32_t pa);
void dz_wr(int32_t pa, int32_t val, int32_t lnt);

#endif /* VAX4XX_DZ_H_ */
