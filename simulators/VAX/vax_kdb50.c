/* vax_kdb50.c: VAXBI KDB50 disk controller */
// SPDX-FileCopyrightText: 2026 The ZIMH contributors
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stdint.h>

#include "vax_defs.h"
#include "sim_disk.h"
#include "vax_kdb50.h"
#include "vax_kdb50_internal.h"
#include "vax_mscp.h"

typedef struct {
    uint16_t sa;
    uint16_t saw;
    uint32_t ip_reads;
} KDB50_PORT;

/*
 * Real KDB50 completions are asynchronous to the host's IP poll. A short
 * delay avoids delivering the response interrupt while a driver is still
 * returning from the poll and before it has blocked for completion.
 */
#define KDB50_RESPONSE_DELAY 100000
#define KDB50_DTYPE_REVISION 0u
#define KDB50_BER_RD 0x7FFF000Fu
#define KDB50_BER_W1C 0x7FFF0007u
#define KDB50_BER_HARD_ERROR 0x7FFF0000u
#define KDB50_BER_SOFT_ERROR 0x00000007u
#define KDB50_UIIC_FORCE (1u << (BIICR_V_FRC + IPL_KDB50))
#define KDB50_UIIC_SENT (1u << (BIICR_V_SNT + IPL_KDB50))
#define KDB50_UIIC_COMPLETE (1u << (BIICR_V_ITC + IPL_KDB50))
#define KDB50_UIIC_ABORT (1u << (28 + IPL_KDB50))
#define KDB50_PROCESSOR_DESTINATIONS ((1u << TR_KA0) | (1u << TR_KA1))

/*
 * Profile values combine KDB50 controller identity, RA81 unit identity, and
 * the VAXBI mapped-buffer format described by the KDB50 User Guide, MSCP, and
 * SSP specifications.
 */
static const vax_mscp_profile kdb50_mscp_profile = {
    .controller_model = KDB50_MSCP_CONTROLLER_MODEL,
    .controller_hw_version = KDB50_MSCP_HW_VERSION,
    .controller_sw_version = KDB50_MSCP_SW_VERSION,
    .controller_timeout = KDB50_MSCP_DEFAULT_TIMEOUT,
    .interrupt_vector_external = 1u,
    .max_units = KDB50_MSCP_MAX_UNITS,
    .unit_model = KDB50_MSCP_RA81_UNIT_MODEL,
    .sectors_per_track = KDB50_MSCP_RA81_SECTORS_PER_TRACK,
    .tracks_per_group = KDB50_MSCP_RA81_TRACKS_PER_GROUP,
    .groups_per_cylinder = KDB50_MSCP_RA81_GROUPS_PER_CYLINDER,
    .unit_version = 0,
    .media_id = KDB50_MSCP_RA81_MEDIA_ID,
    .buffer_map_bit = KDB50_MSCP_BUFFER_MAP,
    .phys_addr_mask = KDB50_MSCP_PHYS_ADDR_MASK,
    .bus_addr_max = KDB50_MSCP_BUS_ADDR_MAX,
    .mapped_mapbase_invalid = KDB50_MSCP_MAPPED_MAPBASE_INVALID,
    .mapped_descriptor_max = KDB50_MSCP_MAPPED_DESCRIPTOR_MAX,
    .unmapped_max_addr = KDB50_MSCP_UNMAPPED_MAX_ADDR,
    .pte_valid = KDB50_MSCP_PTE_VALID,
    .pte_pfn_mask = KDB50_MSCP_PTE_PFN_MASK,
    .page_bytes = KDB50_MSCP_PAGE_BYTES,
    .forced_error_blocks = KDB50_RA81_BLOCKS,
};

static BIIC kdb50_biic;
static vax_mscp kdb50_mscp;
static KDB50_PORT kdb50_port;
static uint32_t kdb50_eicr_requested_levels;

static t_stat kdb50_reset(DEVICE *dptr);
static t_stat kdb50_svc(UNIT *uptr);
static t_stat kdb50_attach(UNIT *uptr, const char *cptr);
static t_stat kdb50_detach(UNIT *uptr);
static t_stat kdb50_set_type(UNIT *uptr, int32_t val, const char *cptr,
                             void *desc);
