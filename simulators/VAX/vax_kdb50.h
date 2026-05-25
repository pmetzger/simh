/* vax_kdb50.h: VAXBI KDB50 disk controller interfaces */
// SPDX-FileCopyrightText: 2026 The ZIMH contributors
// SPDX-License-Identifier: MIT

#ifndef VAX_KDB50_H_
#define VAX_KDB50_H_ 1

#include "sim_defs.h"

extern DEVICE kdb50_dev;

int32_t kdb50_get_vector(int32_t lvl);

#endif /* VAX_KDB50_H_ */
