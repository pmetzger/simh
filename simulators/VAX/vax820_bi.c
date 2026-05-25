/* vax820_bi.c: VAX 8200 BI

   Copyright (c) 2019, Matt Burke
   This module incorporates code from SimH, Copyright (c) 2004-2008, Robert M Supnik

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

   This module contains the VAX 8200 system-specific registers and devices.

   bi                   bus controller
*/

#include <stdbool.h>
#include <stdint.h>

#include "vax_defs.h"
#include "vax_cpu.h"
#include "vax_cpu1.h"
#include "vax820_bi.h"
#include "vax_kdb50.h"

#ifdef DONT_USE_INTERNAL_ROM
#define BOOT_CODE_FILENAME "vmb.exe"
#else /* !DONT_USE_INTERNAL_ROM */
#include "vax_vmb_exe.h" /* Defines BOOT_CODE_FILENAME and BOOT_CODE_ARRAY, etc */
#endif /* DONT_USE_INTERNAL_ROM */

/* KA820 specific IPRs */

/* Writeable control store */

#define WCSA_RW         0x3FFFFF                        /* writeable */
#define WCSD_RD_VAL     0xFF                            /* fixed read val */
#define WCSD_WR         0xFFFFFFFF                      /* write */
#define MBRK_RW         0x1FFF                          /* microbreak */

/* Accelerator control/status register */

#define ACCS_ON         0x1                             /* enable F-chip */
#define ACCS_RW         (ACCS_ON)

/* KA820 boot device definitions */

struct boot_dev {
    const char          *name;
    int32_t             code;
    int32_t             let;
    };

static uint32_t wcs_addr = 0;
static uint32_t wcs_data = 0;
uint32_t nexus_req[NEXUS_HLVL];                         /* nexus int req */
static int32_t ipr_int = 0;
int32_t rxcd_int = 0;
int32_t ipir = 0;
static int32_t accs = 0;
int32_t sys_model = 0;
static int32_t mchk_flag[KA_NUM] = { 0 };
static char cpu_boot_cmd[CBUFSIZE]  = { 0 };            /* boot command */

static t_stat (*nexusR[NEXUS_NUM])(int32_t *dat, int32_t ad, int32_t md);
static t_stat (*nexusW[NEXUS_NUM])(int32_t dat, int32_t ad, int32_t md);

static struct boot_dev boot_tab[] = {
    { "HK", BOOT_HK, 0 },
    { "RL", BOOT_RL, 0 },
    { "RQ", BOOT_UDA, 1 << 24 },
    { "RQB", BOOT_UDA, 1 << 24 },
    { "RQC", BOOT_UDA, 1 << 24 },
    { "RQD", BOOT_UDA, 1 << 24 },
    { "KDB", BOOT_KDB, 0 },
    { "CS", BOOT_CS, 0 },
    { NULL }
    };

static t_stat bi_reset (DEVICE *dptr);
static t_stat vax820_boot (int32_t flag, const char *ptr);
static t_stat vax820_boot_parse (int32_t flag, const char *ptr);

/* BI data structures

   bi_dev       BI device descriptor
   bi_unit      BI unit
   bi_reg       BI register list
*/

static UNIT bi_unit = { UDATA (NULL, 0, 0) };

static REG bi_reg[] = {
    { HRDATA (NREQ14, nexus_req[0], 16) },
    { HRDATA (NREQ15, nexus_req[1], 16) },
    { HRDATA (NREQ16, nexus_req[2], 16) },
    { HRDATA (NREQ17, nexus_req[3], 16) },
    { HRDATA (WCSA, wcs_addr, 21) },
    { HRDATA (WCSD, wcs_data, 32) },
    { NULL }
    };

DEVICE bi_dev = {
    "BI", &bi_unit, bi_reg, NULL,
    1, 16, 16, 1, 16, 8,
    NULL, NULL, &bi_reset,
    NULL, NULL, NULL,
    NULL, 0
    };

/* Special boot command, overrides regular boot */

CTAB vax820_cmd[] = {
    { "BOOT", &vax820_boot, RU_BOOT,
      "bo{ot} <device>{/R5:flg} boot device\n"
      "                         type HELP CPU to see bootable devices\n", NULL, &run_cmd_message },
    { NULL }
    };