static t_stat kdb50_rdreg(int32_t *val, int32_t pa, int32_t lnt);
static t_stat kdb50_wrreg(int32_t val, int32_t pa, int32_t lnt);
static int kdb50_read_words(void *ctx, uint32_t addr, uint16_t *buf,
                            uint32_t bytes);
static int kdb50_read_bytes(void *ctx, uint32_t addr, uint8_t *buf,
                            uint32_t bytes);
static int kdb50_write_words(void *ctx, uint32_t addr, const uint16_t *buf,
                             uint32_t bytes);
static int kdb50_write_bytes(void *ctx, uint32_t addr, const uint8_t *buf,
                             uint32_t bytes);
static int kdb50_read_sectors(void *ctx, uint16_t unit, uint32_t lbn,
                              uint8_t *buf, uint32_t sectors);
static int kdb50_write_sectors(void *ctx, uint16_t unit, uint32_t lbn,
                               const uint8_t *buf, uint32_t sectors);
static int kdb50_read_map_pte(void *ctx, uint32_t mapbase, uint32_t page_index,
                              uint32_t *pte);
static void kdb50_ring_interrupt(void *ctx);
static void kdb50_update_error_interrupts(void);
static void kdb50_update_user_interrupts(void);

static bool kdb50_intrdes_targets_processor(void)
{
    return (kdb50_biic.idest & KDB50_PROCESSOR_DESTINATIONS) != 0;
}

static DIB kdb50_dib = {KDB50_DEFAULT_NEXUS, 0, &kdb50_rdreg,
                        &kdb50_wrreg,        0, 0};

static UNIT kdb50_unit[] = {
    {UDATA(&kdb50_svc, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE, 0)},
    {UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE, 0)},
    {UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE, 0)},
    {UDATA(NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE, 0)}};

static REG kdb50_reg[] = {{HRDATA(BICSR, kdb50_biic.csr, 32)},
                          {HRDATA(BIBER, kdb50_biic.ber, 32)},
                          {HRDATA(BIECR, kdb50_biic.eicr, 32)},
                          {HRDATA(BIDEST, kdb50_biic.idest, 32)},
                          {HRDATA(BIUIIC, kdb50_biic.uiic, 32)},
                          {HRDATA(SA, kdb50_port.sa, 16)},
                          {HRDATA(SAW, kdb50_port.saw, 16)},
                          {NULL}};

static MTAB kdb50_mod[] = {
    {MTAB_XTD | MTAB_VDV, KDB50_DEFAULT_NEXUS, "NEXUS", NULL, NULL, &show_nexus,
     NULL, "Display nexus"},
    {MTAB_XTD | MTAB_VUN, 0, NULL, "KDB50", &kdb50_set_type, NULL, NULL,
     "Accept generic KDB50 disk type"},
    {MTAB_XTD | MTAB_VUN, KDB50_RA81_BLOCKS, NULL, "RA81", &kdb50_set_type,
     NULL, NULL, "Accept RA81 disk image type"},
    {0}};

DEVICE kdb50_dev = {"KDB",
                    kdb50_unit,
                    kdb50_reg,
                    kdb50_mod,
                    4,
                    16,
                    16,
                    1,
                    16,
                    8,
                    NULL,
                    NULL,
                    &kdb50_reset,
                    NULL,
                    kdb50_attach,
                    kdb50_detach,
                    &kdb50_dib,
                    DEV_NEXUS | DEV_DISABLE | DEV_DIS | DEV_DISK | DEV_SECTORS,
                    0};

/* Return the KDB50 byte offset within the addressed VAXBI node. */
static uint32_t kdb50_byte_offset(uint32_t pa)
{
    return pa & ((1u << REG_V_NEXUS) - 1u);
}

/* Read words from VAX physical memory for the KDB50 MSCP engine. */
static int kdb50_read_words(void *ctx, uint32_t addr, uint16_t *buf,
                            uint32_t bytes)
{
    (void)ctx;

    for (uint32_t i = 0; i < bytes; i += 2)
        *buf++ = (uint16_t)ReadW(addr + i);
    return 0;
}

/* Read bytes from VAX physical memory for the KDB50 MSCP engine. */
static int kdb50_read_bytes(void *ctx, uint32_t addr, uint8_t *buf,
                            uint32_t bytes)
{
    (void)ctx;

    for (uint32_t i = 0; i < bytes; i++)
        *buf++ = (uint8_t)ReadB(addr + i);
    return 0;
}

