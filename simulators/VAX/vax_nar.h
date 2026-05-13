/* vax_nar.h: network address ROM interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX_NAR_H_
#define VAX_NAR_H_ 1

#include <stdint.h>

int32_t nar_rd(int32_t pa);

#endif
