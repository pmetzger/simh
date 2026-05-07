#include "test_cmocka.h"

#include "sigma_cis.c"

static uint32 test_registers[16];

uint32 *R = test_registers;
uint32 CC;
uint32 PSW1;
uint32 bvamqrx;
uint32 cpu_model;

uint32 ReadB(uint32 bva, uint32 *dat, uint32 acc)
{
    (void)bva;
    (void)dat;
    (void)acc;
    return SCPE_IERR;
}

uint32 WriteB(uint32 bva, uint32 dat, uint32 acc)
{
    (void)bva;
    (void)dat;
    (void)acc;
    return SCPE_IERR;
}

static void test_gen_lshift_preserves_multiword_shift_count(void **state)
{
    (void)state;

    dstr_t decimal = {
        0,
        {0x11111111, 0x22222222, 0x33333333, 0x44444444},
    };

    assert_true(GenLshift(&decimal, 16));

    assert_int_equal(decimal.val[0], 0);
    assert_int_equal(decimal.val[1], 0);
    assert_int_equal(decimal.val[2], 0x11111111);
    assert_int_equal(decimal.val[3], 0x22222222);
}

static void test_gen_lshift_preserves_multinibble_remainder(void **state)
{
    (void)state;

    dstr_t decimal = {
        0,
        {0x11111111, 0x22222222, 0x33333333, 0x44444444},
    };

    assert_true(GenLshift(&decimal, 11));

    assert_int_equal(decimal.val[0], 0);
    assert_int_equal(decimal.val[1], 0x11111000);
    assert_int_equal(decimal.val[2], 0x22222111);
    assert_int_equal(decimal.val[3], 0x33333222);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_gen_lshift_preserves_multiword_shift_count),
        cmocka_unit_test(test_gen_lshift_preserves_multinibble_remainder),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
