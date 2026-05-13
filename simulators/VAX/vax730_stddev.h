/* vax730_stddev.h: VAX 11/730 standard device interfaces */
// SPDX-FileCopyrightText: 2010-2011 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX730_STDDEV_H_
#define VAX730_STDDEV_H_ 1

#include <stdint.h>

extern int32_t csi_int;
extern int32_t cso_int;
extern int32_t tmr_int;
extern int32_t tti_int;
extern int32_t tto_int;

int32_t csrd_rd(void);
int32_t csrs_rd(void);
void csrs_wr(int32_t data);
void cstd_wr(int32_t data);
int32_t csts_rd(void);
void csts_wr(int32_t data);
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

#endif /* VAX730_STDDEV_H_ */
