/* vax750_uba.h: VAX 11/750 Unibus adapter interfaces */
// SPDX-FileCopyrightText: 2012-2017 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX750_UBA_H_
#define VAX750_UBA_H_ 1

#include <stdint.h>

#include "sim_defs.h"

int32_t Map_ReadB(uint32_t ba, int32_t bc, uint8_t *buf);
int32_t Map_ReadW(uint32_t ba, int32_t bc, uint16_t *buf);
int32_t Map_WriteB(uint32_t ba, int32_t bc, const uint8_t *buf);
int32_t Map_WriteW(uint32_t ba, int32_t bc, const uint16_t *buf);
void init_ubus_tab(void);
t_stat build_ubus_tab(DEVICE *dptr, DIB *dibp);
void uba_eval_int(void);
int32_t uba_get_ubvector(int32_t lvl);
void uba_ioreset(void);

#endif /* VAX750_UBA_H_ */
