/* pdp11_rom.c: Read-Only Memory.

   Copyright (c) 2018, Lars Brinkhoff

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   LARS BRINKHOFF BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdint.h>

#include "pdp11_defs.h"

t_stat rom_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32_t sw);
t_stat rom_dep (t_value val, t_addr addr, UNIT *uptr, int32_t sw);
t_stat rom_rd (int32_t *data, int32_t PA, int32_t access);
t_stat rom_reset (DEVICE *dptr);
t_stat rom_boot (int32_t u, DEVICE *dptr);
t_stat rom_set_addr (UNIT *, int32_t, const char *, void *);
t_stat rom_show_addr (FILE *, UNIT *, int32_t, const void *);
static t_stat rom_make_dib (UNIT *uptr);
t_stat rom_attach (UNIT *uptr, const char *cptr);
t_stat rom_detach (UNIT *uptr);
t_stat rom_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
t_stat rom_help_attach (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
const char *rom_description (DEVICE *dptr);

/* ROM data structures

   rom_dev       ROM device descriptor
   rom_unit      ROM unit list
*/

#define unit_base u3
#define unit_end u4

#define ROM_UNITS 4
#define ROM_UNIT_FLAGS UNIT_RO|UNIT_MUSTBUF|UNIT_BUFABLE|UNIT_ATTABLE

DIB rom_dib[ROM_UNITS];

MTAB rom_mod[] = {
    { MTAB_XTD|MTAB_VUN|MTAB_VALR, 010, "ADDRESS", "ADDRESS",
        &rom_set_addr, &rom_show_addr, NULL, "Bus address" },
    { 0 }
    };

UNIT rom_unit[ROM_UNITS];

DEVICE rom_dev = {
    "ROM", rom_unit, NULL, rom_mod,
    ROM_UNITS, 8, 9, 2, 8, 16,
    rom_ex, rom_dep, rom_reset,
    &rom_boot, &rom_attach, &rom_detach,
    &rom_dib[0], DEV_DISABLE | DEV_UBUS | DEV_QBUS,
    0, NULL, NULL, NULL,
    &rom_help, &rom_help_attach,          /* help and attach_help routines */
    NULL, &rom_description                     /* description routine */
    };

t_stat rom_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32_t sw)
{
/* Generic memory examine signature.
   This implementation does not use every parameter. */
(void) uptr;
(void) sw;

int32_t data;
t_stat r;

r = rom_rd (&data, addr, 0);
if (r != SCPE_OK)
    return r;
*vptr = (t_value)data;
return SCPE_OK;
}

t_stat rom_dep (t_value val, t_addr addr, UNIT *uptr, int32_t sw)
{
/* Generic memory deposit signature.
   This implementation does not use every parameter. */
(void) sw;

uint16_t *image = (uint16_t *)uptr->filebuf;

image[(addr - uptr->unit_base) >> 1] = val;
return SCPE_OK;
}

t_stat rom_rd (int32_t *data, int32_t PA, int32_t access)
{
/* Bus read callback signature.
   This implementation does not use every parameter. */
(void) access;

int i;
for (i = 0; i < ROM_UNITS; i++) {
    if (PA >= rom_unit[i].unit_base && PA < rom_unit[i].unit_end) {
        uint16_t *image = (uint16_t *)rom_unit[i].filebuf;
        *data = image[(PA - rom_unit[i].unit_base) >> 1];
        return SCPE_OK;
    }
}
return SCPE_NXM;
}

t_stat rom_reset (DEVICE *dptr)
{
int i;
dptr->ctxt = &rom_dib[0];
for (i = 0; i < ROM_UNITS; i++) {
    rom_unit[i].flags |= ROM_UNIT_FLAGS;
    rom_dib[i].next = &rom_dib[i + 1];
    }
rom_dib[ROM_UNITS - 1].next = NULL;
return SCPE_OK;
}

