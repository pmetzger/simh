/* scp_context.c: shared SCP simulator context helpers

   This file holds the small set of SCP helpers that interpret and
   manage simulator context: default device pointers, internal device
   registration, and device/unit naming and lookup.  These routines are
   used by the command processor and by shared support code, but they are
   separate from command parsing and the top-level control loop.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include "sim_defs.h"
#include "scp.h"

/* Current/default simulator context and the list of registered internal
   devices that participate in SCP lookup. */
DEVICE *sim_dfdev = NULL;
UNIT *sim_dfunit = NULL;
DEVICE **sim_internal_devices = NULL;
uint32 sim_internal_device_count = 0;

/* Return the display name for a device, preferring any assigned alias. */
const char *sim_dname(DEVICE *dptr)
{
    return (dptr ? (dptr->lname ? dptr->lname : dptr->name) : "");
}

/* Cache and return the display name for a unit. */
const char *sim_uname(UNIT *uptr)
{
    DEVICE *d;
    char uname[CBUFSIZE];

    if (!uptr)
        return "";
    if (uptr->uname)
        return uptr->uname;
    d = find_dev_from_unit(uptr);
    if (!d)
        return "";
    if (d->numunits == 1)
        sprintf(uname, "%s", sim_dname(d));
    else
        sprintf(uname, "%s%d", sim_dname(d), (int)(uptr - d->units));
    return sim_set_uname(uptr, uname);
}

/* Replace the cached display name for a unit. */
const char *sim_set_uname(UNIT *uptr, const char *uname)
{
    free(uptr->uname);
    return uptr->uname = strcpy((char *)malloc(1 + strlen(uname)), uname);
}

/* Find the named device in either simulator or internal device tables. */
DEVICE *find_dev(const char *cptr)
{
    int32 i;
    DEVICE *dptr;

    if (cptr == NULL)
        return NULL;
    for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
        if ((strcmp(cptr, dptr->name) == 0) ||
            (dptr->lname && (strcmp(cptr, dptr->lname) == 0)))
            return dptr;
    }
    for (i = 0; sim_internal_device_count && (dptr = sim_internal_devices[i]);
         ++i) {
        if ((strcmp(cptr, dptr->name) == 0) ||
            (dptr->lname && (strcmp(cptr, dptr->lname) == 0)))
            return dptr;
    }
    return NULL;
}

/* Find the named unit, accepting both device and fully qualified names. */
DEVICE *find_unit(const char *cptr, UNIT **uptr)
{
    uint32 i, u;
    const char *nptr;
    const char *tptr;
    t_stat r;
    DEVICE *dptr;

    if (uptr == NULL)
        return NULL;
    *uptr = NULL;
    if ((dptr = find_dev(cptr))) {
        if (qdisable(dptr))
            return NULL;
        *uptr = dptr->units;
        return dptr;
    }

    for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
        if (qdisable(dptr))
            continue;
        if (dptr->numunits && (((nptr = dptr->name) &&
                                (strncmp(cptr, nptr, strlen(nptr)) == 0)) ||
                               ((nptr = dptr->lname) &&
                                (strncmp(cptr, nptr, strlen(nptr)) == 0)))) {
            tptr = cptr + strlen(nptr);
            if (sim_isdigit(*tptr)) {
                if (qdisable(dptr))
                    return NULL;
                u = (uint32)get_uint(tptr, 10, dptr->numunits - 1, &r);
                if (r != SCPE_OK)
                    *uptr = NULL;
                else
                    *uptr = dptr->units + u;
                return dptr;
            }
        }
        for (u = 0; u < dptr->numunits; u++) {
            if (0 == strcmp(cptr, sim_uname(&dptr->units[u]))) {
                *uptr = &dptr->units[u];
                return dptr;
            }
        }
    }
    return NULL;
}

/* Add a device to the internal device list if it is not already present. */
t_stat sim_register_internal_device(DEVICE *dptr)
{
    uint32 i;

    for (i = 0; i < sim_internal_device_count; i++)
        if (sim_internal_devices[i] == dptr)
            return SCPE_OK;
    for (i = 0; (sim_devices[i] != NULL); i++)
        if (sim_devices[i] == dptr)
            return SCPE_OK;
    ++sim_internal_device_count;
    sim_internal_devices = (DEVICE **)realloc(
        sim_internal_devices,
        (sim_internal_device_count + 1) * sizeof(*sim_internal_devices));
    sim_internal_devices[sim_internal_device_count - 1] = dptr;
    sim_internal_devices[sim_internal_device_count] = NULL;
    return SCPE_OK;
}

/* Return whether a device is currently disabled. */
bool qdisable(DEVICE *dptr)
{
    return ((dptr->flags & DEV_DIS) != 0);
}
