/* vax_vc.h: QVSS video interfaces */
// SPDX-FileCopyrightText: 2011-2013 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX_VC_H_
#define VAX_VC_H_ 1

#include <stdint.h>

#include "sim_defs.h"

extern DEVICE vc_dev;
extern uint32_t *vc_buf;

int32_t vc_mem_rd(int32_t pa);
void vc_mem_wr(int32_t pa, int32_t val, int32_t mode);

#endif /* VAX_VC_H_ */
