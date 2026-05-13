/* vax610_sysdev.c: MicroVAX I system-specific logic

   Copyright (c) 2011-2012, Matt Burke
   This module incorporates code from SimH, Copyright (c) 1998-2008, Robert M Supnik

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
   THE AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name(s) of the author(s) shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author(s).

   This module contains the MicroVAX I system-specific registers and devices.

   sysd         system devices

   15-Feb-2012  MB      First Version
*/

#include <stdbool.h>
#include <stdint.h>

#include "vax_defs.h"
#include "vax_cpu.h"
#include "vax_cpu1.h"
#include "vax_lk.h"
#include "vax610_sysdev.h"
#include "vax_vs.h"

#ifdef DONT_USE_INTERNAL_ROM
#define BOOT_CODE_FILENAME "ka610.bin"
#else /* !DONT_USE_INTERNAL_ROM */
#include "vax_ka610_bin.h" /* Defines BOOT_CODE_FILENAME and BOOT_CODE_ARRAY, etc */
#endif /* DONT_USE_INTERNAL_ROM */

/* MicroVAX I boot device definitions */

struct boot_dev {
    const char          *devname;
    const char          *devalias;
    int32_t             code;
    };

static int32_t conisp, conpc, conpsl;                   /* console reg */
int32_t sys_model = 0;                                  /* MicroVAX or VAXstation */
static char cpu_boot_cmd[CBUFSIZE]  = { 0 };            /* boot command */

static struct boot_dev boot_tab[] = {
    { "RQ",  "DUA", 0x00415544 },                       /* DUAn */
    { "RQ",  "DU",  0x00415544 },                       /* DUAn */
    { "XQ",  "XQA", 0x00415158 },                       /* XQAn */
    { NULL }
    };

static t_stat sysd_reset (DEVICE *dptr);
static const char *sysd_description (DEVICE *dptr);
static t_stat vax610_boot (int32_t flag, const char *ptr);
static t_stat vax610_boot_parse (int32_t flag, const char *ptr);

/* SYSD data structures

   sysd_dev     SYSD device descriptor
   sysd_unit    SYSD units
   sysd_reg     SYSD register list
*/

static UNIT sysd_unit = { UDATA (NULL, 0, 0) };

static REG sysd_reg[] = {
    { HRDATAD (CONISP, conisp, 32, "console ISP") },
    { HRDATAD (CONPC,   conpc, 32, "console PD") },
    { HRDATAD (CONPSL, conpsl, 32, "console PSL") },
    { BRDATA (BOOTCMD, cpu_boot_cmd, 16, 8, CBUFSIZE), REG_HRO },
    { NULL }
    };

DEVICE sysd_dev = {
    "SYSD", &sysd_unit, sysd_reg, NULL,
    1, 16, 16, 1, 16, 8,
    NULL, NULL, &sysd_reset,
    NULL, NULL, NULL,
    NULL, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL,
    &sysd_description
    };

/* Special boot command, overrides regular boot */

static CTAB vax610_cmd[] = {
    { "BOOT", &vax610_boot, RU_BOOT,
      "bo{ot} <device>{/R5:flg} boot device\n"
      "                         type HELP CPU to see bootable devices\n", NULL, &run_cmd_message },
    { NULL }
    };

/* Read KA610 specific IPR's */