/* The VAX 8200 has three sources of interrupts

   - internal device interrupts (CPU, console, clock)
   - nexus interupts (e.g., memory controller, MBA, UBA)
   - external device interrupts (Unibus)

   Internal devices vector to fixed SCB locations.

   Nexus interrupts vector to an SCB location based on this
   formula: SCB_NEXUS + ((IPL - 0x14) * 0x40) + (TR# * 0x4)

   External device interrupts do not vector directly.
   Instead, the interrupt handler for a given UBA IPL
   reads a vector register that contains the Unibus vector
   for that IPL.
*/
/* Find highest priority vectorable interrupt */

int32_t eval_int (void)
{
int32_t ipl = PSL_GETIPL (PSL);
int32_t cpu_msk = (1u << cur_cpu);
int32_t i, t;

static const int32_t sw_int_mask[IPL_SMAX] = {
    0xFFFE, 0xFFFC, 0xFFF8, 0xFFF0,                     /* 0 - 3 */
    0xFFE0, 0xFFC0, 0xFF80, 0xFF00,                     /* 4 - 7 */
    0xFE00, 0xFC00, 0xF800, 0xF000,                     /* 8 - B */
    0xE000, 0xC000, 0x8000                              /* C - E */
    };

if (hlt_pin)                                            /* hlt pin int */
    return IPL_HLTPIN;
if ((ipl < IPL_CRDERR) && crd_err)                      /* crd err int */
    return IPL_CRDERR;
if ((ipl < IPL_CLKINT) && tmr_int)                      /* clock int */
    return IPL_CLKINT;
uba_eval_int ();                                        /* update UBA */
for (i = IPL_HMAX; i >= IPL_HMIN; i--) {                /* chk hwre int */
    if (i <= ipl)                                       /* at ipl? no int */
        return 0;
    if (nexus_req[i - IPL_HMIN])                        /* req != 0? int */
        return i;
    }
if ((ipl < IPL_RXCDINT) && rxcd_int)                    /* rxcd int */
    return IPL_RXCDINT;
if ((ipl < IPL_IPRINT) && (ipir & cpu_msk))             /* ipr int */
    return IPL_IPRINT;
if ((ipl < IPL_FLINT) && fl_int)                        /* console floppy int */
    return IPL_FLINT;
if ((ipl < IPL_TTINT) && (tti_int && (cur_cpu == 0)))   /* console int */
    return IPL_TTINT;
if ((ipl < IPL_TTINT) && (tto_int & cpu_msk))           /* console int */
    return IPL_TTINT;
if (ipl >= IPL_SMAX)                                    /* ipl >= sw max? */
    return 0;
if ((t = SISR & sw_int_mask[ipl]) == 0)
    return 0;                                           /* eligible req */
for (i = IPL_SMAX; i > ipl; i--) {                      /* check swre int */
    if ((t >> i) & 1)                                   /* req != 0? int */
        return i;
    }
return 0;
}

/* Return vector for highest priority hardware interrupt at IPL lvl */

int32_t get_vector (int32_t lvl)
{
int32_t i, l;
int32_t vec;
int32_t cpu_msk = (1u << cur_cpu);

if (lvl == IPL_CRDERR) {                                /* CRD error? */
    crd_err = 0;
    return SCB_CRDERR;
    }
if (lvl == IPL_CLKINT) {                                /* clock? */
    tmr_int = tmr_int & ~(1u << cur_cpu);               /* clear req */
    return SCB_INTTIM;                                  /* return vector */
    }
if (lvl > IPL_HMAX) {                                   /* error req lvl? */
    ABORT (STOP_UIPL);                                  /* unknown intr */
    }
if ((lvl <= IPL_HMAX) && (lvl >= IPL_HMIN)) {           /* nexus? */
    l = lvl - IPL_HMIN;
    if (nexus_req[l] & (1u << TR_UBA)) {                /* unibus int? */
        nexus_req[l] = nexus_req[l] & ~(1u << TR_UBA);
        vec = uba_get_ubvector(l);
        return vec;                                     /* return vector */
        }
    for (i = 0; nexus_req[l] && (i < NEXUS_NUM); i++) {
        if ((nexus_req[l] >> i) & 1) {
            nexus_req[l] = nexus_req[l] & ~(1u << i);
            if (i == TR_KDB50)
                vec = kdb50_get_vector(l);
            else
                vec = SCB_NEXUS + (l << 6) + (i << 2);
            return vec;                                 /* return vector */
            }
        }
    }
if (lvl == IPL_RXCDINT) {
    if (rxcd_int) {
        rxcd_int = 0;                                   /* clear req */
        return SCB_RXCD;                                /* return vector */
        }
    }
if (lvl == IPL_IPRINT) {                                /* inter-processor? */
    if (ipir & (1u << cur_cpu)) {
        ipir = ipir & ~(1u << cur_cpu);                 /* clear req */
        return SCB_IPRINT;                              /* return vector */
        }
    }
if (lvl == IPL_FLINT) {                                 /* console floppy? */
    if (fl_int) {
        fl_int = 0;                                     /* clear req */
        return SCB_FLINT;                               /* return vector */
        }
    }
if (lvl == IPL_TTINT) {                                 /* console? */
    if (tti_int && (cur_cpu == 0)) {                    /* input? */
        tti_int = 0;                                    /* clear req */
        return SCB_TTI;                                 /* return vector */
        }
    if (tto_int & cpu_msk) {                            /* output? */
        tto_int = 0;                                    /* clear req */
        return SCB_TTO;                                 /* return vector */
        }
    }
return 0;
}

