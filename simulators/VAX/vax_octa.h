/* vax_octa.h: VAX octaword and H-floating instruction interface */
// SPDX-FileCopyrightText: 2004-2011 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_OCTA_H_
#define VAX_OCTA_H_ 1

#include "vax_defs.h"

int32_t op_octa(uint32_t *opnd, int32_t cc, int32_t opc, int32_t acc,
                int32_t spec, int32_t va, InstHistory *hst);

#endif /* VAX_OCTA_H_ */
