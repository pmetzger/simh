/* vax780_stddev.h: VAX 11/780 standard device interfaces */
// SPDX-FileCopyrightText: 1998-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX780_STDDEV_H_
#define VAX780_STDDEV_H_ 1

#include <stdint.h>

#include "sim_defs.h"

extern int32_t tmr_int;
extern int32_t tti_int;
extern int32_t tto_int;
extern UNIT fl_unit;

int32_t iccs_rd(void);
void iccs_wr(int32_t data);
int32_t icr_rd(void);
int32_t nicr_rd(void);
void nicr_wr(int32_t data);
int32_t rxcs_rd(void);
void rxcs_wr(int32_t data);
int32_t rxdb_rd(void);
int32_t todr_rd(void);
void todr_wr(int32_t data);
int32_t txcs_rd(void);
void txcs_wr(int32_t data);
void txdb_wr(int32_t data);

#endif /* VAX780_STDDEV_H_ */
