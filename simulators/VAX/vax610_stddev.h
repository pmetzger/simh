/* vax610_stddev.h: MicroVAX I standard device interfaces */
// SPDX-FileCopyrightText: 2011-2012 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX610_STDDEV_H_
#define VAX610_STDDEV_H_ 1

#include <stdint.h>

int32_t iccs_rd(void);
void iccs_wr(int32_t data);
int32_t rxcs_rd(void);
void rxcs_wr(int32_t data);
int32_t rxdb_rd(void);
int32_t txcs_rd(void);
void txcs_wr(int32_t data);
void txdb_wr(int32_t data);

#endif /* VAX610_STDDEV_H_ */
