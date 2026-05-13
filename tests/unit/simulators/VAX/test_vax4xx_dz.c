#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "vax4xx_dz_internal.h"
#include "vax4xx_defs.h"
#include "vax_defs.h"
#include "vax_lk.h"
#include "vax_vs.h"
#include "sim_tmxr.h"

#define DZ_LINES 4
#define DZ_CSR_CLR 0000020
#define DZ_CSR_MSE 0000040
#define DZ_CSR_SAE 0x1000
#define DZ_CSR_SA 0x2000
#define DZ_CSR_BIT_6 0000100
#define DZ_CSR_RDONE 0000200
#define DZ_CSR_TX_READY 0100000
#define DZ_CSR_BIT_14 0040000
#define DZ_FUNC_TMXR 0
#define DZ_FUNC_KEYBOARD 2
#define DZ_FUNC_MOUSE 3
#define DZ_LPR_RCVE 0x1000
#define DZ_RBUF_VALID 0x8000
#define DZ_RBUF_LINE_SHIFT 8
#define DZ_SILO_CAPACITY 48
#define DZ_DEBUG_REG 0x0001
#define DZ_DEBUG_RCV TMXR_DBG_RCV

int32_t int_req[IPL_HLVL];
uint32_t R[16];
int32_t sys_model;
uint32_t fault_PC;
uint32_t trpirq;
int32_t hlt_pin;
int32_t tmxr_poll = 10000;
jmp_buf save_env;

static uint8_t keyboard_input[DZ_SILO_CAPACITY];
static size_t keyboard_input_len;
static size_t keyboard_input_pos;

/* Queue synthetic LK keyboard input for receive-path unit tests. */
static void queue_keyboard_input(size_t count)
{
    assert_true(count <= DZ_SILO_CAPACITY);

    for (size_t i = 0; i < count; i++)
        keyboard_input[i] = (uint8_t)('A' + (i % 26));

    keyboard_input_len = count;
    keyboard_input_pos = 0;
}

t_stat lk_rd(uint8_t *data)
{
    if (keyboard_input_pos >= keyboard_input_len)
        return SCPE_EOF;

    *data = keyboard_input[keyboard_input_pos];
    keyboard_input_pos++;
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
    return SCPE_EOF;
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

/* Reset the DZ globals and attach stack-backed terminal line descriptors. */
static void reset_dz_state(TMLN lines[DZ_LINES])
{
    memset(lines, 0, sizeof(*lines) * DZ_LINES);
    for (int line = 0; line < DZ_LINES; line++) {
        lines[line].xmte = 1;
        lines[line].mp = &dz_desc;
        lines[line].uptr = &dz_unit[0];
    }

    dz_ldsc = lines;
    dz_desc.ldsc = lines;
    dz_desc.dptr = &dz_dev;
    dz_desc.uptr = &dz_unit[0];
    dz_csr = 0;
    dz_scnt = 0;
    dz_tcr = 0;
    sys_model = 0;
    keyboard_input_len = 0;
    keyboard_input_pos = 0;
    int_req[0] = 0;
    trpirq = 0;
    sim_cancel(&dz_unit[0]);
    sim_cancel(&dz_unit[1]);
}

/* Install synthetic buffered TMXR input on a DZ terminal line. */
static void queue_tmxr_input(TMLN *line, const char *input)
{
    size_t input_len = strlen(input);

    line->rxb = malloc(input_len);
    line->rbr = calloc(input_len, 1);
    assert_non_null(line->rxb);
    assert_non_null(line->rbr);

    memcpy(line->rxb, input, input_len);
    line->rxbsz = (int32_t)input_len;
    line->rxbpi = (int32_t)input_len;
    line->rxbpr = 0;
    line->conn = 1;
    line->rcve = 1;
}

/* Release any synthetic TMXR buffers installed by a test. */
static void free_tmxr_input(TMLN lines[DZ_LINES])
{
    for (int line = 0; line < DZ_LINES; line++) {
        free(lines[line].rxb);
        free(lines[line].rbr);
    }
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

static void test_clearing_scan_enable_clears_receive_silo(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);
    sys_model = 1;
    assert_int_equal(dz_reset(&dz_dev), SCPE_OK);
    queue_keyboard_input(3);

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(4, DZ_LPR_RCVE, L_WORD);
    dz_update_rcvi();

    assert_true(dz_csr & DZ_CSR_RDONE);
    assert_true(dz_scnt > 0);

    dz_wr(0, 0, L_WORD);

    assert_false(dz_csr & DZ_CSR_RDONE);
    assert_int_equal(dz_scnt, 0);
    assert_int_equal(dz_getc(), 0);
}

static void test_clearing_scan_enable_discards_buffered_line_input(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);
    sys_model = 1;
    assert_int_equal(dz_reset(&dz_dev), SCPE_OK);
    queue_tmxr_input(&lines[3], "\033[?1;2c");

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(4, DZ_LPR_RCVE | 3, L_WORD);

    assert_true(tmxr_getc_ln(&lines[3]) != 0);
    lines[3].rxbpr = 0;
    lines[3].rxnexttime = 0;

    dz_wr(0, 0, L_WORD);
    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_update_rcvi();

    assert_int_equal(lines[3].rxbpi, 0);
    assert_int_equal(lines[3].rxbpr, 0);
    assert_int_equal(dz_scnt, 0);
    assert_int_equal(tmxr_getc_ln(&lines[3]), 0);

    free_tmxr_input(lines);
}