/* Write words to VAX physical memory for the KDB50 MSCP engine. */
static int kdb50_write_words(void *ctx, uint32_t addr, const uint16_t *buf,
                             uint32_t bytes)
{
    (void)ctx;

    for (uint32_t i = 0; i < bytes; i += 2)
        WriteW(addr + i, *buf++);
    return 0;
}

/* Write bytes to VAX physical memory for the KDB50 MSCP engine. */
static int kdb50_write_bytes(void *ctx, uint32_t addr, const uint8_t *buf,
                             uint32_t bytes)
{
    (void)ctx;

    for (uint32_t i = 0; i < bytes; i++)
        WriteB(addr + i, *buf++);
    return 0;
}

/* Read disk sectors for the KDB50 MSCP engine. */
static int kdb50_read_sectors(void *ctx, uint16_t unit, uint32_t lbn,
                              uint8_t *buf, uint32_t sectors)
{
    t_seccnt done = 0;
    t_stat status;

    (void)ctx;
    if (unit >= kdb50_mscp_profile.max_units)
        return -1;
    status = sim_disk_rdsect(&kdb50_unit[unit], lbn, buf, &done, sectors);
    return (status == SCPE_OK && done == sectors) ? 0 : -1;
}

/* Write disk sectors for the KDB50 MSCP engine. */
static int kdb50_write_sectors(void *ctx, uint16_t unit, uint32_t lbn,
                               const uint8_t *buf, uint32_t sectors)
{
    t_seccnt done = 0;
    t_stat status;

    (void)ctx;
    if (unit >= kdb50_mscp_profile.max_units)
        return -1;
    status =
        sim_disk_wrsect(&kdb50_unit[unit], lbn, (uint8_t *)buf, &done, sectors);
    return (status == SCPE_OK && done == sectors) ? 0 : -1;
}

/*
 * Read a mapped-transfer PTE. SSP 3.1 section 2.3.7.2 defines the map
 * register base as a physical address and requires the controller to access
 * consecutive longword map registers with unmapped addressing.
 */
static int kdb50_read_map_pte(void *ctx, uint32_t mapbase, uint32_t page_index,
                              uint32_t *pte)
{
    uint32_t pte_addr = mapbase + (page_index * 4u);

    (void)ctx;
    *pte = (uint32_t)ReadL(pte_addr);
    return 0;
}

/* Raise a KDB50 BI interrupt after an MSCP response ring transition. */
static void kdb50_ring_interrupt(void *ctx)
{
    uint32_t vector = kdb50_biic.uiic & BIICR_VEC;

    (void)ctx;

    if (vector == 0)
        return;
    /*
     * Reasserting a KDB50 user interrupt is equivalent to deasserting and
     * reasserting the BIIC request; otherwise INTRC blocks another interrupt
     * at the same level.
     */
    kdb50_biic.uiic &=
        ~(BIICR_VEC | KDB50_UIIC_FORCE | KDB50_UIIC_SENT | KDB50_UIIC_COMPLETE);
    kdb50_biic.uiic |= KDB50_UIIC_FORCE | vector;
    sim_activate(&kdb50_unit[0], KDB50_RESPONSE_DELAY);
}

/* Return true when the user-interface register still owns this IPL request. */
static bool kdb50_uiic_request_active(int32_t lvl)
{
    return (lvl == IPL_KDB50) && ((kdb50_biic.uiic & KDB50_UIIC_FORCE) != 0) &&
           ((kdb50_biic.uiic & KDB50_UIIC_COMPLETE) == 0);
}

/* Return true when the BIIC error interrupt register has an active source. */
static bool kdb50_error_source_active(void)
{
    bool hard_error = (kdb50_biic.ber & KDB50_BER_HARD_ERROR) != 0;
    bool soft_error = (kdb50_biic.ber & KDB50_BER_SOFT_ERROR) != 0;

    if (kdb50_biic.eicr & BIECR_FRC)
        return true;
    if (hard_error && (kdb50_biic.csr & BICSR_HIE))
        return true;
    if (soft_error && (kdb50_biic.csr & BICSR_SIE))
        return true;
    return false;
}