int32_t ReadIPR (int32_t rg)
{
int32_t val;

switch (rg) {

    case MT_ICCS:                                       /* ICCS */
        val = iccs_rd ();
        break;

    case MT_RXCS:                                       /* RXCS */
        val = rxcs_rd ();
        break;

    case MT_RXDB:                                       /* RXDB */
        val = rxdb_rd ();
        break;

    case MT_TXCS:                                       /* TXCS */
        val = txcs_rd ();
        break;

    case MT_TXDB:                                       /* TXDB */
        val = 0;
        break;

    case MT_CONISP:                                     /* console ISP */
        val = conisp;
        break;

    case MT_CONPC:                                      /* console PC */
        val = conpc;
        break;

    case MT_CONPSL:                                     /* console PSL */
        val = conpsl;
        break;

    case MT_SID:                                        /* SID */
        val = (VAX610_SID | ((cpu_instruction_set & VAX_DFLOAT) ? VAX610_FLOAT: 0) | VAX610_MREV | VAX610_HWREV);
        break;

    case MT_NICR:                                       /* NICR */
    case MT_ICR:                                        /* ICR */
    case MT_TODR:                                       /* TODR */
    case MT_CSRS:                                       /* CSRS */
    case MT_CSRD:                                       /* CSRD */
    case MT_CSTS:                                       /* CSTS */
    case MT_CSTD:                                       /* CSTD */
    case MT_TBDR:                                       /* TBDR */
    case MT_CADR:                                       /* CADR */
    case MT_MCESR:                                      /* MCESR */
    case MT_CAER:                                       /* CAER */
    case MT_SBIFS:                                      /* SBIFS */
    case MT_SBIS:                                       /* SBIS */
    case MT_SBISC:                                      /* SBISC */
    case MT_SBIMT:                                      /* SBIMT */
    case MT_SBIER:                                      /* SBIER */
    case MT_SBITA:                                      /* SBITA */
    case MT_SBIQC:                                      /* SBIQC */
    case MT_TBDATA:                                     /* TBDATA */
    case MT_MBRK:                                       /* MBRK */
    case MT_PME:                                        /* PME */
        val = 0;
        break;

    default:
        RSVD_OPND_FAULT(ReadIPR);
        }

return val;
}

/* Write KA610 specific IPR's */

void WriteIPR (int32_t rg, int32_t val)
{
switch (rg) {

    case MT_ICCS:                                       /* ICCS */
        iccs_wr (val);
        break;

    case MT_RXCS:                                       /* RXCS */
        rxcs_wr (val);
        break;

    case MT_RXDB:                                       /* RXDB */
        break;

    case MT_TXCS:                                       /* TXCS */
        txcs_wr (val);
        break;

    case MT_TXDB:                                       /* TXDB */
        txdb_wr (val);
        break;

    case MT_IORESET:                                    /* IORESET */
        ioreset_wr (val);
        break;

    case MT_SID:
    case MT_CONISP:
    case MT_CONPC:
    case MT_CONPSL:                                     /* halt reg */
        RSVD_OPND_FAULT(WriteIPR);

    case MT_NICR:                                       /* NICR */
    case MT_ICR:                                        /* ICR */
    case MT_TODR:                                       /* TODR */
    case MT_CSRS:                                       /* CSRS */
    case MT_CSRD:                                       /* CSRD */
    case MT_CSTS:                                       /* CSTS */
    case MT_CSTD:                                       /* CSTD */
    case MT_TBDR:                                       /* TBDR */
    case MT_CADR:                                       /* CADR */
    case MT_MCESR:                                      /* MCESR */
    case MT_CAER:                                       /* CAER */
    case MT_SBIFS:                                      /* SBIFS */
    case MT_SBIS:                                       /* SBIS */
    case MT_SBISC:                                      /* SBISC */
    case MT_SBIMT:                                      /* SBIMT */
    case MT_SBIER:                                      /* SBIER */
    case MT_SBITA:                                      /* SBITA */
    case MT_SBIQC:                                      /* SBIQC */
    case MT_TBDATA:                                     /* TBDATA */
    case MT_MBRK:                                       /* MBRK */
    case MT_PME:                                        /* PME */
        break;

    default:
        RSVD_OPND_FAULT(WriteIPR);
        }

return;
}

/* Read/write I/O register space

   These routines are the 'catch all' for address space map.  Any
   address that doesn't explicitly belong to memory or I/O
   is given to these routines for processing.
*/

struct reglink {                                        /* register linkage */
    uint32_t    low;                                    /* low addr */
    uint32_t    high;                                   /* high addr */
    int32_t     (*read)(int32_t pa, int32_t lnt);       /* read routine */
    void        (*write)(int32_t pa, int32_t val, int32_t lnt); /* write routine */
    };

static struct reglink regtable[] = {
    { 0, 0, NULL, NULL }
    };

/* ReadReg - read register space

   Inputs:
        pa      =       physical address
        lnt     =       length (BWLQ) - ignored
   Output:
        longword of data
*/

