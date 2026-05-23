#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "vax_defs.h"

uint32_t *M;
uint32_t R[16];
uint32_t PSL;
uint32_t SISR;
uint32_t fault_PC;
uint32_t p1;
uint32_t p2;
uint32_t trpirq;
uint32_t nexus_req[NEXUS_HLVL];
uint32_t mchk_ref;
int32_t hlt_pin;
int32_t crd_err;
jmp_buf save_env;
DEVICE cpu_dev;
UNIT cpu_unit;

static int32_t readio_test_values[2];
static int32_t readio_test_offsets[2];
static int32_t readio_test_count;

/*
 * Include the implementation directly so these tests can exercise the DWBUA
 * interrupt state machine without exporting test-only production hooks.
 */

int32_t eval_int(void)
{
    return 0;
}

int32_t ReadReg(uint32_t pa, int32_t lnt)
{
    (void)pa;
    (void)lnt;

    return 0;
}

void WriteReg(uint32_t pa, int32_t val, int32_t lnt)
{
    (void)pa;
    (void)val;
    (void)lnt;
}

void init_ubus_tab(void) {}

t_stat build_ubus_tab(DEVICE *dptr, DIB *dibp)
{
    (void)dptr;
    (void)dibp;

    return SCPE_OK;
}

t_stat show_iospace(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

t_stat show_nexus(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

t_stat show_bus_map(FILE *st, const char *cptr, uint32_t *busmap,
                    uint32_t nmapregs, const char *busname, uint32_t mapvalid)
{
    (void)st;
    (void)cptr;
    (void)busmap;
    (void)nmapregs;
    (void)busname;
    (void)mapvalid;

    return SCPE_OK;
}

t_stat set_autocon(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;

    return SCPE_OK;
}

t_stat show_autocon(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

#include "vax820_uba.c"

static t_stat test_readio_dispatch(int32_t *data, int32_t ad, int32_t mode)
{
    assert_true(readio_test_count < 2);

    readio_test_offsets[readio_test_count] = ad - IOPAGEBASE;
    *data = readio_test_values[readio_test_count];
    readio_test_count++;
    assert_int_equal(mode, READ);

    return SCPE_OK;
}

static void reset_uba_interrupt_state(void)
{
    int32_t i;
    int32_t bit;

    uba_int = 0;
    uba_csr = 0;
    uba_biic.csr = 0;
    uba_biic.eicr = 0;
    uba_vo = 0;
    for (i = 0; i < NEXUS_HLVL; i++) {
        nexus_req[i] &= ~(1 << TR_UBA);
        int_req[i] = 0;
        for (bit = 0; bit < 32; bit++) {
            int_ack[i][bit] = NULL;
            int_vec[i][bit] = 0;
        }
    }
}

static int teardown_readio_dispatch(void **state)
{
    (void)state;

    iodispR[0] = NULL;
    iodispR[1] = NULL;
    return 0;
}

/*
 * VMS V4.x probes the DWBUA and can leave a UBA adapter error pending while
 * the UDA50/RQ controller is reinitializing. A stale adapter condition must
 * not hide the normal Unibus device interrupt that advances RQ init.
 */
static void test_adapter_interrupt_does_not_mask_device_interrupt(void **state)
{
    (void)state;

    reset_uba_interrupt_state();

    uba_int = 1;
    int_req[IPL_RQ] |= INT_RQ;

    uba_eval_int();

    assert_true(nexus_req[IPL_RQ] & (1 << TR_UBA));
}

static void
test_error_interrupt_enable_gates_new_dwbua_request(void **state)
{
    (void)state;

    reset_uba_interrupt_state();

    uba_biic.csr = BICSR_HIE;
    uba_biic.eicr = (1u << (BIECR_V_LVL + IPL_UBA));

    uba_ub_nxm(0);
    uba_eval_int();

    assert_true(uba_csr & UBACSR_TO);
    assert_false(uba_biic.eicr & BIECR_FRC);
    assert_false(uba_int);
    assert_false(nexus_req[IPL_UBA] & (1 << TR_UBA));
}

static void test_clearing_buacsr_error_leaves_force_request(void **state)
{
    (void)state;

    reset_uba_interrupt_state();

    uba_csr = UBACSR_EIE | UBACSR_TO;
    uba_biic.csr = BICSR_HIE;
    uba_biic.eicr = BIECR_FRC | BIECR_COM |
                    (1u << (BIECR_V_LVL + IPL_UBA));
    uba_int = 1;

    assert_int_equal(uba_wrreg(UBACSR_EIE | UBACSR_TO,
                               UBACSR_OF << REG_V_OFS, L_LONG),
                     SCPE_OK);
    uba_eval_int();

    assert_false(uba_csr & UBACSR_TO);
    assert_true(uba_csr & UBACSR_EIE);
    assert_true(uba_biic.eicr & BIECR_FRC);
    assert_true(uba_biic.eicr & BIECR_COM);
    assert_false(nexus_req[IPL_UBA] & (1 << TR_UBA));

    assert_int_equal(uba_wrreg(0, BI_EICR << REG_V_OFS, L_LONG), SCPE_OK);
    uba_eval_int();

    assert_false(uba_biic.eicr & BIECR_FRC);
    assert_false(uba_biic.eicr & BIECR_COM);
    assert_false(nexus_req[IPL_UBA] & (1 << TR_UBA));
}

static void test_biic_error_enable_gates_adapter_interrupt(void **state)
{
    (void)state;

    reset_uba_interrupt_state();

    uba_biic.eicr = BIECR_FRC | (1u << (BIECR_V_LVL + IPL_UBA));

    uba_eval_int();
    assert_false(nexus_req[IPL_UBA] & (1 << TR_UBA));

    uba_biic.csr = BICSR_HIE;

    uba_eval_int();
    assert_true(nexus_req[IPL_UBA] & (1 << TR_UBA));
}

static void test_bi_failure_sets_buacsr_bif(void **state)
{
    const int32_t bad_bi_addr = 0x12345678;

    (void)state;

    reset_uba_interrupt_state();

    uba_csr = UBACSR_EIE;
    uba_biic.csr = BICSR_HIE;
    uba_biic.eicr = (1u << (BIECR_V_LVL + IPL_UBA));

    uba_bi_nxm(bad_bi_addr);
    uba_eval_int();

    assert_true(uba_csr & UBACSR_BIF);
    assert_int_equal(uba_bifa, bad_bi_addr);
    assert_true(uba_biic.eicr & BIECR_FRC);
    assert_false(uba_biic.ber & BIBER_BTO);
    assert_true(nexus_req[IPL_UBA] & (1 << TR_UBA));
}

static void test_error_interrupt_force_requests_adapter_interrupt(void **state)
{
    (void)state;

    reset_uba_interrupt_state();

    uba_biic.csr = BICSR_HIE;
    uba_biic.eicr = BIECR_FRC | (1u << (BIECR_V_LVL + IPL_UBA));

    uba_eval_int();

    assert_true(nexus_req[IPL_UBA] & (1 << TR_UBA));
    assert_false(uba_int);

    assert_int_equal(uba_wrreg(0, BI_EICR << REG_V_OFS, L_LONG), SCPE_OK);
    uba_eval_int();

    assert_false(nexus_req[IPL_UBA] & (1 << TR_UBA));
}

static void
test_error_interrupt_complete_suppresses_adapter_interrupt(void **state)
{
    (void)state;

    reset_uba_interrupt_state();

    uba_biic.csr = BICSR_HIE;
    uba_biic.eicr = BIECR_FRC | BIECR_COM | (1u << (BIECR_V_LVL + IPL_UBA));
    uba_int = 1;

    uba_eval_int();

    assert_false(nexus_req[IPL_UBA] & (1 << TR_UBA));

    assert_int_equal(uba_wrreg(uba_biic.eicr, BI_EICR << REG_V_OFS, L_LONG),
                     SCPE_OK);
    uba_eval_int();

    assert_true(nexus_req[IPL_UBA] & (1 << TR_UBA));
}

static void test_forced_adapter_vector_ack_sets_complete(void **state)
{
    const int32_t vector = 01234;

    (void)state;

    reset_uba_interrupt_state();

    uba_biic.csr = BICSR_HIE;
    uba_biic.eicr = BIECR_FRC | (1u << (BIECR_V_LVL + IPL_UBA)) | vector;

    uba_eval_int();
    assert_true(nexus_req[IPL_UBA] & (1 << TR_UBA));

    assert_int_equal(uba_get_ubvector(IPL_UBA), vector);
    uba_eval_int();

    assert_true(uba_biic.eicr & BIECR_FRC);
    assert_true(uba_biic.eicr & BIECR_COM);
    assert_false(uba_biic.eicr & BIECR_SNT);
    assert_false(nexus_req[IPL_UBA] & (1 << TR_UBA));

    assert_int_equal(uba_wrreg(uba_biic.eicr, BI_EICR << REG_V_OFS, L_LONG),
                     SCPE_OK);
    uba_eval_int();

    assert_true(nexus_req[IPL_UBA] & (1 << TR_UBA));

    assert_int_equal(uba_wrreg(0, BI_EICR << REG_V_OFS, L_LONG), SCPE_OK);
    uba_eval_int();

    assert_false(uba_biic.eicr & BIECR_FRC);
    assert_false(uba_biic.eicr & BIECR_COM);
    assert_false(nexus_req[IPL_UBA] & (1 << TR_UBA));
}

static void test_adapter_interrupt_uses_programmed_bi_level(void **state)
{
    const int32_t level = IPL_UBA + 1;
    const int32_t vector = 01234;

    (void)state;

    reset_uba_interrupt_state();

    uba_biic.csr = BICSR_HIE;
    uba_biic.eicr = BIECR_FRC | (1u << (BIECR_V_LVL + level)) | vector;

    uba_eval_int();

    assert_false(nexus_req[IPL_UBA] & (1 << TR_UBA));
    assert_true(nexus_req[level] & (1 << TR_UBA));
    assert_int_equal(uba_get_ubvector(level), vector);
}

static void
test_adapter_vector_precedes_device_vector_at_same_level(void **state)
{
    const int32_t adapter_vector = 01234;
    const int32_t device_vector = 0560;

    (void)state;

    reset_uba_interrupt_state();

    uba_biic.csr = BICSR_HIE;
    uba_biic.eicr = BIECR_FRC | (1u << (BIECR_V_LVL + IPL_RQ)) |
                    adapter_vector;
    int_req[IPL_RQ] = INT_RQ;
    int_vec[IPL_RQ][INT_V_RQ] = device_vector;

    uba_eval_int();

    assert_true(nexus_req[IPL_RQ] & (1 << TR_UBA));
    assert_int_equal(uba_get_ubvector(IPL_RQ), adapter_vector);
    assert_true(int_req[IPL_RQ] & INT_RQ);

    assert_int_equal(uba_wrreg(0, BI_EICR << REG_V_OFS, L_LONG), SCPE_OK);
    uba_eval_int();

    assert_int_equal(uba_get_ubvector(IPL_RQ), device_vector);
    assert_false(int_req[IPL_RQ] & INT_RQ);
}

static void test_longword_iopage_read_uses_two_unibus_reads(void **state)
{
    (void)state;

    readio_test_values[0] = 0x1122;
    readio_test_values[1] = 0x3344;
    readio_test_offsets[0] = readio_test_offsets[1] = -1;
    readio_test_count = 0;
    iodispR[0] = test_readio_dispatch;
    iodispR[1] = test_readio_dispatch;

    assert_int_equal(ReadIO(IOPAGEBASE, L_LONG), 0x33441122);
    assert_int_equal(readio_test_count, 2);
    assert_int_equal(readio_test_offsets[0], 0);
    assert_int_equal(readio_test_offsets[1], 2);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_adapter_interrupt_does_not_mask_device_interrupt),
        cmocka_unit_test(
            test_error_interrupt_enable_gates_new_dwbua_request),
        cmocka_unit_test(test_clearing_buacsr_error_leaves_force_request),
        cmocka_unit_test(test_biic_error_enable_gates_adapter_interrupt),
        cmocka_unit_test(test_bi_failure_sets_buacsr_bif),
        cmocka_unit_test(test_error_interrupt_force_requests_adapter_interrupt),
        cmocka_unit_test(
            test_error_interrupt_complete_suppresses_adapter_interrupt),
        cmocka_unit_test(test_forced_adapter_vector_ack_sets_complete),
        cmocka_unit_test(test_adapter_interrupt_uses_programmed_bi_level),
        cmocka_unit_test(
            test_adapter_vector_precedes_device_vector_at_same_level),
        cmocka_unit_test_teardown(
            test_longword_iopage_read_uses_two_unibus_reads,
            teardown_readio_dispatch),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