t_stat rom_boot (int32_t u, DEVICE *dptr)
{
/* Generic boot signature.
   This implementation does not use every parameter. */
(void) dptr;

cpu_set_boot (rom_unit[u].unit_base);
return SCPE_OK;
}

t_stat rom_set_addr (UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
/* Generic set modifier signature.
   This implementation does not use every parameter. */
(void) val;
(void) desc;

int32_t addr;
t_stat r;

if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
if (cptr == NULL)
    return SCPE_ARG;
addr = (int32_t) get_uint (cptr, 8, IOPAGEBASE+IOPAGEMASK, &r);
if (r != SCPE_OK)
    return r;
if (addr < IOPAGEBASE)
    return sim_messagef (SCPE_ARG, "ROM must be in I/O page, at or above 0%o\n",
                         IOPAGEBASE);
uptr->unit_base = uptr->unit_end = addr;
return SCPE_OK;
}

t_stat rom_show_addr (FILE *f, UNIT *uptr, int32_t val, const void *desc)
{
/* Generic show modifier signature.
   This implementation does not use every parameter. */
(void) val;
(void) desc;

if (uptr == NULL)
    return SCPE_IERR;
if (uptr->unit_base != uptr->unit_end)
    fprintf (f, "address=%o-%o", uptr->unit_base, uptr->unit_end - 1);
else
    fprintf (f, "address=%o", uptr->unit_base);
return SCPE_OK;
}

static t_stat rom_make_dib (UNIT *uptr)
{
DIB *dib = &rom_dib[uptr - rom_unit];

dib->ba = uptr->unit_base;
dib->lnt = uptr->capac;
dib->rd = &rom_rd;
return build_ubus_tab (&rom_dev, dib);
}

t_stat rom_attach (UNIT *uptr, const char *cptr)
{
t_stat r;

if (uptr->flags & UNIT_ATT)
    return SCPE_ALATT;
if (uptr->unit_base == 0)
    return sim_messagef (SCPE_ARG, "Set address first.\n");
sim_switches |= SWMASK ('Q');
uptr->capac = sim_fsize_name (cptr);
if (uptr->capac == 0)
    return SCPE_OPENERR;
r = attach_unit (uptr, cptr);
if (r != SCPE_OK)
    return r;
r = rom_make_dib (uptr);
if (r != SCPE_OK)
    return rom_detach (uptr);
uptr->unit_end = uptr->unit_base + uptr->capac;
return SCPE_OK;
}

/* Detach */

t_stat rom_detach (UNIT *uptr)
{
t_stat r;
DIB *dib = &rom_dib[uptr - rom_unit];

dib->rd = NULL;
r = build_ubus_tab (&rom_dev, dib);
if (r != SCPE_OK)
    return r;
uptr->unit_end = uptr->unit_base;
dib->lnt = uptr->capac = 0;
return detach_unit (uptr);
}

t_stat rom_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr)
{
/* Generic help signature.
   This implementation does not use every parameter. */
(void) dptr;
(void) uptr;
(void) flag;
(void) cptr;

fprintf (st, "ROM, Read-Only Memory\n\n");
fprintf (st, "The ROM device can be used to add ROM modules to the I/O page.\n");
fprintf (st, "First the ROM unit ADDRESS has to be set, and then the ATTACH command\n");
fprintf (st, "can be used to fill the ROM with contents.  The BOOT command is supported\n");
fprintf (st, "for starting from the ROM.\n");
return SCPE_OK;
}

t_stat rom_help_attach (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr)
{
/* Generic attach help signature.
   This implementation does not use every parameter. */
(void) dptr;
(void) uptr;
(void) flag;
(void) cptr;

fprintf (st, "The ATTACH command is used to specify the contents of a ROM unit.\n");
fprintf (st, "Any file can be used.  The file contents must be a flat binary image.\n");
fprintf (st, "The unit ADDRESS must be set first.\n");
return SCPE_OK;
}

const char *rom_description (DEVICE *dptr)
{
/* Generic device description signature.
   This implementation does not use every parameter. */
(void) dptr;

return "Read-Only Memory";
}