static void test_disabled_scan_write_does_not_discard_line_input(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);
    sys_model = 1;
    assert_int_equal(dz_reset(&dz_dev), SCPE_OK);
    queue_tmxr_input(&lines[3], "x");

    dz_wr(0, 0, L_WORD);
    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(4, DZ_LPR_RCVE | 3, L_WORD);
    dz_update_rcvi();

    assert_int_equal(dz_scnt, 1);
    assert_int_equal(dz_getc() & 0xff, 'x');

    free_tmxr_input(lines);
}

static void test_csr_read_polls_pending_line_input(void **state)
{
    TMLN lines[DZ_LINES];
    int32_t csr;
    (void)state;

    reset_dz_state(lines);
    sys_model = 1;
    assert_int_equal(dz_reset(&dz_dev), SCPE_OK);

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(4, DZ_LPR_RCVE | 3, L_WORD);
    assert_false(dz_csr & DZ_CSR_RDONE);

    queue_tmxr_input(&lines[3], "\033");

    csr = dz_rd(0);

    assert_true(csr & DZ_CSR_RDONE);
    assert_int_equal(dz_scnt, 1);

    free_tmxr_input(lines);
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

static void test_transmit_data_write_delays_output_by_unit_wait(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(8, 1, L_WORD);
    assert_true(dz_csr & DZ_CSR_TX_READY);

    dz_wr(12, 'x', L_WORD);

    assert_true(sim_is_active(&dz_unit[1]));
    assert_int_equal(sim_activate_time(&dz_unit[1]), dz_unit[1].wait + 1);
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

static void test_vaxstation_reset_uses_ka48_line_roles(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);
    sys_model = 1;

    assert_int_equal(dz_reset(&dz_dev), SCPE_OK);

    assert_int_equal(dz_func[0], DZ_FUNC_KEYBOARD);
    assert_int_equal(dz_func[1], DZ_FUNC_MOUSE);
    assert_int_equal(dz_func[2], DZ_FUNC_TMXR);
    assert_int_equal(dz_func[3], DZ_FUNC_TMXR);
    assert_int_equal(dz_lnorder[0], 2);
    assert_int_equal(dz_lnorder[1], 3);
    assert_int_equal(dz_lnorder[2], 2);
    assert_int_equal(dz_lnorder[3], 3);
}

static void test_vaxstation_receive_silo_accepts_48_keyboard_chars(
    void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);
    sys_model = 1;
    assert_int_equal(dz_reset(&dz_dev), SCPE_OK);
    queue_keyboard_input(DZ_SILO_CAPACITY);

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(4, DZ_LPR_RCVE, L_WORD);

    for (size_t i = 0; i < DZ_SILO_CAPACITY; i++)
        dz_update_rcvi();

    assert_int_equal(dz_scnt, DZ_SILO_CAPACITY);
    for (size_t i = 0; i < DZ_SILO_CAPACITY; i++) {
        uint16_t c = dz_getc();

        assert_true(c & DZ_RBUF_VALID);
        assert_int_equal(c & 0xff, 'A' + (i % 26));
    }
}

