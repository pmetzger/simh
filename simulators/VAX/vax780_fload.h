/* vax780_fload.h: VAX 780 FLOAD command interface */
// SPDX-FileCopyrightText: 2006-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX780_FLOAD_H_
#define VAX780_FLOAD_H_ 1

#include <stdint.h>

#include "sim_defs.h"

t_stat vax780_fload(int32_t flag, const char *cptr);

#endif /* VAX780_FLOAD_H_ */