/* Update nexus requests driven by the KDB50 BIIC error interrupt register. */
static void kdb50_update_error_interrupts(void)
{
    uint32_t new_levels = 0;
    uint32_t level_mask = (kdb50_biic.eicr >> BIECR_V_LVL) & BIECR_M_LVL;

    if (!kdb50_error_source_active())
        kdb50_biic.eicr &= ~(BIECR_COM | BIECR_SNT);

    if (kdb50_error_source_active() && !(kdb50_biic.eicr & BIECR_COM)) {
        if (kdb50_intrdes_targets_processor())
            new_levels = level_mask;
        else
            kdb50_biic.eicr =
                (kdb50_biic.eicr & ~BIECR_SNT) | BIECR_ABO | BIECR_COM;
    }

    for (int32_t lvl = 0; lvl < NEXUS_HLVL; lvl++) {
        uint32_t bit = 1u << lvl;

        if ((kdb50_eicr_requested_levels & bit) && !(new_levels & bit) &&
            !kdb50_uiic_request_active(lvl))
            nexus_req[lvl] &= ~(1u << TR_KDB50);
        if (new_levels & bit) {
            kdb50_biic.eicr |= BIECR_SNT;
            nexus_req[lvl] |= (1u << TR_KDB50);
        }
    }
    kdb50_eicr_requested_levels = new_levels;
}

/*
 * Reconcile a pending user-interface interrupt with INTRDES.  The BIIC
 * application note says an interrupt with no selected destination is marked
 * aborted in the BIIC rather than delivered.
 */
static void kdb50_update_user_interrupts(void)
{
    if (((kdb50_biic.uiic & KDB50_UIIC_FORCE) == 0) ||
        ((kdb50_biic.uiic & KDB50_UIIC_COMPLETE) != 0))
        return;

    if (kdb50_intrdes_targets_processor())
        return;

    kdb50_biic.uiic = (kdb50_biic.uiic & ~KDB50_UIIC_SENT) | KDB50_UIIC_ABORT |
                      KDB50_UIIC_COMPLETE;
    if ((kdb50_eicr_requested_levels & (1u << IPL_KDB50)) == 0)
        nexus_req[IPL_KDB50] &= ~(1u << TR_KDB50);
}

static t_stat kdb50_svc(UNIT *uptr)
{
    (void)uptr;

    if (((kdb50_biic.uiic & KDB50_UIIC_FORCE) != 0) &&
        ((kdb50_biic.uiic & KDB50_UIIC_COMPLETE) == 0)) {
        if (kdb50_intrdes_targets_processor()) {
            kdb50_biic.uiic |= KDB50_UIIC_SENT;
            SET_NEXUS_INT(KDB50);
        } else
            kdb50_update_user_interrupts();
    }
    return SCPE_OK;
}

/* Return the vector supplied by a KDB50 interrupt register at this level. */
int32_t kdb50_get_vector(int32_t lvl)
{
    int32_t vector;

    if ((lvl >= 0) && (lvl < NEXUS_HLVL) &&
        (kdb50_eicr_requested_levels & (1u << lvl)) &&
        kdb50_error_source_active()) {
        vector = kdb50_biic.eicr & BIECR_VEC;
        kdb50_biic.eicr &= ~BIECR_SNT;
        kdb50_biic.eicr |= BIECR_COM;
        kdb50_update_error_interrupts();
        return vector;
    }

    vector = kdb50_biic.uiic & BIICR_VEC;
    kdb50_biic.uiic &= ~KDB50_UIIC_SENT;
    kdb50_biic.uiic |= KDB50_UIIC_COMPLETE;
    if ((lvl >= 0) && (lvl < NEXUS_HLVL))
        nexus_req[lvl] &= ~(1u << TR_KDB50);
    kdb50_update_error_interrupts();
    return vector;
}

/* Publish currently attached KDB50 units to the MSCP engine. */
static void kdb50_sync_units(void)
{
    for (uint16_t i = 0; i < kdb50_mscp_profile.max_units; i++) {
        uint32_t blocks = 0;

        if (kdb50_unit[i].flags & UNIT_ATT)
            blocks = (uint32_t)kdb50_unit[i].capac;
        vax_mscp_set_unit(&kdb50_mscp, i, blocks);
    }
}

