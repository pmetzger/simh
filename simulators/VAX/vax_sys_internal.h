/* vax_sys_internal.h: internal VAX symbolic helper interfaces */
// SPDX-FileCopyrightText: 1998-2017 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_SYS_INTERNAL_H_
#define VAX_SYS_INTERNAL_H_ 1

#include <stdint.h>

#include "vax_defs.h"

t_stat fprint_sym_m(FILE *of, uint32_t addr, t_value *val);
t_stat parse_sym_m(const char *cptr, uint32_t addr, t_value *val);

#endif
