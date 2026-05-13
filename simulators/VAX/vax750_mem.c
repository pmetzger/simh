/* vax750_mem.c: VAX 11/750 memory controllers */
// SPDX-FileCopyrightText: 2010-2012 Matt Burke
// SPDX-License-Identifier: X11

/*
   mctl               MS750 memory controller

   21-Oct-2012  MB      First Version
*/

#include <stdbool.h>
#include <stdint.h>

#include "vax_defs.h"
#include "vax_cpu.h"
#include "vax750_cmi.h"
#include "vax750_mem.h"
#include "vax750_mem_internal.h"
#include "uint_bits.h"

#ifndef DONT_USE_INTERNAL_ROM
#include "vax_ka750_bin_old.h" /* Defines BOOT_CODE_FILENAME and BOOT_CODE_ARRAY, etc */
#undef BOOT_CODE_FILENAME
#undef BOOT_CODE_SIZE
#undef BOOT_CODE_ARRAY
#include "vax_ka750_bin_new.h" /* Defines BOOT_CODE_FILENAME and BOOT_CODE_ARRAY, etc */
#endif /* DONT_USE_INTERNAL_ROM */

/* Memory adapter register 0 */

#define MCSR0_OF        0x00
#define MCSR0_ES        0x0000007F                      /* Error syndrome */
#define MCSR0_V_EP      9
#define MCSR0_M_EP      0x7FFF
#define MCSR0_EP        (MCSR0_M_EP << MCSR0_V_EP)      /* Error page */
#define MCSR0_CRD       0x20000000                      /* Corrected read data */
#define MCSR0_RDSH      0x40000000                      /* Read data subs high */
#define MCSR0_RDS       0x80000000                      /* Read data substitute */
#define MCSR0_RS        (MCSR0_CRD | MCSR0_RDSH | MCSR0_RDS)

/* Memory adapter register 1 */

#define MCSR1_OF        0x01
#define MCSR1_CS        0x0000007F                      /* Check syndrome */
#define MCSR1_V_EP      9
#define MCSR1_M_EP      0x7FFF
#define MCSR1_EP        (MCSR1_M_EP << MCSR1_V_EP)      /* Page mode address */
#define MCSR1_ECCD      0x02000000                      /* ECC disable */
#define MCSR1_DIAG      0x04000000                      /* Diag mode */
#define MCSR1_PM        0x08000000                      /* Page mode */
#define MCSR1_CRE       0x10000000                      /* CRD enable */
#define MCSR1_RW        (MCSR1_CS | MCSR1_ECCD | MCSR1_DIAG | \
                         MCSR1_PM | MCSR1_CRE)

/* Debug switches */

#define MCTL_DEB_RRD     0x01                            /* reg reads */
#define MCTL_DEB_RWR     0x02                            /* reg writes */

static uint32_t mcsr0 = 0;
static uint32_t mcsr1 = 0;
static uint32_t mcsr2 = 0;

uint32_t rom[ROMSIZE/sizeof(uint32_t)];                 /* boot ROM */

static t_stat mctl_reset (DEVICE *dptr);
static const char *mctl_description (DEVICE *dptr);
static t_stat mctl_rdreg (int32_t *val, int32_t pa, int32_t mode);
static t_stat mctl_wrreg (int32_t val, int32_t pa, int32_t mode);

/* MCTL data structures

   mctl_dev    MCTL device descriptor
   mctl_unit   MCTL unit
   mctl_reg    MCTL register list
*/

static DIB mctl_dib = { TR_MCTL, 0, &mctl_rdreg, &mctl_wrreg, 0 };

static UNIT mctl_unit = { UDATA (NULL, 0, 0) };

static REG mctl_reg[] = {
    { HRDATAD (CSR0, mcsr0, 32, "ECC syndrome bits") },
    { HRDATAD (CSR1, mcsr1, 32, "CPU error control/check bits") },
    { HRDATAD (CSR2, mcsr2, 32, "Memory Configuration") },
    { BRDATAD (ROM,    rom, 16, 32, 256, "Bootstrap ROM") },
    { NULL }
    };

static MTAB mctl_mod[] = {
    { MTAB_XTD|MTAB_VDV, TR_MCTL, "NEXUS", NULL,
      NULL, &show_nexus, NULL, "Display Nexus" },
    { 0 }
    };

static DEBTAB mctl_deb[] = {
    { "REGREAD", MCTL_DEB_RRD },
    { "REGWRITE", MCTL_DEB_RWR },
    { NULL, 0 }
    };

DEVICE mctl_dev = {
    "MCTL", &mctl_unit, mctl_reg, mctl_mod,
    1, 16, 16, 1, 16, 8,
    NULL, NULL, &mctl_reset,
    NULL, NULL, NULL,
    &mctl_dib, DEV_NEXUS | DEV_DEBUG, 0,
    mctl_deb, NULL, NULL, NULL, NULL, NULL,
    &mctl_description
    };

/* Memory controller register read */

