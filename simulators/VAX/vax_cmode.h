/* vax_cmode.h: VAX compatibility-mode interfaces */
// SPDX-FileCopyrightText: 2004-2016 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_CMODE_H_
#define VAX_CMODE_H_ 1

#include "vax_defs.h"

int32_t op_cmode(int32_t cc);
bool BadCmPSL(int32_t newpsl);

#endif /* VAX_CMODE_H_ */