/* Reset the KDB50 BIIC-visible state after self-test completion. */
static t_stat kdb50_reset(DEVICE *dptr)
{
    DIB *dibp = (DIB *)dptr->ctxt;
    uint32_t nexus = dibp ? dibp->ba : KDB50_DEFAULT_NEXUS;

    sim_cancel(&kdb50_unit[0]);
    kdb50_biic.dtype = (KDB50_DTYPE_REVISION << 16) | DTYPE_KDB50;
    kdb50_biic.csr = (1u << BICSR_V_IF) | BICSR_STS | (nexus & BICSR_NODE);
    kdb50_biic.ber = 0;
    kdb50_biic.eicr = 0;
    kdb50_biic.idest = 0;
    kdb50_biic.imsk = 0;
    kdb50_biic.fidest = 0;
    kdb50_biic.isrc = 0;
    kdb50_biic.sa = 0;
    kdb50_biic.ea = 0;
    kdb50_biic.bcic = BIBCI_UCE | BIBCI_BIE | BIBCI_INE;
    kdb50_biic.wsts = 0;
    kdb50_biic.ficmd = 0;
    kdb50_biic.uiic = BIICR_EXV;
    kdb50_biic.gpr0 = 0;
    kdb50_biic.gpr1 = 0;
    kdb50_biic.gpr2 = 0;
    kdb50_biic.gpr3 = 0;
    kdb50_biic.sosr = 0;
    kdb50_biic.rxcd = 0;
    kdb50_eicr_requested_levels = 0;
    for (int32_t i = 0; i < NEXUS_HLVL; i++)
        nexus_req[i] &= ~(1u << TR_KDB50);

    kdb50_port.sa = 0;
    kdb50_port.saw = 0;
    kdb50_port.ip_reads = 0;

    const vax_mscp_bus bus = {
        .ctx = NULL,
        .profile = &kdb50_mscp_profile,
        .read_words = kdb50_read_words,
        .read_bytes = kdb50_read_bytes,
        .write_words = kdb50_write_words,
        .write_bytes = kdb50_write_bytes,
        .read_sectors = kdb50_read_sectors,
        .write_sectors = kdb50_write_sectors,
        .read_map_pte = kdb50_read_map_pte,
        .ring_interrupt = kdb50_ring_interrupt,
    };
    if (!vax_mscp_reset_with_bus(&kdb50_mscp, &bus))
        return SCPE_IERR;
    kdb50_sync_units();
    kdb50_port.sa = vax_mscp_read_sa(&kdb50_mscp);

    return SCPE_OK;
}

/* Attach a disk image to a KDB50 unit. */
static t_stat kdb50_attach(UNIT *uptr, const char *cptr)
{
    t_stat r = sim_disk_attach_ex(uptr, cptr, 512, sizeof(uint16_t), false, 0,
                                  "KDB50", 0, 0, NULL);

    if (r == SCPE_OK)
        kdb50_sync_units();
    return r;
}

/* Detach a disk image from a KDB50 unit. */
static t_stat kdb50_detach(UNIT *uptr)
{
    t_stat r = sim_disk_detach(uptr);

    if (r == SCPE_OK)
        kdb50_sync_units();
    return r;
}

/* Accept disk image type names used by SIMH disk containers. */
static t_stat kdb50_set_type(UNIT *uptr, int32_t val, const char *cptr,
                             void *desc)
{
    (void)cptr;
    (void)desc;

    if (uptr->flags & UNIT_ATT)
        return SCPE_ALATT;
    if (val > 0)
        uptr->capac = (t_addr)val;
    return SCPE_OK;
}

