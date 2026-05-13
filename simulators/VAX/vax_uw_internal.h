/* vax_uw_internal.h: VAXstation 100 Unibus window test seams */
// SPDX-FileCopyrightText: 2023 Lars Brinkhoff
// SPDX-License-Identifier: X11

#ifndef VAX_UW_INTERNAL_H_
#define VAX_UW_INTERNAL_H_ 1

#include <stdint.h>

#include "vax_defs.h"

extern uint16_t uw_csr[];

t_stat uw_wr(int32_t data, int32_t pa, int32_t access);

#endif /* VAX_UW_INTERNAL_H_ */