/* Read 8200-specific IPR's */

int32_t ReadIPR (int32_t rg)
{
int32_t val;

switch (rg) {

    case MT_ICCS:                                       /* ICCS */
        val = iccs_rd ();
        break;

    case MT_NICR:                                       /* NICR */
        val = nicr_rd ();
        break;

    case MT_ICR:                                        /* ICR */
        val = icr_rd ();
        break;

    case MT_TODR:                                       /* TODR */
        val = todr_rd ();
        break;

    case MT_RXCS:                                       /* RXCS */
        if (cur_cpu == 0)
            val = rxcs_rd ();
        else
            val = 0;
        break;

    case MT_RXDB:                                       /* RXDB */
        if (cur_cpu == 0)
            val = rxdb_rd ();
        else
            val = 0;
        break;

    case MT_TXCS:                                       /* TXCS */
        val = txcs_rd ();
        break;

    case MT_TBDR:                                       /* TBDR (not impl) */
        val = 0;
        break;

    case MT_CADR:                                       /* CADR (not impl) */
        val = 0;
        break;

    case MT_ACCS:                                       /* ACCS */
        val = accs & ACCS_RW;
        break;

    case MT_WCSA:                                       /* WCSA */
        val = wcs_addr & WCSA_RW;
        break;

    case MT_WCSD:                                       /* WCSD */
        val = WCSD_RD_VAL;
        break;

    case MT_SID:                                        /* SID */
        if (sys_model)
            val = VAX820_SID | VAX820_REV | VAX820_PATCH | VAX825_TYP | VAX820_UCODE;
        else
            val = VAX820_SID | VAX820_REV | VAX820_PATCH | VAX820_TYP | VAX820_UCODE;
        break;

    case MT_RXCS1:                                      /* RXCS1 */
        val = 0;
        break;

    case MT_RXDB1:                                      /* RXDB1 */
        val = 0;
        break;

    case MT_TXCS1:                                      /* TXCS1 */
        val = CSR_DONE;
        break;

    case MT_RXCS2:                                      /* RXCS2 */
        val = 0;
        break;

    case MT_RXDB2:                                      /* RXDB2 */
        val = 0;
        break;

    case MT_TXCS2:                                      /* TXCS2 */
        val = CSR_DONE;
        break;

    case MT_RXCS3:                                      /* RXCS3 */
        val = 0;
        break;

    case MT_RXDB3:                                      /* RXDB3 */
        val = 0;
        break;

    case MT_TXCS3:                                      /* TXCS3 */
        val = CSR_DONE;
        break;

    case MT_BINID:                                      /* BINID */
        val = TR_KA0 + cur_cpu;
        break;

    case MT_RXCD:                                       /* RXCD */
        val = rxcd_rd ();
        break;

    default:
        RSVD_OPND_FAULT(ReadIPR);
        }

return val;
}

/* Write 8200-specific IPR's */

