#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "pdp11_defs.h"

int32 int_req[IPL_HLVL];
uint32 cpu_opt;
uint32 cpu_type;
uint16 *M;
jmp_buf save_env;

t_stat auto_config(const char *name, int32 nctrl)
{
    (void)name;
    (void)nctrl;
    return SCPE_OK;
}

t_stat set_addr(UNIT *uptr, int32 val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat show_addr(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

t_stat set_vec(UNIT *uptr, int32 val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat show_vec(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

void cpu_set_boot(int32 pc)
{
    (void)pc;
}

#ifndef PDP11_TD_SOURCE
#define PDP11_TD_SOURCE "pdp11_td.c"
#endif

#include PDP11_TD_SOURCE

static t_bool td_rx_cpu_interrupt_is_set(void)
{
    return ((int_req[IPL_TDRX] & (int32)INT_TDRX) != 0);
}

static t_bool td_tx_cpu_interrupt_is_set(void)
{
    return ((int_req[IPL_TDTX] & (int32)INT_TDTX) != 0);
}

static void reset_td_interrupts(void)
{
    memset(int_req, 0, sizeof(int_req));
    tdi_ireq = 0;
    tdo_ireq = 0;
}

static void test_tdi_set_int_tracks_single_controller_state(void **state)
{
    (void)state;

    reset_td_interrupts();

    tdi_set_int(0, FALSE);
    assert_int_equal(tdi_ireq, 0);
    assert_false(td_rx_cpu_interrupt_is_set());

    tdi_set_int(0, TRUE);
    assert_int_equal(tdi_ireq, 1u);
    assert_true(td_rx_cpu_interrupt_is_set());

    tdi_set_int(0, TRUE);
    assert_int_equal(tdi_ireq, 1u);
    assert_true(td_rx_cpu_interrupt_is_set());

    tdi_set_int(0, FALSE);
    assert_int_equal(tdi_ireq, 0);
    assert_false(td_rx_cpu_interrupt_is_set());
}

static void test_tdi_set_int_preserves_shared_interrupt_until_clear(void **state)
{
    (void)state;

    reset_td_interrupts();

    tdi_set_int(0, TRUE);
    tdi_set_int(1, TRUE);
    assert_int_equal(tdi_ireq, 3u);
    assert_true(td_rx_cpu_interrupt_is_set());

    tdi_set_int(0, FALSE);
    assert_int_equal(tdi_ireq, 2u);
    assert_true(td_rx_cpu_interrupt_is_set());

    tdi_set_int(1, FALSE);
    assert_int_equal(tdi_ireq, 0);
    assert_false(td_rx_cpu_interrupt_is_set());
}

static void test_tdo_set_int_tracks_single_controller_state(void **state)
{
    (void)state;

    reset_td_interrupts();

    tdo_set_int(0, FALSE);
    assert_int_equal(tdo_ireq, 0);
    assert_false(td_tx_cpu_interrupt_is_set());

    tdo_set_int(0, TRUE);
    assert_int_equal(tdo_ireq, 1u);
    assert_true(td_tx_cpu_interrupt_is_set());

    tdo_set_int(0, TRUE);
    assert_int_equal(tdo_ireq, 1u);
    assert_true(td_tx_cpu_interrupt_is_set());

    tdo_set_int(0, FALSE);
    assert_int_equal(tdo_ireq, 0);
    assert_false(td_tx_cpu_interrupt_is_set());
}

static void test_tdo_set_int_preserves_shared_interrupt_until_clear(void **state)
{
    (void)state;

    reset_td_interrupts();

    tdo_set_int(0, TRUE);
    tdo_set_int(1, TRUE);
    assert_int_equal(tdo_ireq, 3u);
    assert_true(td_tx_cpu_interrupt_is_set());

    tdo_set_int(0, FALSE);
    assert_int_equal(tdo_ireq, 2u);
    assert_true(td_tx_cpu_interrupt_is_set());

    tdo_set_int(1, FALSE);
    assert_int_equal(tdo_ireq, 0);
    assert_false(td_tx_cpu_interrupt_is_set());
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_tdi_set_int_tracks_single_controller_state),
        cmocka_unit_test(
            test_tdi_set_int_preserves_shared_interrupt_until_clear),
        cmocka_unit_test(test_tdo_set_int_tracks_single_controller_state),
        cmocka_unit_test(
            test_tdo_set_int_preserves_shared_interrupt_until_clear),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
