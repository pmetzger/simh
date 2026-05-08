#include <stddef.h>

#include "test_cmocka.h"

#include "vax_qbus_internal.h"

/* Verify mapped Qbus word writes preserve the non-selected word lane. */
static void test_replace_word_covers_both_word_lanes(void **state)
{
    static const struct {
        uint32 pa;
        uint32 val;
        uint32 expected;
    } cases[] = {
        {0, 0xabcd, 0x1234abcd},
        {2, 0xabcd, 0xabcd5678},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(
            vax_qbus_replace_word(0x12345678u, cases[i].pa, cases[i].val),
            cases[i].expected);
}

/* Verify mapped Qbus word writes can set the high bit without signed shift. */
static void test_replace_word_can_set_sign_bit(void **state)
{
    (void)state;

    assert_int_equal(vax_qbus_replace_word(0x12345678u, 2, 0xffffu),
                     0xffff5678u);
}

/* Verify mapped Qbus byte writes preserve all non-selected byte lanes. */
static void test_replace_byte_covers_every_byte_lane(void **state)
{
    static const struct {
        uint32 pa;
        uint32 val;
        uint32 expected;
    } cases[] = {
        {0, 0xaa, 0x123456aa},
        {1, 0xaa, 0x1234aa78},
        {2, 0xaa, 0x12aa5678},
        {3, 0xaa, 0xaa345678},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(
            vax_qbus_replace_byte(0x12345678u, cases[i].pa, cases[i].val),
            cases[i].expected);
}

/* Verify mapped Qbus byte writes store only the low byte of the value. */
static void test_replace_byte_masks_replacement_value(void **state)
{
    (void)state;

    assert_int_equal(vax_qbus_replace_byte(0x12345678u, 3, 0x1a5u),
                     0xa5345678u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_replace_word_covers_both_word_lanes),
        cmocka_unit_test(test_replace_word_can_set_sign_bit),
        cmocka_unit_test(test_replace_byte_covers_every_byte_lane),
        cmocka_unit_test(test_replace_byte_masks_replacement_value),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
