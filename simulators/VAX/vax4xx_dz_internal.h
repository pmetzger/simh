/* vax4xx_dz_internal.h: KA4xx DZ terminal multiplexer test seams */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-FileCopyrightText: 2001-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX4XX_DZ_INTERNAL_H_
#define VAX4XX_DZ_INTERNAL_H_ 1

#include <stdbool.h>
#include <stdint.h>

#include "sim_tmxr.h"
#include "vax_defs.h"

extern DEVICE dz_dev;
extern TMXR dz_desc;
extern TMLN *dz_ldsc;
extern UNIT dz_unit[];
extern uint16_t dz_csr;
extern uint16_t dz_silo[];
extern uint16_t dz_scnt;
extern uint16_t dz_tcr;
extern uint32_t dz_func[];
extern int32_t dz_lnorder[];

t_stat dz_clear(bool flag);
uint16_t dz_getc(void);
t_stat dz_reset(DEVICE *dptr);
void dz_update_rcvi(void);

#endif /* VAX4XX_DZ_INTERNAL_H_ */
