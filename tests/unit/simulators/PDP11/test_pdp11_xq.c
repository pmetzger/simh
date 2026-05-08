#include "test_cmocka.h"

#include "pdp11_xq_internal.h"

/* Verify low-half IBA writes preserve the existing high-half bits. */
static void test_replace_iba_low_preserves_high_word(void **state)
{
    (void)state;

    assert_int_equal(xq_replace_iba_low(0x12345678u, 0xabcd), 0x1234abcdu);
}

/* Verify high-half IBA writes preserve the existing low-half bits. */
static void test_replace_iba_high_preserves_low_word(void **state)
{
    (void)state;

    assert_int_equal(xq_replace_iba_high(0x12345678u, 0xabcd), 0xabcd5678u);
}

/* Verify high-half IBA writes can set the sign bit without signed shift. */
static void test_replace_iba_high_can_set_sign_bit(void **state)
{
    (void)state;

    assert_int_equal(xq_replace_iba_high(0x12345678u, 0xff00), 0xff005678u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_replace_iba_low_preserves_high_word),
        cmocka_unit_test(test_replace_iba_high_preserves_low_word),
        cmocka_unit_test(test_replace_iba_high_can_set_sign_bit),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