void WriteIPR (int32_t rg, int32_t val)
{
switch (rg) {

    case MT_IPIR:                                       /* IPIR */
        ipir = val;
        break;

    case MT_ICCS:                                       /* ICCS */
        iccs_wr (val);
        break;

    case MT_NICR:                                       /* NICR */
        nicr_wr (val);
        break;

    case MT_TODR:                                       /* TODR */
        todr_wr (val);
        break;

    case MT_RXCS:                                       /* RXCS */
        if (cur_cpu == 0)
            rxcs_wr (val);
        break;

    case MT_TXCS:                                       /* TXCS */
        txcs_wr (val);
        break;

    case MT_TXDB:                                       /* TXDB */
        txdb_wr (val);
        break;

    case MT_TBDR:                                       /* TBDR (not impl) */
        break;

    case MT_CADR:                                       /* CADR (not impl) */
        break;

    case MT_MCESR:                                      /* MCESR */
        mchk_flag[cur_cpu] = 0;
        break;

    case MT_ACCS:                                       /* ACCS */
        accs = val & ACCS_RW;
        break;

    case MT_WCSA:                                       /* WCSA */
        wcs_addr = val & WCSA_RW;
        break;

    case MT_WCSL:                                       /* WCSL */
        wcs_data = val & WCSD_WR;
        break;

    case MT_RXCS1:                                      /* RXCS1 */
    case MT_TXCS1:                                      /* TXCS1 */
    case MT_TXDB1:                                      /* TXDB1 */
    case MT_RXCS2:                                      /* RXCS2 */
    case MT_TXCS2:                                      /* TXCS2 */
    case MT_TXDB2:                                      /* TXDB2 */
    case MT_RXCS3:                                      /* RXCS3 */
    case MT_TXCS3:                                      /* TXCS3 */
    case MT_TXDB3:                                      /* TXDB3 */
    case MT_CACHEX:                                     /* CACHEX */
    case MT_BISTOP:                                     /* BISTOP */
        break;

    case MT_RXCD:                                       /* RXCD */
        rxcd_wr (val);
        break;

    default:
        RSVD_OPND_FAULT(WriteIPR);
        }

return;
}

struct reglink {                                        /* register linkage */
    uint32_t    low;                                    /* low addr */
    uint32_t    high;                                   /* high addr */
    int32_t     (*read)(int32_t pa);                    /* read routine */
    void        (*write)(int32_t pa, int32_t val, int32_t lnt); /* write routine */
    };

static struct reglink regtable[] = {
    { WATCHBASE, WATCHBASE+WATCHSIZE, &wtc_rd_pa, &wtc_wr_pa },
    { 0x20088000, 0x20088004, &pcsr_rd, &pcsr_wr },
    { 0x200B0000, 0x200B0020, &fl_rd, &fl_wr },
    { 0, 0, NULL, NULL }
    };

/* ReadReg - read register space

   Inputs:
        pa      =       physical address
        lnt     =       length (BWLQ)
   Output:
        longword of data
*/

int32_t ReadReg (uint32_t pa, int32_t lnt)
{
int32_t nexus, val;
struct reglink *p;

if (ADDR_IS_REG (pa)) {                                 /* reg space? */
    nexus = NEXUS_GETNEX (pa);                          /* get nexus */
    if (nexusR[nexus] &&                                /* valid? */
        (nexusR[nexus] (&val, pa, lnt) == SCPE_OK)) {
        SET_IRQL;
        return val;
        }
    MACH_CHECK (MCHK_BIERR);                            /* machine check */
    return 0;
    }
for (p = &regtable[0]; p->low != 0; p++) {
    if ((pa >= p->low) && (pa < p->high) && p->read)
        return p->read (pa);
    }
MACH_CHECK (MCHK_BIERR);                                /* machine check */
return 0;
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
int32_t nexus;
struct reglink *p;

if (ADDR_IS_REG (pa)) {                                 /* reg space? */
    nexus = NEXUS_GETNEX (pa);                          /* get nexus */
    if (nexusW[nexus] &&                                /* valid? */
        (nexusW[nexus] (val, pa, lnt) == SCPE_OK)) {
        SET_IRQL;
        return;
        }
    }
for (p = &regtable[0]; p->low != 0; p++) {
    if ((pa >= p->low) && (pa < p->high) && p->write) {
        p->write (pa, val, lnt);
        return;
        }
    }
return;
}

/* Machine check

   Error status word format
   <2:0> =      ASTLVL
   <3> =        PME
   <6:4> =      arith trap code
   Rest will be zero
*/

