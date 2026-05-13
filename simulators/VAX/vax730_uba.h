/* vax730_uba.h: VAX 11/730 Unibus adapter interfaces */
// SPDX-FileCopyrightText: 2010-2011 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX730_UBA_H_
#define VAX730_UBA_H_ 1

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"

int32_t Map_ReadB(uint32_t ba, int32_t bc, uint8_t *buf);
int32_t Map_ReadW(uint32_t ba, int32_t bc, uint16_t *buf);
int32_t Map_WriteB(uint32_t ba, int32_t bc, const uint8_t *buf);
int32_t Map_WriteW(uint32_t ba, int32_t bc, const uint16_t *buf);
void init_ubus_tab(void);
t_stat build_ubus_tab(DEVICE *dptr, DIB *dibp);
int32_t ubamap_rd(int32_t pa);
void ubamap_wr(int32_t pa, int32_t val, int32_t lnt);
bool uba_eval_int(int32_t lvl);
int32_t uba_get_ubvector(int32_t lvl);

#endif /* VAX730_UBA_H_ */
