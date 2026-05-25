#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"

uint32_t *M;
uint32_t R[16];
uint32_t PSL;
uint32_t SBR;
uint32_t SISR;
uint32_t fault_PC;
uint32_t p1;
uint32_t p2;
uint32_t trpirq;
uint32_t mchk_va;
uint32_t nexus_req[NEXUS_HLVL];
uint32_t mchk_ref;
int32_t mapen;
int32_t hlt_pin;
int32_t crd_err;
jmp_buf save_env;
DEVICE cpu_dev;
UNIT cpu_unit;

static uint32_t test_memory[0x210000u >> 2];

int32_t eval_int(void)
{
    return 0;
}

t_stat show_nexus(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

int32_t ReadIO(uint32_t pa, int32_t lnt)
{
    (void)pa;
    (void)lnt;
    fail_msg("unexpected ReadIO");
    return 0;
}

void WriteIO(uint32_t pa, int32_t val, int32_t lnt)
{
    (void)pa;
    (void)val;
    (void)lnt;
    fail_msg("unexpected WriteIO");
}

int32_t ReadReg(uint32_t pa, int32_t lnt)
{
    (void)pa;
    (void)lnt;
    fail_msg("unexpected ReadReg");
    return 0;
}

void WriteReg(uint32_t pa, int32_t val, int32_t lnt)
{
    (void)pa;
    (void)val;
    (void)lnt;
    fail_msg("unexpected WriteReg");
}

/*
 * Include the implementation directly so these tests can exercise the
 * KDB50's BI-visible register behavior without exporting test-only hooks.
 */
#include "vax_kdb50.c"

static uint32_t kdb_pa(uint32_t byte_offset)
{
    return (KDB50_DEFAULT_NEXUS << REG_V_NEXUS) | byte_offset;
}

static void reset_kdb50(void)
{
    memset(nexus_req, 0, sizeof(nexus_req));
    memset(test_memory, 0xa5, sizeof(test_memory));
    M = test_memory;
    cpu_unit.capac = sizeof(test_memory);
    assert_int_equal(kdb50_reset(&kdb50_dev), SCPE_OK);
}

static void target_kdb50_interrupts_to_primary_processor(void)
{
    assert_int_equal(
        kdb50_wrreg(1u << TR_KA0, kdb_pa(BI_IDEST << REG_V_OFS), L_LONG),
        SCPE_OK);
}

static void write_phys_word(uint32_t addr, uint16_t value)
{
    WriteW(addr, value);
}

static void write_phys_long(uint32_t addr, uint32_t value)
{
    write_phys_word(addr, value & 0xffffu);
    write_phys_word(addr + 2u, value >> 16);
}

static t_stat short_read_sector(UNIT *uptr, t_lba lba, uint8_t *buf,
                                t_seccnt *sectsread, t_seccnt sects)
{
    (void)uptr;
    (void)lba;
    (void)buf;
    if (sectsread != NULL)
        *sectsread = sects - 1u;
    return SCPE_OK;
}

static t_stat short_write_sector(UNIT *uptr, t_lba lba, uint8_t *buf,
                                 t_seccnt *sectswritten, t_seccnt sects)
{
    (void)uptr;
    (void)lba;
    (void)buf;
    if (sectswritten != NULL)
        *sectswritten = sects - 1u;
    return SCPE_OK;
}

static void bring_kdb50_port_up(uint32_t comm)
{
    assert_int_equal(kdb50_wrreg(0x80a4, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(
        kdb50_wrreg(comm & 0xffffu, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
        SCPE_OK);
    assert_int_equal(kdb50_wrreg(comm >> 16, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_wrreg(0x0001, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
}

static void test_reset_exposes_kdb50_bi_node_identity(void **state)
{
    int32_t value;

    (void)state;

    reset_kdb50();

    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_DTYPE << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(value, DTYPE_KDB50);
    assert_int_equal((value >> 16) & 0xffff, 0);
    assert_int_equal(value & 0xffff, DTYPE_KDB50);

    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_CSR << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_true(value & BICSR_STS);
    assert_int_equal((value >> BICSR_V_IF) & BICSR_M_IF, 1);
    assert_int_equal(value & BICSR_NODE, KDB50_DEFAULT_NEXUS);
}

static void test_bicsr_node_reset_reinitializes_mscp_port(void **state)
{
    (void)state;

    reset_kdb50();
    assert_int_equal(
        kdb50_wrreg(BICSR_RST, kdb_pa(BI_CSR << REG_V_OFS), L_LONG), SCPE_OK);
    assert_int_equal(kdb50_port.sa, 0x0940);
}

static void test_reset_exposes_kdb50_bci_access_and_interrupts(void **state)
{
    int32_t value;

    (void)state;

    reset_kdb50();

    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(value, BIBCI_UCE | BIBCI_BIE | BIBCI_INE);
    assert_true(kdb50_biic.bcic & BIBCI_UCE);
    assert_true(kdb50_biic.bcic & BIBCI_BIE);
    assert_true(kdb50_biic.bcic & BIBCI_INE);
}

static void
test_kdb50_unimplemented_biic_registers_do_not_alias_bcic(void **state)
{
    static const uint32_t offsets[] = {
        BI_IMSK, BI_FIDEST, BI_ISRC, BI_SA, BI_EA,
    };
    int32_t value;

    (void)state;

    reset_kdb50();
    assert_int_equal(
        kdb50_wrreg(BIBCI_RW, kdb_pa(BI_BCIC << REG_V_OFS), L_LONG), SCPE_OK);
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        assert_int_equal(
            kdb50_rdreg(&value, kdb_pa(offsets[i] << REG_V_OFS), L_LONG),
            SCPE_OK);
        assert_int_equal(value, 0);
    }
}

static void test_kdb50_ber_read_masks_reserved_bit_and_w1c_errors(void **state)
{
    int32_t value;

    (void)state;

    reset_kdb50();
    kdb50_biic.ber =
        0x80000000u | BIBER_MPE | BIBER_BTO | BIBER_UPE | BIBER_CRD | BIBER_NPE;

    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_BER << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_false((uint32_t)value & 0x80000000u);
    assert_true((uint32_t)value & BIBER_MPE);
    assert_true((uint32_t)value & BIBER_UPE);

    assert_int_equal(
        kdb50_wrreg(BIBER_MPE | BIBER_CRD | BIBER_UPE | 0x80000000u,
                    kdb_pa(BI_BER << REG_V_OFS), L_LONG),
        SCPE_OK);
    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_BER << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_false((uint32_t)value & BIBER_MPE);
    assert_false((uint32_t)value & BIBER_CRD);
    assert_true((uint32_t)value & BIBER_BTO);
    assert_true((uint32_t)value & BIBER_NPE);
    assert_true((uint32_t)value & BIBER_UPE);
    assert_false((uint32_t)value & 0x80000000u);
}

static void test_kdb50_intrdes_masks_to_destination_word(void **state)
{
    int32_t value;

    (void)state;

    reset_kdb50();

    assert_int_equal(
        kdb50_wrreg(0xffff1234u, kdb_pa(BI_IDEST << REG_V_OFS), L_LONG),
        SCPE_OK);
    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_IDEST << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(value, 0x1234);
    assert_int_equal(kdb50_biic.idest, 0x1234);
}

static void test_kdb50_eintrcsr_deassert_clears_completion(void **state)
{
    int32_t level = IPL_KDB50;

    (void)state;

    reset_kdb50();
    kdb50_biic.eicr = BIECR_FRC | BIECR_COM | BIECR_SNT |
                      (1u << (BIECR_V_LVL + level)) | 0x0120;

    assert_int_equal(kdb50_wrreg(0, kdb_pa(BI_EICR << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_false(kdb50_biic.eicr & BIECR_FRC);
    assert_false(kdb50_biic.eicr & BIECR_COM);
    assert_false(kdb50_biic.eicr & BIECR_SNT);
    assert_int_equal(nexus_req[level] & (1u << TR_KDB50), 0);
}

static void test_kdb50_eintrcsr_force_uses_programmed_level(void **state)
{
    int32_t value;
    const int32_t level = IPL_KDB50 + 1;
    const int32_t vector = 0x0124;

    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();

    assert_int_equal(
        kdb50_wrreg(BIECR_FRC | (1u << (BIECR_V_LVL + level)) | vector,
                    kdb_pa(BI_EICR << REG_V_OFS), L_LONG),
        SCPE_OK);

    assert_int_equal(nexus_req[IPL_KDB50] & (1u << TR_KDB50), 0);
    assert_true(nexus_req[level] & (1u << TR_KDB50));
    assert_int_equal(kdb50_get_vector(level), vector);
    assert_true(kdb50_biic.eicr & BIECR_COM);
    assert_false(kdb50_biic.eicr & BIECR_SNT);
    assert_int_equal(nexus_req[level] & (1u << TR_KDB50), 0);

    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_EICR << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(value & BIECR_VEC, vector);
}

static void test_kdb50_eintrcsr_force_requires_intrdes_target(void **state)
{
    const int32_t level = IPL_KDB50 + 1;
    const int32_t vector = 0x0124;

    (void)state;

    reset_kdb50();

    assert_int_equal(
        kdb50_wrreg(BIECR_FRC | (1u << (BIECR_V_LVL + level)) | vector,
                    kdb_pa(BI_EICR << REG_V_OFS), L_LONG),
        SCPE_OK);

    assert_int_equal(nexus_req[level] & (1u << TR_KDB50), 0);
    assert_true(kdb50_biic.eicr & BIECR_ABO);
    assert_true(kdb50_biic.eicr & BIECR_COM);
    assert_false(kdb50_biic.eicr & BIECR_SNT);
}

static void test_kdb50_intrdes_clear_aborts_pending_error_intr(void **state)
{
    const int32_t level = IPL_KDB50 + 1;
    const int32_t vector = 0x0124;

    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(
        kdb50_wrreg(BIECR_FRC | (1u << (BIECR_V_LVL + level)) | vector,
                    kdb_pa(BI_EICR << REG_V_OFS), L_LONG),
        SCPE_OK);
    assert_true(nexus_req[level] & (1u << TR_KDB50));
    assert_true(kdb50_biic.eicr & BIECR_SNT);

    assert_int_equal(kdb50_wrreg(0, kdb_pa(BI_IDEST << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(nexus_req[level] & (1u << TR_KDB50), 0);
    assert_true(kdb50_biic.eicr & BIECR_ABO);
    assert_true(kdb50_biic.eicr & BIECR_COM);
    assert_false(kdb50_biic.eicr & BIECR_SNT);
}

static void test_kdb50_ip_is_high_word_of_gpr0(void **state)
{
    int32_t value;

    (void)state;

    reset_kdb50();

    assert_int_equal(kdb50_wrreg(0xffff, kdb_pa(KDB50_IP_OF), L_WORD), SCPE_OK);
    assert_int_equal(kdb50_rdreg(&value, kdb_pa(KDB50_IP_OF), L_WORD), SCPE_OK);
    assert_int_equal(value, 0);
    assert_int_equal(kdb50_port.ip_reads, 1);
    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_GPR0 << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(value, 0);
}

static void test_kdb50_gpr0_long_read_polls_ip(void **state)
{
    int32_t value;

    (void)state;

    reset_kdb50();

    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_GPR0 << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(value, 0);
    assert_int_equal(kdb50_port.ip_reads, 1);
}

static void test_kdb50_sa_read_and_write_share_gpr1(void **state)
{
    int32_t value;

    (void)state;

    reset_kdb50();

    assert_int_equal(kdb50_rdreg(&value, kdb_pa(KDB50_SA_READ_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(value, 0x0940);

    assert_int_equal(kdb50_wrreg(0x8080, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_port.saw, 0x8080);

    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_GPR1 << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_true((uint32_t)value == 0x80801080u);
}

static void test_kdb50_gpr1_long_read_refreshes_sar(void **state)
{
    int32_t value;

    (void)state;

    reset_kdb50();
    assert_int_equal(kdb50_wrreg(0x8080, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);

    assert_int_equal(kdb50_rdreg(&value, kdb_pa(BI_GPR1 << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_true((uint32_t)value == 0x80801080u);
    assert_int_equal(kdb50_port.sa, 0x1080);
}

static void test_kdb50_gpr1_long_write_updates_saw(void **state)
{
    (void)state;

    reset_kdb50();

    assert_int_equal(
        kdb50_wrreg(0x80800000, kdb_pa(BI_GPR1 << REG_V_OFS), L_LONG), SCPE_OK);
    assert_int_equal(kdb50_port.saw, 0x8080);
    assert_int_equal(vax_mscp_read_sa(&kdb50_mscp), 0x1080);
}

static void test_kdb50_response_completion_defers_nexus_interrupt(void **state)
{
    int32_t value;
    const uint32_t comm = 0x00002000u;
    const uint32_t cmd_packet = 0x00002040u;
    const uint32_t rsp_packet = 0x00002080u;

    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);
    bring_kdb50_port_up(comm);
    assert_int_equal(kdb50_wrreg(BIBCI_UCE | BIBCI_INE,
                                 kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    write_phys_long(comm, 0x80000000u | rsp_packet);
    write_phys_long(comm + 4u, 0x80000000u | cmd_packet);
    write_phys_word(cmd_packet - 4u, 0);
    write_phys_word(cmd_packet + 4u, 0x1234);
    write_phys_word(cmd_packet + 12u, 4);

    assert_int_equal(nexus_req[IPL_KDB50], 0);
    assert_int_equal(kdb50_rdreg(&value, kdb_pa(KDB50_IP_OF), L_WORD), SCPE_OK);
    assert_int_equal(nexus_req[IPL_KDB50], 0);

    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);
    assert_true(nexus_req[IPL_KDB50] & (1u << TR_KDB50));
}

static void test_kdb50_service_requires_posted_interrupt(void **state)
{
    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(BIBCI_UCE | BIBCI_INE,
                                 kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);
    assert_int_equal(nexus_req[IPL_KDB50], 0);
}

static void test_kdb50_service_requires_intrdes_target(void **state)
{
    (void)state;

    reset_kdb50();
    assert_int_equal(kdb50_wrreg(BIBCI_UCE | BIBCI_INE,
                                 kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_wrreg(0x80a4, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);

    assert_int_equal(nexus_req[IPL_KDB50] & (1u << TR_KDB50), 0);
    assert_true(kdb50_biic.uiic & (1u << (28 + IPL_KDB50)));
    assert_true(kdb50_biic.uiic & (1u << (BIICR_V_ITC + IPL_KDB50)));
    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_SNT + IPL_KDB50)));
}

static void test_kdb50_init_interrupt_uses_uiic_vector(void **state)
{
    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(BIBCI_UCE | BIBCI_INE,
                                 kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_wrreg(0x80a4, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);

    assert_int_equal(kdb50_biic.uiic & BIICR_FRC,
                     1u << (BIICR_V_FRC + IPL_KDB50));
    assert_int_equal(kdb50_biic.uiic & BIICR_VEC, 0x0114);
    assert_true(nexus_req[IPL_KDB50] & (1u << TR_KDB50));
    assert_true(kdb50_biic.uiic & (1u << (BIICR_V_SNT + IPL_KDB50)));
    assert_int_equal(kdb50_get_vector(IPL_KDB50), 0x0114);
    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_SNT + IPL_KDB50)));
    assert_true(kdb50_biic.uiic & (1u << (BIICR_V_FRC + IPL_KDB50)));
    assert_true(kdb50_biic.uiic & (1u << (BIICR_V_ITC + IPL_KDB50)));
}

static void
test_kdb50_user_interrupt_does_not_require_source_bcic_enable(void **state)
{
    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(0, kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_wrreg(0x80a4, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);

    assert_true(nexus_req[IPL_KDB50] & (1u << TR_KDB50));
    assert_true(kdb50_biic.uiic & (1u << (BIICR_V_SNT + IPL_KDB50)));
}

static void test_kdb50_zero_uiic_vector_suppresses_init_interrupt(void **state)
{
    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(BIBCI_UCE | BIBCI_INE,
                                 kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_wrreg(0xa480, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);

    assert_false(kdb50_biic.uiic & BIICR_FRC);
    assert_int_equal(kdb50_biic.uiic & BIICR_VEC, 0);
    assert_true(kdb50_biic.uiic & BIICR_EXV);
    assert_false(nexus_req[IPL_KDB50] & (1u << TR_KDB50));
    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_SNT + IPL_KDB50)));
    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_ITC + IPL_KDB50)));
}

static void test_kdb50_zero_step1_vector_uses_uiic_vector(void **state)
{
    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(BIBCI_UCE | BIBCI_INE,
                                 kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_wrreg(0xa480, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);

    assert_int_equal(kdb50_biic.uiic & BIICR_FRC, KDB50_UIIC_FORCE);
    assert_int_equal(kdb50_biic.uiic & BIICR_VEC, 0x0114);
    assert_true(nexus_req[IPL_KDB50] & (1u << TR_KDB50));
    assert_true(kdb50_biic.uiic & KDB50_UIIC_SENT);
    assert_int_equal(kdb50_get_vector(IPL_KDB50), 0x0114);
    assert_true(kdb50_biic.uiic & KDB50_UIIC_COMPLETE);
}

static void test_kdb50_reasserted_user_interrupt_clears_completion(void **state)
{
    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_wrreg(0xa480, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);
    assert_int_equal(kdb50_get_vector(IPL_KDB50), 0x0114);
    assert_true(kdb50_biic.uiic & KDB50_UIIC_COMPLETE);

    assert_int_equal(kdb50_wrreg(0x0000, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_false(kdb50_biic.uiic & KDB50_UIIC_COMPLETE);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);

    assert_true(nexus_req[IPL_KDB50] & (1u << TR_KDB50));
    assert_true(kdb50_biic.uiic & KDB50_UIIC_SENT);
    assert_int_equal(kdb50_get_vector(IPL_KDB50), 0x0114);
}

static void test_kdb50_uiic_write_clears_deasserted_interrupt(void **state)
{
    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(BIBCI_UCE | BIBCI_INE,
                                 kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_wrreg(0x80a4, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);
    assert_int_equal(kdb50_get_vector(IPL_KDB50), 0x0114);

    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_FRC + IPL_KDB50)));
    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_SNT + IPL_KDB50)));
    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_ITC + IPL_KDB50)));
}

static void test_kdb50_uiic_deassert_clears_pending_nexus_request(void **state)
{
    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(BIBCI_UCE | BIBCI_INE,
                                 kdb_pa(BI_BCIC << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(kdb50_wrreg(0x80a4, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);
    assert_true(nexus_req[IPL_KDB50] & (1u << TR_KDB50));

    assert_int_equal(kdb50_wrreg(0, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(nexus_req[IPL_KDB50] & (1u << TR_KDB50), 0);
    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_FRC + IPL_KDB50)));
    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_SNT + IPL_KDB50)));
    assert_false(kdb50_biic.uiic & (1u << (BIICR_V_ITC + IPL_KDB50)));
}

static void test_kdb50_intrdes_clear_aborts_pending_user_intr(void **state)
{
    (void)state;

    reset_kdb50();
    target_kdb50_interrupts_to_primary_processor();
    assert_int_equal(kdb50_wrreg(0x0114, kdb_pa(BI_UIIC << REG_V_OFS), L_LONG),
                     SCPE_OK);
    assert_int_equal(kdb50_wrreg(0x80a4, kdb_pa(KDB50_SA_WRITE_OF), L_WORD),
                     SCPE_OK);
    assert_int_equal(kdb50_svc(&kdb50_unit[0]), SCPE_OK);
    assert_true(nexus_req[IPL_KDB50] & (1u << TR_KDB50));
    assert_true(kdb50_biic.uiic & KDB50_UIIC_SENT);

    assert_int_equal(kdb50_wrreg(0, kdb_pa(BI_IDEST << REG_V_OFS), L_LONG),
                     SCPE_OK);

    assert_int_equal(nexus_req[IPL_KDB50] & (1u << TR_KDB50), 0);
    assert_true(kdb50_biic.uiic & KDB50_UIIC_ABORT);
    assert_true(kdb50_biic.uiic & KDB50_UIIC_COMPLETE);
    assert_false(kdb50_biic.uiic & KDB50_UIIC_SENT);
}

static void test_kdb50_map_pte_read_uses_physical_pte_list(void **state)
{
    uint32_t pte = 0;
    uint32_t expected = PTE_V | (0x00005000u >> VA_N_OFF);
    uint32_t wrong = PTE_V | (0x00007000u >> VA_N_OFF);

    (void)state;

    reset_kdb50();
    SBR = 0x00001000u;
    write_phys_long(0x00000480u, expected);
    write_phys_long(0x00001008u, PTE_V | (0x00003000u >> VA_N_OFF));
    write_phys_long(0x00003080u, wrong);

    assert_int_equal(kdb50_read_map_pte(NULL, 0x00000380u, 64, &pte), 0);
    assert_int_equal(pte, expected);
}

static void test_kdb50_ra81_type_sets_attach_capacity(void **state)
{
    (void)state;

    reset_kdb50();
    kdb50_unit[0].capac = 0;

    assert_int_equal(
        kdb50_set_type(&kdb50_unit[0], KDB50_RA81_BLOCKS, NULL, NULL), SCPE_OK);
    assert_int_equal(kdb50_unit[0].capac, KDB50_RA81_BLOCKS);
}

static void test_kdb50_device_capacity_is_in_sectors(void **state)
{
    (void)state;

    assert_true(kdb50_dev.flags & DEV_SECTORS);
}

static void test_kdb50_disk_callbacks_reject_short_sim_disk_io(void **state)
{
    uint8_t sector[KDB50_MSCP_PAGE_BYTES];
    SIM_DISK_TEST_BACKEND backend = {
        .rdsect = short_read_sector,
        .wrsect = short_write_sector,
    };

    (void)state;

    reset_kdb50();
    memset(sector, 0, sizeof(sector));
    assert_int_equal(sim_disk_set_test_backend(&kdb50_unit[0], &backend),
                     SCPE_OK);

    assert_int_equal(kdb50_read_sectors(NULL, 0, 0, sector, 1), -1);
    assert_int_equal(kdb50_write_sectors(NULL, 0, 0, sector, 1), -1);

    sim_disk_clear_test_backend(&kdb50_unit[0]);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_reset_exposes_kdb50_bi_node_identity),
        cmocka_unit_test(test_bicsr_node_reset_reinitializes_mscp_port),
        cmocka_unit_test(test_reset_exposes_kdb50_bci_access_and_interrupts),
        cmocka_unit_test(
            test_kdb50_unimplemented_biic_registers_do_not_alias_bcic),
        cmocka_unit_test(test_kdb50_ber_read_masks_reserved_bit_and_w1c_errors),
        cmocka_unit_test(test_kdb50_intrdes_masks_to_destination_word),
        cmocka_unit_test(test_kdb50_eintrcsr_deassert_clears_completion),
        cmocka_unit_test(test_kdb50_eintrcsr_force_uses_programmed_level),
        cmocka_unit_test(test_kdb50_eintrcsr_force_requires_intrdes_target),
        cmocka_unit_test(test_kdb50_intrdes_clear_aborts_pending_error_intr),
        cmocka_unit_test(test_kdb50_ip_is_high_word_of_gpr0),
        cmocka_unit_test(test_kdb50_gpr0_long_read_polls_ip),
        cmocka_unit_test(test_kdb50_sa_read_and_write_share_gpr1),
        cmocka_unit_test(test_kdb50_gpr1_long_read_refreshes_sar),
        cmocka_unit_test(test_kdb50_gpr1_long_write_updates_saw),
        cmocka_unit_test(test_kdb50_response_completion_defers_nexus_interrupt),
        cmocka_unit_test(test_kdb50_service_requires_posted_interrupt),
        cmocka_unit_test(test_kdb50_service_requires_intrdes_target),
        cmocka_unit_test(test_kdb50_init_interrupt_uses_uiic_vector),
        cmocka_unit_test(
            test_kdb50_user_interrupt_does_not_require_source_bcic_enable),
        cmocka_unit_test(test_kdb50_zero_uiic_vector_suppresses_init_interrupt),
        cmocka_unit_test(test_kdb50_zero_step1_vector_uses_uiic_vector),
        cmocka_unit_test(
            test_kdb50_reasserted_user_interrupt_clears_completion),
        cmocka_unit_test(test_kdb50_uiic_write_clears_deasserted_interrupt),
        cmocka_unit_test(test_kdb50_uiic_deassert_clears_pending_nexus_request),
        cmocka_unit_test(test_kdb50_intrdes_clear_aborts_pending_user_intr),
        cmocka_unit_test(test_kdb50_map_pte_read_uses_physical_pte_list),
        cmocka_unit_test(test_kdb50_ra81_type_sets_attach_capacity),
        cmocka_unit_test(test_kdb50_device_capacity_is_in_sectors),
        cmocka_unit_test(test_kdb50_disk_callbacks_reject_short_sim_disk_io),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
