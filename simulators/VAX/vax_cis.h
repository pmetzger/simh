/* vax_cis.h: VAX commercial instruction set interface */
// SPDX-FileCopyrightText: 2004-2016 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_CIS_H_
#define VAX_CIS_H_ 1

#include <stdint.h>

int32_t op_cis(uint32_t *opnd, int32_t cc, int32_t opc, int32_t acc);

#endif /* VAX_CIS_H_ */
