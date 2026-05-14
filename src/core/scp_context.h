/* scp_context.h: shared SCP simulator context helpers

   These declarations cover the small slice of SCP state and helper
   logic that describes the active simulator context: default device and
   unit pointers, internal device registration, and device/unit naming
   and lookup helpers.  Keeping these declarations together makes the
   boundary between generic SCP helpers and the command loop easier to
   follow.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#ifndef SIM_SCP_CONTEXT_H_
#define SIM_SCP_CONTEXT_H_ 1

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"

/* Default device selected for SCP commands that operate on a device. */
extern DEVICE *sim_dfdev;

/* Default unit selected for SCP commands that operate on a unit. */
extern UNIT *sim_dfunit;

/* Extra devices registered for SCP lookup outside the main simulator set. */
extern DEVICE **sim_internal_devices;

/* Number of devices currently registered in sim_internal_devices. */
extern uint32_t sim_internal_device_count;

/* Return the display name for a device, preferring any assigned alias. */
const char *sim_dname(DEVICE *dptr);

/* Return a cached display name for a unit, generating it if needed. */
const char *sim_uname(UNIT *uptr);

/* Replace the cached display name stored on a unit. */
const char *sim_set_uname(UNIT *uptr, const char *uname);

/* Find a device by physical or logical name. */
DEVICE *find_dev(const char *cptr);

/* Find a device and unit by SCP-style device or unit name. */
DEVICE *find_unit(const char *cptr, UNIT **uptr);

/* Register an internal helper device so SCP lookup can see it. */
t_stat sim_register_internal_device(DEVICE *dptr);

/* Return whether a device is currently disabled. */
bool qdisable(DEVICE *dptr);

#endif
