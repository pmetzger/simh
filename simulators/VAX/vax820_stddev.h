/* vax820_stddev.h: VAX 8200 standard device interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX820_STDDEV_H_
#define VAX820_STDDEV_H_ 1

#include <stdint.h>

extern int32_t fl_int;
extern int32_t tmr_int;
extern int32_t tti_int;
extern int32_t tto_int;

int32_t fl_rd(int32_t pa);
void fl_wr(int32_t pa, int32_t val, int32_t lnt);
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

#endif /* VAX820_STDDEV_H_ */
