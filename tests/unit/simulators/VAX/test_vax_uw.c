#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"
#include "vax_uw_internal.h"

enum {
    UW_CSR = 0,
    UW_IRR = 1,
    UW_CSR_COUNT = 16,
    UW_CSR_TRN = 0x8000,
};

int32_t int_req[IPL_HLVL];
int32_t sys_model;
uint32_t fault_PC;
uint32_t trpirq;
int32_t hlt_pin;
jmp_buf save_env;

static void reset_uw_state(void)
{
    memset(uw_csr, 0, sizeof(uint16_t) * UW_CSR_COUNT);
    memset(int_req, 0, sizeof(int_req));
}

static void test_csr0_clear_link_transition_clears_interrupt(void **state)
{
    (void)state;

    reset_uw_state();
    uw_csr[UW_CSR] = UW_CSR_TRN;
    int_req[IPL_UW] = INT_UW;

    assert_int_equal(uw_wr(0, 0, 0), SCPE_OK);

    assert_int_equal(int_req[IPL_UW] & INT_UW, 0);
    assert_int_equal(uw_csr[UW_CSR], 0);
}

static void test_csr1_zero_write_clears_interrupt(void **state)
{
    (void)state;

    reset_uw_state();
    int_req[IPL_UW] = INT_UW;

    assert_int_equal(uw_wr(0, 2, 0), SCPE_OK);

    assert_int_equal(int_req[IPL_UW] & INT_UW, 0);
    assert_int_equal(uw_csr[UW_IRR], 0);
}

static void
test_csr0_zero_write_without_transition_preserves_interrupt(void **state)
{
    (void)state;

    reset_uw_state();
    int_req[IPL_UW] = INT_UW;

    assert_int_equal(uw_wr(0, 0, 0), SCPE_OK);

    assert_int_equal(int_req[IPL_UW] & INT_UW, INT_UW);
    assert_int_equal(uw_csr[UW_CSR], 0);
}

int32_t Map_ReadB(uint32_t ba, int32_t bc, uint8_t *buf)
{
    (void)ba;
    (void)bc;
    (void)buf;
    return 0;
}

int32_t Map_ReadW(uint32_t ba, int32_t bc, uint16_t *buf)
{
    (void)ba;
    (void)bc;
    (void)buf;
    return 0;
}

int32_t Map_WriteB(uint32_t ba, int32_t bc, const uint8_t *buf)
{
    (void)ba;
    (void)bc;
    (void)buf;
    return 0;
}

int32_t Map_WriteW(uint32_t ba, int32_t bc, const uint16_t *buf)
{
    (void)ba;
    (void)bc;
    (void)buf;
    return 0;
}

t_stat auto_config(const char *name, int32_t n)
{
    (void)name;
    (void)n;
    return SCPE_OK;
}

t_stat set_addr(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;
    return SCPE_OK;
}

t_stat show_addr(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

t_stat show_vec(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;
    return SCPE_OK;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_csr0_clear_link_transition_clears_interrupt),
        cmocka_unit_test(test_csr1_zero_write_clears_interrupt),
        cmocka_unit_test(
            test_csr0_zero_write_without_transition_preserves_interrupt),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