int32_t ReadReg (uint32_t pa, int32_t lnt)
{
/* Model-dependent register read signature.
   This implementation does not use every parameter. */
(void) lnt;

struct reglink *p;

for (p = &regtable[0]; p->low != 0; p++) {
    if ((pa >= p->low) && (pa < p->high) && p->read)
        return p->read (pa, lnt);
    }
MACH_CHECK (MCHK_READ);
}

/* ReadRegU - read register space, unaligned

   Inputs:
        pa      =       physical address
        lnt     =       length in bytes (1, 2, or 3)
   Output:
        returned data, not shifted
*/

int32_t ReadRegU (uint32_t pa, int32_t lnt)
{
/* Model-dependent unaligned register read signature.
   This implementation does not use every parameter. */
(void) lnt;

return ReadReg (pa & ~03, L_LONG);
}

/* WriteReg - write register space

   Inputs:
        pa      =       physical address
        val     =       data to write, right justified in 32b longword
        lnt     =       length (BWLQ)
   Outputs:
        none
*/

void WriteReg (uint32_t pa, int32_t val, int32_t lnt)
{
struct reglink *p;

for (p = &regtable[0]; p->low != 0; p++) {
    if ((pa >= p->low) && (pa < p->high) && p->write) {
        p->write (pa, val, lnt);
        return;
        }
    }
mem_err = 1;
SET_IRQL;
}

/* WriteRegU - write register space, unaligned

   Inputs:
        pa      =       physical address
        val     =       data to write, right justified in 32b longword
        lnt     =       length (1, 2, or 3)
   Outputs:
        none
*/

void WriteRegU (uint32_t pa, int32_t val, int32_t lnt)
{
int32_t sc = (pa & 03) << 3;
int32_t dat = ReadReg (pa & ~03, L_LONG);

dat = (dat & ~(insert[lnt] << sc)) | ((val & insert[lnt]) << sc);
WriteReg (pa & ~03, dat, L_LONG);
return;
}

/* Special boot command - linked into SCP by initial reset

   Syntax: BOOT <device>{/R5:val}

   Sets up R0-R5, calls SCP boot processor with effective BOOT CPU
*/

static t_stat vax610_boot (int32_t flag, const char *ptr)
{
t_stat r;

if ((ptr = get_sim_sw (ptr)) == NULL)                   /* get switches */
    return SCPE_INVSW;
r = vax610_boot_parse (flag, ptr);                      /* parse the boot cmd */
if (r != SCPE_OK) {                                     /* error? */
    if (r >= SCPE_BASE) {                               /* message available? */
        sim_printf ("%s\n", sim_error_text (r));
        r |= SCPE_NOMESSAGE;
        }
    return r;
    }
strlcpy (cpu_boot_cmd, ptr, sizeof (cpu_boot_cmd));     /* save for reboot */
return run_cmd (flag, "CPU");
}

/* Parse boot command, set up registers - also used on reset */