static t_stat mctl_rdreg (int32_t *val, int32_t pa, int32_t lnt)
{
/* Nexus register read signature.
   This implementation does not use every parameter. */
(void) lnt;

int32_t ofs;
ofs = NEXUS_GETOFS (pa);                                /* get offset */

switch (ofs) {                                          /* case on offset */

    case MCSR0_OF:                                      /* CSR0 */
        *val = mcsr0;
        break;

    case MCSR1_OF:                                      /* CSR1 */
        *val = mcsr1;
        break;

    case MCSR2_OF:                                      /* CSR2 */
        *val = mcsr2 & ~MCSR2_MBZ;
        break;

    default:
        return SCPE_NXM;
    }

if (DEBUG_PRI (mctl_dev, MCTL_DEB_RRD))
    fprintf (sim_deb, ">>MCTL: reg %d(%x) read, value = %X\n", ofs, pa, *val);

return SCPE_OK;
}

/* Memory controller register write */

static t_stat mctl_wrreg (int32_t val, int32_t pa, int32_t lnt)
{
/* Nexus register write signature.
   This implementation does not use every parameter. */
(void) lnt;

int32_t ofs;

ofs = NEXUS_GETOFS (pa);                                /* get offset */

switch (ofs) {                                          /* case on offset */

    case MCSR0_OF:                                      /* CSR0 */
        mcsr0 = mcsr0 & ~(MCSR0_RS & val);
        break;

    case MCSR1_OF:                                      /* CSR1 */
        mcsr1 = val & MCSR1_RW;
        break;

    case MCSR2_OF:                                      /* CSR2 */
        break;

    default:
        return SCPE_NXM;
    }

if (DEBUG_PRI (mctl_dev, MCTL_DEB_RWR))
    fprintf (sim_deb, ">>MCTL: reg %d(%x) write, value = %X\n", ofs, pa, val);

return SCPE_OK;
}

/* Used by CPU */

void rom_wr_B (int32_t pa, int32_t val)
{
int32_t rg = ((pa - ROMBASE) & ROMAMASK) >> 2;

rom[rg] = u32_put_addr_u8_le(rom[rg], (uint32_t)val, (uint32_t)pa);
return;
}

/* Memory controller reset */

static t_stat mctl_reset (DEVICE *dptr)
{
/* Generic device reset signature.
   This implementation does not use every parameter. */
(void) dptr;

mcsr0 = 0;
mcsr1 = 0;
mcsr2 = vax750_mcsr2_reset_value((uint32_t)MEMSIZE);
return SCPE_OK;
}

t_stat mctl_populate_rom (const char *rom_filename)
{
#ifdef DONT_USE_INTERNAL_ROM
return cpu_load_bootcode (rom_filename, NULL, sizeof (rom), true, 0);
#else
if (strcmp (rom_filename, "ka750_new.bin") == 0)
    return cpu_load_bootcode ("ka750_new.bin", vax_ka750_bin_new, BOOT_CODE_SIZE, true, 0);
else
    return cpu_load_bootcode ("ka750_old.bin", vax_ka750_bin_old, BOOT_CODE_SIZE, true, 0);
#endif
}

static const char *mctl_description (DEVICE *dptr)
{
/* Generic device description signature.
   This implementation does not use every parameter. */
(void) dptr;

return "Memory controller";
}

t_stat cpu_show_memory (FILE* st, UNIT* uptr, int32_t val, const void* desc)
{
/* Generic show modifier signature.
   This implementation does not use every parameter. */
(void) uptr;
(void) val;
(void) desc;

uint32_t baseaddr = 0;
struct {
    uint32_t capacity;
    const char *option;
    } boards[] = {
        { 4096, "MS750-JD M7199"},
        { 1024, "MS750-CA M8750"},
        {  256, "MS750-AA M8728"},
        {    0, NULL}};
int32_t i, bd = 0;

for (i=0; i<8; i++) {
    if (mcsr2&MCSR2_CS256) {
        switch ((mcsr2&(3<<(i*2)))>>(i*2)) {
            case 0:
            case 3:
                bd = 3;         /* Not Present */
                break;
            case 2:
                bd = 1;         /* 64Kb chips */
                break;
            case 1:
                bd = 0;         /* 256Kb chips */
                break;
            }
        }
    else {
        switch ((mcsr2&(3<<(i*2)))>>(i*2)) {
            case 0:
                bd = 3;         /* Not Present */
                break;
            case 3:
                bd = 2;         /* 16Kb chips */
                break;
            case 1:
            case 2:
                bd = 1;         /* 64Kb chips */
                break;
            }
        }
    if (boards[bd].capacity)
        fprintf(st, "Memory slot %d (@0x%08x): %3d %sbytes (%s)\n", 11+i, baseaddr, boards[bd].capacity/((boards[bd].capacity>=1024) ? 1024 : 1), (boards[bd].capacity>=1024) ? "M" : "K", boards[bd].option);
    baseaddr += boards[bd].capacity<<10;
    }
return SCPE_OK;
}
