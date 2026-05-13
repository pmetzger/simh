/* vax_syscm.h: PDP-11 compatibility-mode symbolic interfaces */
// SPDX-FileCopyrightText: 1993-2012 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_SYSCM_H_
#define VAX_SYSCM_H_ 1

#include "vax_defs.h"

t_stat fprint_sym_cm(FILE *of, t_addr addr, t_value *bytes, int32_t sw);
t_stat parse_sym_cm(const char *cptr, t_addr addr, t_value *bytes, int32_t sw);

#endif /* VAX_SYSCM_H_ */
