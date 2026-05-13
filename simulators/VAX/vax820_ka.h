/* vax820_ka.h: VAX 8200 KA CPU interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX820_KA_H_
#define VAX820_KA_H_ 1

#include <stdint.h>

extern int32_t cur_cpu;

int32_t pcsr_rd(int32_t pa);
void pcsr_wr(int32_t pa, int32_t val, int32_t lnt);
int32_t rxcd_rd(void);
void rxcd_wr(int32_t val);

#endif /* VAX820_KA_H_ */
