/* vax4xx_vc.h: KA4xx VC monochrome video interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX4XX_VC_H_
#define VAX4XX_VC_H_ 1

#include <stdint.h>

#include "sim_defs.h"

extern DEVICE vc_dev;
extern uint32_t vc_last_org;
extern uint32_t vc_org;
extern uint32_t vc_sel;

int32_t vc_mem_rd(int32_t pa);
void vc_mem_wr(int32_t pa, int32_t val, int32_t lnt);
void vc_wr(int32_t pa, int32_t val, int32_t lnt);

#endif /* VAX4XX_VC_H_ */
