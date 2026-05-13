/* vax_va.h: QDSS video interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX_VA_H_
#define VAX_VA_H_ 1

#include <stdint.h>

#include "sim_defs.h"

extern DEVICE va_dev;
extern uint32_t va_addr;

int32_t va_mem_rd(int32_t pa);
void va_mem_wr(int32_t pa, int32_t val, int32_t lnt);

#endif /* VAX_VA_H_ */