int32_t machine_check (int32_t p1, int32_t opc, int32_t cc, int32_t delta)
{
/* System-specific machine-check hook.
   This implementation does not use every parameter. */
(void) opc;
(void) delta;

int32_t acc;

if (mchk_flag[cur_cpu])                                 /* double error? */
    ABORT (STOP_INIE);                                  /* halt */
mchk_flag[cur_cpu] = 1;
if (in_ie)                                              /* in exc? panic */
    ABORT (STOP_INIE);
cc = intexc (SCB_MCHK, cc, 0, IE_SVE);                  /* take exception */
acc = ACC_MASK (KERN);                                  /* in kernel mode */
in_ie = 1;
SP = SP - 36;                                           /* push 8 words */
Write (SP, 32, L_LONG, WA);                             /* # bytes */
Write (SP + 4, p1, L_LONG, WA);                         /* mcheck type */
Write (SP + 8, 0, L_LONG, WA);                          /* parameter 1 */
Write (SP + 12, mchk_va, L_LONG, WA);                   /* VA */
Write (SP + 16, mchk_va, L_LONG, WA);                   /* VA prime */
Write (SP + 20, 0, L_LONG, WA);                         /* memory address */
Write (SP + 24, 0x00400000, L_LONG, WA);                /* status word */
Write (SP + 28, PC, L_LONG, WA);                        /* PC at failure */
Write (SP + 32, 0, L_LONG, WA);                         /* uPC at failure */
in_ie = 0;
return cc;
}

/* Console entry - only reached if CONHALT is set (AUTORESTART is set */

int32_t con_halt (int32_t code, int32_t cc)
{
/* System-specific console halt hook.
   This implementation does not use every parameter. */
(void) code;

if ((cpu_boot_cmd[0] == 0) ||                           /* saved boot cmd? */
    (vax820_boot_parse (0, cpu_boot_cmd) != SCPE_OK) || /* reparse the boot cmd */
    (reset_all (0) != SCPE_OK) ||                       /* reset the world */
    (cpu_boot (0, NULL) != SCPE_OK))                    /* set up boot code */
    ABORT (STOP_BOOT);                                  /* any error? */
sim_printf ("Rebooting...\n");
return cc;
}

/* Special boot command - linked into SCP by initial reset

   Syntax: BOOT <device>{/R5:val}

   Sets up R0-R5, calls SCP boot processor with effective BOOT CPU
*/