static t_stat vax610_boot_parse (int32_t flag, const char *ptr)
{
char gbuf[CBUFSIZE], dbuf[CBUFSIZE], rbuf[CBUFSIZE];
char *slptr;
const char *regptr;
int32_t i, r5v, unitno;
DEVICE *dptr;
UNIT *uptr;
t_stat r;

if (ptr == NULL)
    return SCPE_ARG;
if (*ptr == '/') {                                      /* handle "BOOT /R5:n DEV" format */
    ptr = get_glyph (ptr, rbuf, 0);                     /* get glyph */
    regptr = rbuf;
    ptr = get_glyph (ptr, gbuf, 0);                     /* get glyph */
    }
else {                                                  /* handle "BOOT DEV /R5:n" format */
    regptr = get_glyph (ptr, gbuf, 0);                  /* get glyph */
    if ((slptr = strchr (gbuf, '/'))) {                 /* found slash? */
        regptr = strchr (ptr, '/');                     /* locate orig */
        *slptr = 0;                                     /* zero in string */
        }
    }
/* parse R5 parameter value */
r5v = 0;
/* coverity[NULL_RETURNS] */
if ((strncmp (regptr, "/R5:", 4) == 0) ||
    (strncmp (regptr, "/R5=", 4) == 0) ||
    (strncmp (regptr, "/r5:", 4) == 0) ||
    (strncmp (regptr, "/r5=", 4) == 0)) {
    r5v = (int32_t) get_uint (regptr + 4, 16, LMASK, &r);
    if (r != SCPE_OK)
        return r;
    }
else if (*regptr == '/') {
    r5v = (int32_t) get_uint (regptr + 1, 16, LMASK, &r);
    if (r != SCPE_OK)
        return r;
    }
else if (*regptr != 0)
    return SCPE_ARG;
if (gbuf[0]) {
    unitno = -1;
    for (i = 0; boot_tab[i].devname != NULL; i++) {
        if (memcmp (gbuf, boot_tab[i].devalias, strlen(boot_tab[i].devalias)) == 0) {
            snprintf(dbuf, sizeof(dbuf), "%s%s", boot_tab[i].devname, gbuf + strlen(boot_tab[i].devalias));
            dptr = find_unit (dbuf, &uptr);
            if ((dptr == NULL) || (uptr == NULL))
                return SCPE_ARG;
            unitno = (int32_t) (uptr - dptr->units);
            }
        if ((unitno == -1) &&
            (memcmp (gbuf, boot_tab[i].devname, strlen(boot_tab[i].devname)) == 0)) {
            snprintf(dbuf, sizeof(dbuf), "%s%s", boot_tab[i].devname, gbuf + strlen(boot_tab[i].devname));
            dptr = find_unit (dbuf, &uptr);
            if ((dptr == NULL) || (uptr == NULL))
                return SCPE_ARG;
            unitno = (int32_t) (uptr - dptr->units);
            }
        if (unitno == -1)
            continue;
        R[0] = boot_tab[i].code | (('0' + unitno) << 24);
        R[1] = (sys_model ? 0x80 : 0xC0);
        R[2] = 0;
        R[3] = 0;
        R[4] = 0;
        R[5] = r5v;
        return SCPE_OK;
        }
    }
else {
    R[0] = 0;
    R[1] = (sys_model ? 0x80 : 0xC0);
    R[2] = 0;
    R[3] = 0;
    R[4] = 0;
    R[5] = r5v;
    return SCPE_OK;
    }
return SCPE_NOFNC;
}

int32_t sysd_hlt_enb (void)
{
return 1;
}

/* Machine check */

int32_t machine_check (int32_t p1, int32_t opc, int32_t cc, int32_t delta)
{
/* Model-dependent machine check signature.
   This implementation does not use every parameter. */
(void) opc;
(void) delta;

int32_t p2, acc;

if (in_ie)                                              /* in exc? panic */
    ABORT (STOP_INIE);
p2 = mchk_va + 4;                                       /* save vap */
cc = intexc (SCB_MCHK, cc, 0, IE_EXC);                  /* take exception */
acc = ACC_MASK (KERN);                                  /* in kernel mode */
in_ie = 1;
SP = SP - 16;                                           /* push 4 words */
Write (SP, 12, L_LONG, WA);                             /* # bytes */
Write (SP + 4, p1, L_LONG, WA);                         /* mcheck type */
Write (SP + 8, p2, L_LONG, WA);                         /* parameter 1 */
Write (SP + 12, p2, L_LONG, WA);                        /* parameter 2 */
in_ie = 0;
return cc;
}

/* Console entry */

int32_t con_halt (int32_t code, int32_t cc)
{
/* Model-dependent console halt signature.
   This implementation does not use every parameter. */
(void) code;

if ((vax610_boot_parse (0, cpu_boot_cmd) != SCPE_OK) || /* reparse the boot cmd */
    (reset_all (0) != SCPE_OK) ||                       /* reset the world */
    (cpu_boot (0, NULL) != SCPE_OK))                    /* set up boot code */
    ABORT (STOP_BOOT);                                  /* any error? */
sim_printf ("Rebooting...\n");
return cc;
}

/* Bootstrap */

t_stat cpu_boot (int32_t unitno, DEVICE *dptr)
{
/* Generic CPU boot signature.
   This implementation does not use every parameter. */
(void) unitno;
(void) dptr;

t_stat r;

r = cpu_load_bootcode (BOOT_CODE_FILENAME, BOOT_CODE_ARRAY, BOOT_CODE_SIZE, false, 0x200);
if (r != SCPE_OK)
    return r;
SP = PC = 512;
AP = 1;
return SCPE_OK;
}

