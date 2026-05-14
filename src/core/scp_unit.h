/* scp_unit.h: shared SCP unit attachment helpers

   These helpers were extracted from scp.c because runtime modules and
   tests need unit attachment logic without depending on the entire SCP
   command processor.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#ifndef SIM_SCP_UNIT_H_
#define SIM_SCP_UNIT_H_ 1

#include "sim_defs.h"

/* Open a host file and attach it to a unit using generic SCP rules. */
t_stat attach_unit(UNIT *uptr, const char *cptr);

/* Flush and detach a unit using generic SCP buffered-file rules. */
t_stat detach_unit(UNIT *uptr);

/* Find the owning device for a unit and cache it on the unit. */
DEVICE *find_dev_from_unit(UNIT *uptr);

#endif
