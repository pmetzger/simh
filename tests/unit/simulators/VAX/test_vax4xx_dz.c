#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"
#include "vax_lk.h"
#include "vax_vs.h"
#include "sim_tmxr.h"

#define DZ_LINES 4
#define DZ_CSR_CLR 0000020
#define DZ_CSR_MSE 0000040
#define DZ_CSR_BIT_6 0000100
#define DZ_CSR_TX_READY 0100000
#define DZ_CSR_BIT_14 0040000

extern DEVICE dz_dev;
extern TMXR dz_desc;
extern TMLN *dz_ldsc;
extern UNIT dz_unit[];
extern uint16_t dz_csr;
extern uint16_t dz_tcr;

t_stat dz_clear(bool flag);
void dz_wr(int32_t pa, int32_t data, int32_t access);

int32_t int_req[IPL_HLVL];
int32_t sys_model;
uint32_t trpirq;
int32_t hlt_pin;
int32_t tmxr_poll = 10000;
jmp_buf save_env;

t_stat lk_rd(uint8_t *data)
{
    (void)data;
    return SCPE_OK;
}

t_stat lk_wr(uint8_t data)
{
    (void)data;
    return SCPE_OK;
}

t_stat vs_rd(uint8_t *data)
{
    (void)data;
    return SCPE_OK;
}

t_stat vs_wr(uint8_t data)
{
    (void)data;
    return SCPE_OK;
}

int32_t eval_int(void)
{
    return 0;
}

static void reset_dz_state(TMLN lines[DZ_LINES])
{
    memset(lines, 0, sizeof(*lines) * DZ_LINES);
    for (int line = 0; line < DZ_LINES; line++)
        lines[line].xmte = 1;

    dz_ldsc = lines;
    dz_desc.ldsc = lines;
    dz_csr = 0;
    dz_tcr = 0;
    int_req[0] = 0;
    trpirq = 0;
    sim_cancel(&dz_unit[0]);
    sim_cancel(&dz_unit[1]);
}

static void test_csr_write_ignores_unused_interrupt_enable_bits(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);

    dz_wr(0, DZ_CSR_MSE | DZ_CSR_BIT_6 | DZ_CSR_BIT_14, L_WORD);

    assert_true(dz_csr & DZ_CSR_MSE);
    assert_false(dz_csr & DZ_CSR_BIT_6);
    assert_false(dz_csr & DZ_CSR_BIT_14);
}

static void test_transmit_ready_sets_interrupt_request(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(8, 1, L_WORD);

    assert_true(dz_csr & DZ_CSR_TX_READY);
    assert_true(int_req[0] & INT_DZTX);
}

static void test_transmit_enable_without_scan_does_not_set_ready(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);

    dz_wr(8, 1, L_WORD);

    assert_false(dz_csr & DZ_CSR_TX_READY);
    assert_false(int_req[0] & INT_DZTX);
}

static void test_clearing_scan_enable_clears_transmit_ready(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(8, 1, L_WORD);
    assert_true(dz_csr & DZ_CSR_TX_READY);
    assert_true(int_req[0] & INT_DZTX);
    int_req[0] = 0;

    dz_wr(0, 0, L_WORD);

    assert_false(dz_csr & DZ_CSR_TX_READY);
    assert_false(int_req[0] & INT_DZTX);
}

static void test_transmit_data_write_clears_ready_not_request(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(8, 1, L_WORD);
    assert_true(dz_csr & DZ_CSR_TX_READY);
    assert_true(int_req[0] & INT_DZTX);

    dz_wr(12, 'x', L_WORD);

    assert_false(dz_csr & DZ_CSR_TX_READY);
    assert_true(int_req[0] & INT_DZTX);
}

static void test_csr_clear_does_not_clear_interrupt_request(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(8, 1, L_WORD);
    int_req[0] |= INT_DZRX;
    assert_true(int_req[0] & INT_DZRX);
    assert_true(int_req[0] & INT_DZTX);

    dz_wr(0, DZ_CSR_CLR, L_WORD);

    assert_int_equal(dz_csr, 0);
    assert_true(int_req[0] & INT_DZRX);
    assert_true(int_req[0] & INT_DZTX);
}

static void test_init_reset_clears_interrupt_request(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);
    int_req[0] = INT_DZRX | INT_DZTX;

    assert_int_equal(dz_clear(true), SCPE_OK);

    assert_false(int_req[0] & INT_DZRX);
    assert_false(int_req[0] & INT_DZTX);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_csr_write_ignores_unused_interrupt_enable_bits),
        cmocka_unit_test(test_transmit_ready_sets_interrupt_request),
        cmocka_unit_test(test_transmit_enable_without_scan_does_not_set_ready),
        cmocka_unit_test(test_clearing_scan_enable_clears_transmit_ready),
        cmocka_unit_test(test_transmit_data_write_clears_ready_not_request),
        cmocka_unit_test(test_csr_clear_does_not_clear_interrupt_request),
        cmocka_unit_test(test_init_reset_clears_interrupt_request),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
