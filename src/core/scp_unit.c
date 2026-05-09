/* scp_unit.c: shared SCP unit attachment helpers

   This file contains unit-to-device lookup and generic attach/detach
   operations.  These routines are reused by runtime modules and tests,
   but they are separate from the SCP command parser and control loop.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"
#include "scp.h"

/* Roll back partially initialized attach state on failure. */
static t_stat attach_err(UNIT *uptr, t_stat stat)
{
    free(uptr->filename);
    uptr->filename = NULL;
    return stat;
}

/* Open a host file and attach it to a unit using SCP's generic rules. */
t_stat attach_unit(UNIT *uptr, const char *cptr)
{
    DEVICE *dptr;
    bool open_rw = false;

    if (!(uptr->flags & UNIT_ATTABLE))
        return SCPE_NOATT;
    if ((dptr = find_dev_from_unit(uptr)) == NULL)
        return SCPE_NOATT;
    uptr->filename = (char *)calloc(CBUFSIZE, sizeof(char));
    if (uptr->filename == NULL)
        return SCPE_MEM;
    strlcpy(uptr->filename, cptr, CBUFSIZE);
    if ((sim_switches & SWMASK('R')) || ((uptr->flags & UNIT_RO) != 0)) {
        if (((uptr->flags & UNIT_ROABLE) == 0) &&
            ((uptr->flags & UNIT_RO) == 0))
            return sim_messagef(attach_err(uptr, SCPE_NORO),
                                "%s: Read Only operation not allowed\n",
                                sim_uname(uptr));
        uptr->fileref = sim_fopen(cptr, "rb");
        if (uptr->fileref == NULL)
            return sim_messagef(attach_err(uptr, SCPE_OPENERR),
                                "%s: Can't open '%s': %s\n", sim_uname(uptr),
                                cptr, strerror(errno));
        if (!(uptr->flags & UNIT_RO))
            sim_messagef(SCPE_OK, "%s: unit is read only\n", sim_uname(uptr));
        uptr->flags = uptr->flags | UNIT_RO;
    } else {
        if (sim_switches & SWMASK('N')) {
            uptr->fileref = sim_fopen(cptr, "wb+");
            if (uptr->fileref == NULL)
                return sim_messagef(attach_err(uptr, SCPE_OPENERR),
                                    "%s: Can't open '%s': %s\n",
                                    sim_uname(uptr), cptr, strerror(errno));
            sim_messagef(SCPE_OK, "%s: creating new file: %s\n",
                         sim_uname(uptr), cptr);
        } else {
            uptr->fileref = sim_fopen(cptr, "rb+");
            if (uptr->fileref == NULL) {
#if defined(EPERM)
                if ((errno == EROFS) || (errno == EACCES) || (errno == EPERM)) {
#else
                if ((errno == EROFS) || (errno == EACCES)) {
#endif
                    if ((uptr->flags & UNIT_ROABLE) == 0)
                        return sim_messagef(
                            attach_err(uptr, SCPE_NORO),
                            "%s: Read Only operation not allowed\n",
                            sim_uname(uptr));
                    uptr->fileref = sim_fopen(cptr, "rb");
                    if (uptr->fileref == NULL)
                        return sim_messagef(attach_err(uptr, SCPE_OPENERR),
                                            "%s: Can't open '%s': %s\n",
                                            sim_uname(uptr), cptr,
                                            strerror(errno));
                    uptr->flags = uptr->flags | UNIT_RO;
                    sim_messagef(SCPE_OK, "%s: unit is read only\n",
                                 sim_uname(uptr));
                } else {
                    if (sim_switches & SWMASK('E'))
                        return sim_messagef(attach_err(uptr, SCPE_OPENERR),
                                            "%s: Can't open '%s': %s\n",
                                            sim_uname(uptr), cptr,
                                            strerror(errno));
                    uptr->fileref = sim_fopen(cptr, "wb+");
                    if (uptr->fileref == NULL)
                        return sim_messagef(attach_err(uptr, SCPE_OPENERR),
                                            "%s: Can't open '%s': %s\n",
                                            sim_uname(uptr), cptr,
                                            strerror(errno));
                    sim_messagef(SCPE_OK, "%s: creating new file\n",
                                 sim_uname(uptr));
                }
            } else
                open_rw = true;
        }
    }
    if (uptr->flags & UNIT_BUFABLE) {
        uint32_t cap = ((uint32_t)uptr->capac) / dptr->aincr;

        uptr->filebuf2 = calloc(cap, scp_device_data_size_bytes(dptr));
        if (uptr->filebuf2 == NULL)
            return attach_err(uptr, SCPE_MEM);
        if (uptr->flags & UNIT_MUSTBUF) {
            uptr->filebuf = calloc(cap, scp_device_data_size_bytes(dptr));
            if (uptr->filebuf == NULL) {
                free(uptr->filebuf);
                uptr->filebuf = NULL;
                free(uptr->filebuf2);
                uptr->filebuf2 = NULL;
                return attach_err(uptr, SCPE_MEM);
            }
        }
        sim_messagef(SCPE_OK, "%s: buffering file in memory\n",
                     sim_uname(uptr));
        uptr->hwmark =
            (uint32_t)sim_fread(uptr->filebuf, scp_device_data_size_bytes(dptr),
                              cap, uptr->fileref);
        memcpy(uptr->filebuf2, uptr->filebuf,
               cap * scp_device_data_size_bytes(dptr));
        uptr->flags = uptr->flags | UNIT_BUF;
    }
    uptr->flags = uptr->flags | UNIT_ATT;
    uptr->pos = 0;
    if (open_rw && (sim_switches & SWMASK('A')) && (uptr->flags & UNIT_SEQ) &&
        (!(uptr->flags & UNIT_MUSTBUF)) &&
        (0 == sim_fseek(uptr->fileref, 0, SEEK_END)))
        uptr->pos = (t_addr)sim_ftell(uptr->fileref);
    return SCPE_OK;
}

/* Flush and detach a unit using SCP's generic buffered-file rules. */
t_stat detach_unit(UNIT *uptr)
{
    DEVICE *dptr;

    if (uptr == NULL)
        return SCPE_IERR;
    if (!(uptr->flags & UNIT_ATTABLE))
        return SCPE_NOATT;
    if (!(uptr->flags & UNIT_ATT)) {
        if (sim_switches & SIM_SW_REST)
            return SCPE_OK;
        else
            return SCPE_UNATT;
    }
    if ((dptr = find_dev_from_unit(uptr)) == NULL)
        return SCPE_OK;
    if ((uptr->flags & UNIT_BUF) && (uptr->filebuf)) {
        uint32_t cap = (uptr->hwmark + dptr->aincr - 1) / dptr->aincr;

        if (((uptr->flags & UNIT_RO) == 0) &&
            (memcmp(uptr->filebuf, uptr->filebuf2,
                    (size_t)(scp_device_data_size_bytes(dptr) *
                             (uptr->capac / dptr->aincr))) != 0)) {
            sim_messagef(SCPE_OK, "%s: writing buffer to file: %s\n",
                         sim_uname(uptr), uptr->filename);
            rewind(uptr->fileref);
            sim_fwrite(uptr->filebuf, scp_device_data_size_bytes(dptr), cap,
                       uptr->fileref);
            if (ferror(uptr->fileref))
                sim_printf("%s: I/O error - %s", sim_uname(uptr),
                           strerror(errno));
        }
        if (uptr->flags & UNIT_MUSTBUF) {
            free(uptr->filebuf);
            uptr->filebuf = NULL;
            free(uptr->filebuf2);
            uptr->filebuf2 = NULL;
        }
        uptr->flags = uptr->flags & ~UNIT_BUF;
    }
    uptr->flags =
        uptr->flags & ~(UNIT_ATT | ((uptr->flags & UNIT_ROABLE) ? UNIT_RO : 0));
    free(uptr->filename);
    uptr->filename = NULL;
    if (uptr->fileref) {
        if (fclose(uptr->fileref) == EOF) {
            uptr->fileref = NULL;
            return SCPE_IOERR;
        }
        uptr->fileref = NULL;
    }
    return SCPE_OK;
}

/* Locate the owning device for a unit and cache it on the unit. */
DEVICE *find_dev_from_unit(UNIT *uptr)
{
    DEVICE *dptr;
    uint32_t i, j;

    if (uptr == NULL)
        return NULL;
    if (uptr->dptr)
        return uptr->dptr;
    for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
        for (j = 0; j < dptr->numunits; j++) {
            if (uptr == (dptr->units + j)) {
                uptr->dptr = dptr;
                return dptr;
            }
        }
    }
    for (i = 0; i < sim_internal_device_count; i++) {
        dptr = sim_internal_devices[i];
        for (j = 0; j < dptr->numunits; j++) {
            if (uptr == (dptr->units + j)) {
                uptr->dptr = dptr;
                return dptr;
            }
        }
    }
    return NULL;
}
