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

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(
            vax_qbus_replace_word(0x12345678u, cases[i].pa, cases[i].val),
            cases[i].expected);
}

/* Verify mapped Qbus word writes can set the high bit without signed shift. */
static void test_replace_word_can_set_sign_bit(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
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

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(
            vax_qbus_replace_byte(0x12345678u, cases[i].pa, cases[i].val),
            cases[i].expected);
}

/* Verify mapped Qbus byte writes store only the low byte of the value. */
static void test_replace_byte_masks_replacement_value(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(vax_qbus_replace_byte(0x12345678u, 3, 0x1a5u),
                     0xa5345678u);
}

/* Verify read words are shifted into the address-selected word lane. */
static void test_position_read_word_covers_both_word_lanes(void **state)
{
    static const struct {
        uint32 pa;
        uint32 val;
        uint32 expected;
    } cases[] = {
        {0, 0x8000, 0x00008000},
        {1, 0x8000, 0x00008000},
        {2, 0x8000, 0x80000000},
        {3, 0x8000, 0x80000000},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(
            vax_qbus_position_read_word(cases[i].pa, cases[i].val),
            cases[i].expected);
}

/* Verify positioned read words store only the low word of the value. */
static void test_position_read_word_masks_qbus_word(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(vax_qbus_position_read_word(2, 0x18000u), 0x80000000u);
}

/* Verify read longwords are assembled from low and high Qbus words. */
static void test_combine_read_words_assembles_longword(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(vax_qbus_combine_read_words(0x1234u, 0xff00u),
                     0xff001234u);
}

/* Verify assembled read longwords store only the low word of each value. */
static void test_combine_read_words_masks_qbus_words(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(vax_qbus_combine_read_words(0x11234u, 0x2ff00u),
                     0xff001234u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_replace_word_covers_both_word_lanes),
        cmocka_unit_test(test_replace_word_can_set_sign_bit),
        cmocka_unit_test(test_replace_byte_covers_every_byte_lane),
        cmocka_unit_test(test_replace_byte_masks_replacement_value),
        cmocka_unit_test(test_position_read_word_covers_both_word_lanes),
        cmocka_unit_test(test_position_read_word_masks_qbus_word),
        cmocka_unit_test(test_combine_read_words_assembles_longword),
        cmocka_unit_test(test_combine_read_words_masks_qbus_words),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