static void test_vaxstation_silo_alarm_threshold_remains_16_chars(
    void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);
    sys_model = 1;
    assert_int_equal(dz_reset(&dz_dev), SCPE_OK);
    queue_keyboard_input(16);

    dz_wr(0, DZ_CSR_MSE | DZ_CSR_SAE, L_WORD);
    dz_wr(4, DZ_LPR_RCVE, L_WORD);

    for (int i = 0; i < 16; i++)
        dz_update_rcvi();

    assert_int_equal(dz_scnt, 16);
    assert_true(dz_csr & DZ_CSR_SA);
}

static void test_clearing_scan_enable_rearms_silo_alarm(void **state)
{
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);
    sys_model = 1;
    assert_int_equal(dz_reset(&dz_dev), SCPE_OK);
    queue_keyboard_input(16);

    dz_wr(0, DZ_CSR_MSE | DZ_CSR_SAE, L_WORD);
    dz_wr(4, DZ_LPR_RCVE, L_WORD);

    for (int i = 0; i < 16; i++)
        dz_update_rcvi();

    assert_true(dz_csr & DZ_CSR_SA);

    dz_wr(0, 0, L_WORD);
    assert_false(dz_csr & DZ_CSR_SA);
    assert_int_equal(dz_scnt, 0);

    queue_keyboard_input(16);
    dz_wr(0, DZ_CSR_MSE | DZ_CSR_SAE, L_WORD);
    dz_wr(4, DZ_LPR_RCVE, L_WORD);

    for (int i = 0; i < 16; i++)
        dz_update_rcvi();

    assert_int_equal(dz_scnt, 16);
    assert_true(dz_csr & DZ_CSR_SA);
}

/*
 * These debug-output tests use open_memstream(), which is not available on
 * Windows.  Keep the behavioral DZ tests enabled there; only the tests that
 * capture debug text are POSIX-only.
 */
#if !defined(_WIN32)
/* Capture the receive debug line emitted when dz_getc pops one RBUF word. */
static char *capture_receive_debug_for(uint16_t rbuf)
{
    FILE *old_deb = sim_deb;
    int32_t old_deb_switches = sim_deb_switches;
    uint32_t old_dctrl = dz_dev.dctrl;
    FILE *stream;
    char *output = NULL;
    size_t output_len = 0;

    stream = open_memstream(&output, &output_len);
    assert_non_null(stream);
    sim_deb = stream;
    sim_deb_switches |= SWMASK('F');
    dz_dev.dctrl |= DZ_DEBUG_RCV;
    fault_PC = 0x2004dcba;
    dz_silo[0] = rbuf;
    dz_scnt = 1;

    (void)dz_getc();

    sim_deb = old_deb;
    sim_deb_switches = old_deb_switches;
    dz_dev.dctrl = old_dctrl;
    assert_int_equal(fclose(stream), 0);

    return output;
}

/* Capture the register debug line emitted by a CSR write from a given PC. */
static char *capture_csr_write_debug_at_pc(uint32_t pc)
{
    FILE *old_deb = sim_deb;
    int32_t old_deb_switches = sim_deb_switches;
    uint32_t old_dctrl = dz_dev.dctrl;
    FILE *stream;
    char *output = NULL;
    size_t output_len = 0;

    stream = open_memstream(&output, &output_len);
    assert_non_null(stream);
    sim_deb = stream;
    sim_deb_switches |= SWMASK('F');
    dz_dev.dctrl |= DZ_DEBUG_REG;
    fault_PC = pc;

    dz_wr(0, DZ_CSR_MSE, L_WORD);

    sim_deb = old_deb;
    sim_deb_switches = old_deb_switches;
    dz_dev.dctrl = old_dctrl;
    assert_int_equal(fclose(stream), 0);

    return output;
}