/* Read a KDB50 BIIC or KDB50-specific register. */
static t_stat kdb50_rdreg(int32_t *val, int32_t pa, int32_t lnt)
{
    uint32_t byte_offset = kdb50_byte_offset((uint32_t)pa);
    uint32_t ofs = NEXUS_GETOFS(pa);

    if ((lnt == L_WORD) && (byte_offset == KDB50_IP_OF)) {
        *val = vax_mscp_read_ip(&kdb50_mscp);
        kdb50_port.ip_reads++;
        return SCPE_OK;
    }

    if ((lnt == L_WORD) && (byte_offset == KDB50_SA_READ_OF)) {
        kdb50_port.sa = vax_mscp_read_sa(&kdb50_mscp);
        *val = kdb50_port.sa;
        return SCPE_OK;
    }

    switch (ofs) {
    case BI_DTYPE:
        *val = kdb50_biic.dtype;
        break;
    case BI_CSR:
        *val = kdb50_biic.csr & BICSR_RD;
        break;
    case BI_BER:
        *val = kdb50_biic.ber & KDB50_BER_RD;
        break;
    case BI_EICR:
        *val = kdb50_biic.eicr & BIECR_RD;
        break;
    case BI_IDEST:
        *val = kdb50_biic.idest & BIID_RD;
        break;
    case BI_IMSK:
    case BI_FIDEST:
    case BI_ISRC:
    case BI_SA:
    case BI_EA:
        *val = 0;
        break;
    case BI_BCIC:
        *val = kdb50_biic.bcic & BIBCI_RD;
        break;
    case BI_WSTS:
    case BI_FICMD:
        *val = 0;
        break;
    case BI_UIIC:
        *val = kdb50_biic.uiic & BIICR_RD;
        break;
    case BI_GPR0:
        if (lnt == L_LONG) {
            *val = vax_mscp_read_ip(&kdb50_mscp);
            kdb50_port.ip_reads++;
        } else
            *val = 0;
        break;
    case BI_GPR1:
        if (lnt == L_LONG)
            kdb50_port.sa = vax_mscp_read_sa(&kdb50_mscp);
        *val = ((uint32_t)kdb50_port.saw << 16) | kdb50_port.sa;
        break;
    case BI_GPR2:
    case BI_GPR3:
        *val = 0;
        break;
    default:
        return SCPE_NXM;
    }

    return SCPE_OK;
}

/* Write a KDB50 BIIC or KDB50-specific register. */
static t_stat kdb50_wrreg(int32_t val, int32_t pa, int32_t lnt)
{
    uint32_t byte_offset = kdb50_byte_offset((uint32_t)pa);
    uint32_t ofs = NEXUS_GETOFS(pa);

    if ((lnt == L_WORD) && (byte_offset == KDB50_IP_OF))
        return SCPE_OK;

    if ((lnt == L_WORD) && (byte_offset == KDB50_SA_WRITE_OF)) {
        kdb50_port.saw = (uint16_t)val;
        vax_mscp_write_sa(&kdb50_mscp, kdb50_port.saw);
        return SCPE_OK;
    }

    switch (ofs) {
    case BI_CSR:
        if ((val & BICSR_RST) && kdb50_reset(&kdb50_dev) != SCPE_OK)
            return SCPE_IERR;
        kdb50_biic.csr = (kdb50_biic.csr & ~BICSR_RW) | (val & BICSR_RW);
        break;
    case BI_BER:
        kdb50_biic.ber &= ~(val & KDB50_BER_W1C);
        kdb50_update_error_interrupts();
        break;
    case BI_EICR:
        kdb50_biic.eicr = (kdb50_biic.eicr & ~BIECR_RW) | (val & BIECR_RW);
        kdb50_biic.eicr &= ~(val & BIECR_W1C);
        kdb50_update_error_interrupts();
        break;
    case BI_IDEST:
        kdb50_biic.idest = val & BIID_RW;
        kdb50_update_error_interrupts();
        kdb50_update_user_interrupts();
        break;
    case BI_BCIC:
        kdb50_biic.bcic = val & BIBCI_RW;
        break;
    case BI_UIIC:
        kdb50_biic.uiic = (kdb50_biic.uiic & ~BIICR_RW) | (val & BIICR_RW);
        kdb50_biic.uiic &= ~(val & BIICR_W1C);
        if ((kdb50_biic.uiic & KDB50_UIIC_FORCE) == 0) {
            kdb50_biic.uiic &= ~(KDB50_UIIC_SENT | KDB50_UIIC_COMPLETE);
            nexus_req[IPL_KDB50] &= ~(1u << TR_KDB50);
            kdb50_update_error_interrupts();
        }
        break;
    case BI_GPR0:
        break;
    case BI_GPR1:
        if (lnt == L_LONG) {
            kdb50_port.saw = (uint16_t)((uint32_t)val >> 16);
            vax_mscp_write_sa(&kdb50_mscp, kdb50_port.saw);
        }
        break;
    case BI_GPR2:
    case BI_GPR3:
        break;
    default:
        return SCPE_NXM;
    }

    return SCPE_OK;
}