static t_stat vax820_boot (int32_t flag, const char *ptr)
{
t_stat r;

r = vax820_boot_parse (flag, ptr);                      /* parse the boot cmd */
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

static t_stat vax820_boot_parse (int32_t flag, const char *ptr)
{
char gbuf[CBUFSIZE];
char *slptr;
const char *regptr;
int32_t i, r5v, unitno;
DEVICE *dptr;
UNIT *uptr;
DIB *dibp;
t_stat r;

if (!ptr || !*ptr)
    return SCPE_2FARG;
if ((ptr = get_sim_sw (ptr)) == NULL)                   /* get switches */
    return SCPE_INVSW;
regptr = get_glyph (ptr, gbuf, 0);                      /* get glyph */
if ((slptr = strchr (gbuf, '/'))) {                     /* found slash? */
    regptr = strchr (ptr, '/');                         /* locate orig */
    *slptr = 0;                                         /* zero in string */
    }
dptr = find_unit (gbuf, &uptr);                         /* find device */
if ((dptr == NULL) || (uptr == NULL))
    return SCPE_ARG;
dibp = (DIB *) dptr->ctxt;                              /* get DIB */
if (dibp == NULL)
    return SCPE_ARG;
unitno = (int32_t) (uptr - dptr->units);
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
else if (*regptr != 0)
    return SCPE_ARG;
for (i = 0; boot_tab[i].name != NULL; i++) {
    if (strcmp (dptr->name, boot_tab[i].name) == 0) {
        R[0] = boot_tab[i].code;
        if (boot_tab[i].code == BOOT_KDB) {
            R[1] = dibp->ba;
            R[2] = REGBASE + ((uint32_t)dibp->ba << REG_V_NEXUS);
            }
        else {
            R[1] = TR_UBA;
            R[2] = boot_tab[i].let | (dibp->ba & UBADDRMASK);
            }
        R[3] = unitno;
        R[4] = 0;
        R[5] = r5v;
        return SCPE_OK;
        }
    }
return SCPE_NOFNC;
}

/* Bootstrap - finish up bootstrap process */

t_stat cpu_boot (int32_t unitno, DEVICE *dptr)
{
/* Generic device boot signature.
   This implementation does not use every parameter. */
(void) unitno;
(void) dptr;

t_stat r;

r = cpu_load_bootcode (BOOT_CODE_FILENAME, BOOT_CODE_ARRAY, BOOT_CODE_SIZE, false, 0x200);
if (r != SCPE_OK)
    return r;
SP = PC = 512;
return SCPE_OK;
}

/* BI reset */

static t_stat bi_reset (DEVICE *dptr)
{
/* Generic device reset signature.
   This implementation does not use every parameter. */
(void) dptr;

accs = ACCS_ON;                                         /* enabled by default */
wcs_addr = 0;
wcs_data = 0;
ipr_int = 0;
rxcd_int = 0;
ipir = 0;
sim_vm_cmd = vax820_cmd;
return SCPE_OK;
}

/* Show nexus */

t_stat show_nexus (FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
/* Generic show modifier signature.
   This implementation does not use every parameter. */
(void) uptr;
(void) desc;

fprintf (st, "nexus=%d", val);
return SCPE_OK;
}

/* Init nexus tables */

void init_nexus_tab (void)
{
uint32_t i;

for (i = 0; i < NEXUS_NUM; i++) {
    nexusR[i] = NULL;
    nexusW[i] = NULL;
    }
return;
}

/* Build nexus tables

   Inputs:
        dptr    =       pointer to device
        dibp    =       pointer to DIB
   Outputs:
        status
*/

t_stat build_nexus_tab (DEVICE *dptr, DIB *dibp)
{
uint32_t idx;

if ((dptr == NULL) || (dibp == NULL))
    return SCPE_IERR;
idx = dibp->ba;
if (idx >= NEXUS_NUM)
    return SCPE_IERR;
if ((nexusR[idx] && dibp->rd &&                         /* conflict? */
    (nexusR[idx] != dibp->rd)) ||
    (nexusW[idx] && dibp->wr &&
    (nexusW[idx] != dibp->wr))) {
    sim_printf ("Nexus %s conflict at %d\n", sim_dname (dptr), dibp->ba);
    return SCPE_STOP;
    }
if (dibp->rd)                                           /* set rd dispatch */
    nexusR[idx] = dibp->rd;
if (dibp->wr)                                           /* set wr dispatch */
    nexusW[idx] = dibp->wr;
return SCPE_OK;
}

/* Build dib_tab from device list */

t_stat build_dib_tab (void)
{
uint32_t i;
DEVICE *dptr;
DIB *dibp;
t_stat r;

init_nexus_tab ();
init_ubus_tab ();
for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {     /* loop thru dev */
    dibp = (DIB *) dptr->ctxt;                          /* get DIB */
    if (dibp && !(dptr->flags & DEV_DIS)) {             /* defined, enabled? */
        if (dptr->flags & DEV_NEXUS) {                  /* Nexus? */
            if ((r = build_nexus_tab (dptr, dibp)))     /* add to dispatch table */
                return r;
            }
        else {                                          /* no, Unibus device */
            if ((r = build_ubus_tab (dptr, dibp)))      /* add to dispatch tab */
                return r;
            }                                           /* end else */
        }                                               /* end if enabled */
    }                                                   /* end for */
return SCPE_OK;
}

t_stat cpu_set_model (UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
/* Generic set modifier signature.
   This implementation does not use every parameter. */
(void) uptr;
(void) val;
(void) desc;

if (cptr == NULL)
    return SCPE_ARG;
if (strcmp(cptr, "8200") == 0) {
   sys_model = 0;
   strlcpy (sim_name, "VAX 8200 (KA820)", sizeof (sim_name));
   }
else if (strcmp(cptr, "8250") == 0) {
   sys_model = 1;
   strlcpy (sim_name, "VAX 8250 (KA825)", sizeof (sim_name));
   }
else
   return SCPE_ARG;
return SCPE_OK;
}

t_stat cpu_print_model (FILE *st)
{
fprintf (st, "VAX %s", (sys_model ? "8250" : "8200"));
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

fprintf (st, "Initial memory size is 32MB.\n\n");
fprintf (st, "The simulator is booted with the BOOT command:\n\n");
fprintf (st, "   sim> BO{OT} <device>{/R5:flags}\n\n");
fprintf (st, "where <device> is one of:\n\n");
fprintf (st, "   HKn        to boot from hkn\n");
fprintf (st, "   RLn        to boot from rln\n");
fprintf (st, "   RQn        to boot from rqn\n");
fprintf (st, "   RQBn       to boot from rqbn\n");
fprintf (st, "   RQCn       to boot from rqcn\n");
fprintf (st, "   RQDn       to boot from rqdn\n");
fprintf (st, "   KDBn       to boot from kdbn\n");
fprintf (st, "   TQn        to boot from tqn\n");
fprintf (st, "   CS         to boot from console RL\n\n");
return SCPE_OK;
}
