/* vax4xx_defs.h: KA4xx-family shared definitions */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-FileCopyrightText: 1998-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX4XX_DEFS_H_
#define VAX4XX_DEFS_H_ 1

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"
#include "vax4xx_dz.h"
#include "vax4xx_rd.h"
#include "vax4xx_stddev.h"
#include "vax4xx_va.h"
#include "vax4xx_vc.h"
#include "vax4xx_ve.h"
#include "vax_nar.h"

/* KA4xx standard device state shared with system device dispatch. */
extern int32_t ka_cfgtst;

#endif