t_stat vax610_set_instruction_set (UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
/* Generic set modifier signature.
   This implementation does not use every parameter. */
(void) desc;

char gbuf[CBUFSIZE];

if (!cptr || !*cptr)
    return SCPE_ARG;

get_glyph (cptr, gbuf, 0);
if (MATCH_CMD(gbuf, "G-FLOAT") == 0)
    return cpu_set_instruction_set (uptr, val, "G-FLOAT;NOD-FLOAT", NULL);
if (MATCH_CMD(gbuf, "D-FLOAT") == 0)
    return cpu_set_instruction_set (uptr, val, "D-FLOAT;NOG-FLOAT", NULL);
return sim_messagef (SCPE_ARG, "Unknown/Unsupported instruction set: %s\n", gbuf);
}

/* SYSD reset */

static t_stat sysd_reset (DEVICE *dptr)
{
/* Generic device reset signature.
   This implementation does not use every parameter. */
(void) dptr;

sim_vm_cmd = vax610_cmd;
return SCPE_OK;
}

static const char *sysd_description (DEVICE *dptr)
{
/* Generic device description signature.
   This implementation does not use every parameter. */
(void) dptr;

return "system devices";
}

t_stat cpu_set_model (UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
/* Generic set modifier signature.
   This implementation does not use every parameter. */
(void) uptr;
(void) val;
(void) desc;

char gbuf[CBUFSIZE];

if ((cptr == NULL) || (!*cptr))
    return SCPE_ARG;
cptr = get_glyph (cptr, gbuf, 0);
if (MATCH_CMD(gbuf, "MICROVAX") == 0) {
    sys_model = 0;
#if defined(USE_SIM_VIDEO) && defined(HAVE_LIBSDL)
    vc_dev.flags = vc_dev.flags | DEV_DIS;               /* disable QVSS */
    lk_dev.flags = lk_dev.flags | DEV_DIS;               /* disable keyboard */
    vs_dev.flags = vs_dev.flags | DEV_DIS;               /* disable mouse */
#endif
    strlcpy (sim_name, "MicroVAX I (KA610)", sizeof (sim_name));
    reset_all (0);                                       /* reset everything */
    }
else if (MATCH_CMD(gbuf, "VAXSTATION") == 0) {
#if defined(USE_SIM_VIDEO) && defined(HAVE_LIBSDL)
    sys_model = 1;
    vc_dev.flags = vc_dev.flags & ~DEV_DIS;              /* enable QVSS */
    lk_dev.flags = lk_dev.flags & ~DEV_DIS;              /* enable keyboard */
    vs_dev.flags = vs_dev.flags & ~DEV_DIS;              /* enable mouse */
    strlcpy (sim_name, "VAXstation I (KA610)", sizeof (sim_name));
    reset_all (0);                                       /* reset everything */
#else
    return sim_messagef(SCPE_ARG, "Simulator built without Graphic Device Support\n");
#endif
    }
else
    return SCPE_ARG;
return SCPE_OK;
}

t_stat cpu_print_model (FILE *st)
{
fprintf (st, (sys_model ? "VAXstation I" : "MicroVAX I"));
return SCPE_OK;
}

t_stat cpu_model_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr)
{
/* Generic help signature.
   This implementation does not use every parameter. */
(void) dptr;
(void) uptr;
(void) flag;
(void) cptr;

fprintf (st, "Initial memory size is 4MB.\n\n");
fprintf (st, "The simulator is booted with the BOOT command:\n\n");
fprintf (st, "   sim> BO{OT} <device>{/R5:flags}\n\n");
fprintf (st, "where <device> is one of:\n\n");
fprintf (st, "   RQn        to boot from rqn\n");
fprintf (st, "   DUn        to boot from rqn\n");
fprintf (st, "   DUAn       to boot from rqn\n");
fprintf (st, "   XQ         to boot from xq\n");
fprintf (st, "   XQA        to boot from xq\n\n");
return SCPE_OK;
}
