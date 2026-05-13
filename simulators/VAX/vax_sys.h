/* vax_sys.h: VAX simulator interface declarations */
// SPDX-FileCopyrightText: 1998-2017 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_SYS_H_
#define VAX_SYS_H_ 1

#include "vax_defs.h"

extern char const *const opcode[];
extern const uint16_t drom[NUM_INST][MAX_SPEC + 1];

#endif /* VAX_SYS_H_ */