/* Register tracing needs PC context to follow ROM polling paths. */
static void test_register_debug_reports_fault_pc(void **state)
{
    TMLN lines[DZ_LINES];
    char *output;
    (void)state;

    reset_dz_state(lines);

    output = capture_csr_write_debug_at_pc(0x2004abcd);

    assert_non_null(strstr(output, "fault_PC=2004ABCD"));
    free(output);
}

/* Receive tracing must report the encoded RBUF line, not the FIFO depth. */
static void test_receive_debug_reports_encoded_rbuf_line(void **state)
{
    TMLN lines[DZ_LINES];
    char *output;
    (void)state;

    reset_dz_state(lines);

    output = capture_receive_debug_for(
        DZ_RBUF_VALID | (3u << DZ_RBUF_LINE_SHIFT) | 'x');

    assert_non_null(strstr(output, "fault_PC=2004DCBA"));
    assert_non_null(strstr(output, "DZ Line 3 - Received"));
    assert_non_null(strstr(output, "0x8378"));
    free(output);
}
#endif

/*
 * Characterize the direct DZ/TMXR receive boundary for the ROM DA-reply case:
 * if ROM-like code drains all DA bytes from RBUF, a later boot-loader-like
 * reader should not see residual TMXR input at this unit-test boundary.
 */
static void test_drained_da_reply_leaves_no_tmxr_bytes(void **state)
{
    static const char da_reply[] = "\033[?1;2c";
    TMLN lines[DZ_LINES];
    (void)state;

    reset_dz_state(lines);
    sys_model = 1;
    assert_int_equal(dz_reset(&dz_dev), SCPE_OK);
    queue_tmxr_input(&lines[3], da_reply);

    dz_wr(0, DZ_CSR_MSE, L_WORD);
    dz_wr(4, DZ_LPR_RCVE | 3, L_WORD);

    for (size_t i = 0; i < strlen(da_reply);) {
        if (dz_scnt == 0) {
            lines[3].rxnexttime = 0;
            dz_update_rcvi();
        }

        while (dz_scnt != 0) {
            uint16_t c = dz_getc();

            assert_true(c & DZ_RBUF_VALID);
            assert_int_equal(c & 0xff, da_reply[i]);
            i++;
        }
    }

    lines[3].rxnexttime = 0;
    dz_update_rcvi();
    assert_int_equal(dz_scnt, 0);
    assert_int_equal(tmxr_getc_ln(&lines[3]), 0);

    free_tmxr_input(lines);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_csr_write_ignores_unused_interrupt_enable_bits),
        cmocka_unit_test(test_transmit_ready_sets_interrupt_request),
        cmocka_unit_test(test_transmit_enable_without_scan_does_not_set_ready),
        cmocka_unit_test(test_clearing_scan_enable_clears_transmit_ready),
        cmocka_unit_test(test_clearing_scan_enable_clears_receive_silo),
        cmocka_unit_test(
            test_clearing_scan_enable_discards_buffered_line_input),
        cmocka_unit_test(test_disabled_scan_write_does_not_discard_line_input),
        cmocka_unit_test(test_csr_read_polls_pending_line_input),
        cmocka_unit_test(test_transmit_data_write_clears_ready_not_request),
        cmocka_unit_test(
            test_transmit_data_write_delays_output_by_unit_wait),
        cmocka_unit_test(test_csr_clear_does_not_clear_interrupt_request),
        cmocka_unit_test(test_init_reset_clears_interrupt_request),
        cmocka_unit_test(test_vaxstation_reset_uses_ka48_line_roles),
        cmocka_unit_test(
            test_vaxstation_receive_silo_accepts_48_keyboard_chars),
        cmocka_unit_test(
            test_vaxstation_silo_alarm_threshold_remains_16_chars),
        cmocka_unit_test(test_clearing_scan_enable_rearms_silo_alarm),
        /*
         * These debug-output tests use open_memstream(), which is not
         * available on Windows.
         */
#if !defined(_WIN32)
        cmocka_unit_test(test_register_debug_reports_fault_pc),
        cmocka_unit_test(test_receive_debug_reports_encoded_rbuf_line),
#endif
        cmocka_unit_test(test_drained_da_reply_leaves_no_tmxr_bytes),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
