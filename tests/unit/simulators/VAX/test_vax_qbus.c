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

/* Verify write byte extraction treats the value as an unsigned bit pattern. */
static void test_extract_write_byte_covers_every_byte(void **state)
{
    static const uint32 expected[] = {0x78, 0x56, 0x34, 0x12};

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
        assert_int_equal(vax_qbus_extract_write_byte(0x12345678u, (uint32)i),
                         expected[i]);
}

/* Verify write word extraction avoids signed right-shift semantics. */
static void test_extract_write_word_handles_high_bit_values(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(vax_qbus_extract_write_word(0xff001234u, 0), 0x1234u);
    assert_int_equal(vax_qbus_extract_write_word(0xff001234u, 1), 0x0012u);
    assert_int_equal(vax_qbus_extract_write_word(0xff001234u, 2), 0xff00u);
}

/* Verify partial write values are positioned with unsigned arithmetic. */
static void test_position_write_value_handles_high_bit_values(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(vax_qbus_position_write_value(3, 0xffu, L_BYTE),
                     0xff000000u);
    assert_int_equal(vax_qbus_position_write_value(2, 0xffffu, L_WORD),
                     0xffff0000u);
    assert_int_equal(vax_qbus_position_write_value(0, 0x87654321u, L_LONG),
                     0x87654321u);
}

/* Verify partial register writes preserve non-selected fields. */
static void test_replace_write_field_covers_byte_word_and_long(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(vax_qbus_replace_write_field(0x12345678u, 3, 0xa5u,
                                                  L_BYTE),
                     0xa5345678u);
    assert_int_equal(vax_qbus_replace_write_field(0x12345678u, 1, 0xabcdu,
                                                  L_WORD),
                     0x12abcd78u);
    assert_int_equal(vax_qbus_replace_write_field(0x12345678u, 0,
                                                  0x87654321u, L_LONG),
                     0x87654321u);
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
        cmocka_unit_test(test_extract_write_byte_covers_every_byte),
        cmocka_unit_test(test_extract_write_word_handles_high_bit_values),
        cmocka_unit_test(test_position_write_value_handles_high_bit_values),
        cmocka_unit_test(test_replace_write_field_covers_byte_word_and_long),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
