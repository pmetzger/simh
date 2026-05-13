/* vax730_rb.h: RB730 disk controller interfaces */
// SPDX-FileCopyrightText: 2010-2011 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX730_RB_H_
#define VAX730_RB_H_ 1

#include <stdint.h>

#include "sim_defs.h"

extern DIB rb_dib;

t_stat rb_rd32(int32_t *data, int32_t pa, int32_t access);
t_stat rb_wr32(int32_t data, int32_t pa, int32_t access);

#endif /* VAX730_RB_H_ */
