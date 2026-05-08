#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "uint_bits.h"

/* Verify low masks cover edge widths without undefined shifts. */
static void test_low_mask_covers_edges(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_low_mask(0), 0u);
    assert_int_equal(u32_low_mask(1), 0x00000001u);
    assert_int_equal(u32_low_mask(8), 0x000000ffu);
    assert_int_equal(u32_low_mask(16), 0x0000ffffu);
    assert_int_equal(u32_low_mask(31), 0x7fffffffu);
    assert_int_equal(u32_low_mask(32), 0xffffffffu);
}

/* Verify field masks can safely set the high bit of a uint32_t. */
static void test_field_mask_covers_high_fields(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_field_mask(24, 8), 0xff000000u);
    assert_int_equal(u32_field_mask(16, 16), 0xffff0000u);
    assert_int_equal(u32_field_mask(0, 32), 0xffffffffu);
}

/* Verify making fields masks the source value before shifting. */
static void test_make_field_masks_source_value(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_make_field(0x1a5, 24, 8), 0xa5000000u);
    assert_int_equal(u32_make_field(0x1d617, 16, 16), 0xd6170000u);
    assert_int_equal(u32_make_field(0x87654321u, 0, 32), 0x87654321u);
}

/* Verify field extraction returns a right-justified value. */
static void test_get_field_right_justifies_result(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_get_field(0xa5345678u, 24, 8), 0xa5u);
    assert_int_equal(u32_get_field(0xd6175678u, 16, 16), 0xd617u);
    assert_int_equal(u32_get_field(0x87654321u, 0, 32), 0x87654321u);
}

/* Verify putting a field preserves bits outside the target field. */
static void test_put_field_preserves_other_bits(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_put_field(0x12345678u, 0xa5, 24, 8), 0xa5345678u);
    assert_int_equal(u32_put_field(0x12345678u, 0xd617, 16, 16), 0xd6175678u);
    assert_int_equal(u32_put_field(0x12345678u, 0x87654321u, 0, 32),
                     0x87654321u);
}

/* Verify putting a u8 field covers every aligned field and masks values. */
static void test_put_u8_le_covers_all_fields(void **state)
{
    static const struct {
        uint32_t addr;
        uint32_t expected;
    } cases[] = {
        {0, 0x123456a5u},
        {1, 0x1234a578u},
        {2, 0x12a55678u},
        {3, 0xa5345678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(u32_put_addr_u8_le(0x12345678u, 0x1a5, cases[i].addr),
                         cases[i].expected);
}

/* Verify putting a u16 field covers both aligned fields and masks values. */
static void test_put_u16_le_covers_both_fields(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_put_addr_u16_le(0x12345678u, 0x1d617, 0), 0x1234d617u);
    assert_int_equal(u32_put_addr_u16_le(0x12345678u, 0x1d617, 2), 0xd6175678u);
}

/* Verify making a u16 field covers both aligned fields and masks values. */
static void test_make_u16_le_covers_both_fields(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_make_addr_u16_le(0x1d617, 0), 0x0000d617u);
    assert_int_equal(u32_make_addr_u16_le(0x1d617, 2), 0xd6170000u);
}

/* Verify u8 and u16 field extraction returns right-justified values. */
static void test_aligned_getters_le_right_justify_results(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_get_addr_u8_le(0xa5345678u, 0), 0x78u);
    assert_int_equal(u32_get_addr_u8_le(0xa5345678u, 1), 0x56u);
    assert_int_equal(u32_get_addr_u8_le(0xa5345678u, 2), 0x34u);
    assert_int_equal(u32_get_addr_u8_le(0xa5345678u, 3), 0xa5u);
    assert_int_equal(u32_get_addr_u16_le(0xd6175678u, 0), 0x5678u);
    assert_int_equal(u32_get_addr_u16_le(0xd6175678u, 2), 0xd617u);
}

/* Verify putting counted u8 fields handles every supported count. */
static void test_put_u8_count_le_covers_supported_counts(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_put_addr_u8_count_le(0x12345678u, 0x1a5, 3, 1),
                     0xa5345678u);
    assert_int_equal(u32_put_addr_u8_count_le(0x12345678u, 0x1d617, 2, 2),
                     0xd6175678u);
    assert_int_equal(u32_put_addr_u8_count_le(0x12345678u, 0x1d617a5, 0, 3),
                     0x12d617a5u);
    assert_int_equal(u32_put_addr_u8_count_le(0x12345678u, 0x1d617a5, 1, 3),
                     0xd617a578u);
    assert_int_equal(u32_put_addr_u8_count_le(0x12345678u, 0x87654321u, 0, 4),
                     0x87654321u);
}

/* Verify making counted u8 fields handles every supported count. */
static void test_make_u8_count_le_covers_supported_counts(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_make_addr_u8_count_le(0x1a5, 3, 1), 0xa5000000u);
    assert_int_equal(u32_make_addr_u8_count_le(0x1d617, 2, 2), 0xd6170000u);
    assert_int_equal(u32_make_addr_u8_count_le(0x1d617a5, 0, 3), 0x00d617a5u);
    assert_int_equal(u32_make_addr_u8_count_le(0x1d617a5, 1, 3), 0xd617a500u);
    assert_int_equal(u32_make_addr_u8_count_le(0x87654321u, 0, 4), 0x87654321u);
}

/* Verify u8 and u16 extraction split right-justified values. */
static void test_extract_helpers_split_values(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_get_u8(0xa5345678u, 0), 0x78u);
    assert_int_equal(u32_get_u8(0xa5345678u, 1), 0x56u);
    assert_int_equal(u32_get_u8(0xa5345678u, 2), 0x34u);
    assert_int_equal(u32_get_u8(0xa5345678u, 3), 0xa5u);
    assert_int_equal(u32_get_u16(0xa5345678u, 0), 0x5678u);
    assert_int_equal(u32_get_u16(0xa5345678u, 1), 0x3456u);
    assert_int_equal(u32_get_u16(0xa5345678u, 2), 0xa534u);
}

/* Verify u16 helpers mask and position 16-bit values. */
static void test_u16_helpers_preserve_16_bit_fields(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal(u32_from_u16_pair(0x15678, 0x18034), 0x80345678u);
    assert_int_equal(u32_low_u16(0x80345678u), 0x5678u);
    assert_int_equal(u32_high_u16(0x80345678u), 0x8034u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_low_mask_covers_edges),
        cmocka_unit_test(test_field_mask_covers_high_fields),
        cmocka_unit_test(test_make_field_masks_source_value),
        cmocka_unit_test(test_get_field_right_justifies_result),
        cmocka_unit_test(test_put_field_preserves_other_bits),
        cmocka_unit_test(test_put_u8_le_covers_all_fields),
        cmocka_unit_test(test_put_u16_le_covers_both_fields),
        cmocka_unit_test(test_make_u16_le_covers_both_fields),
        cmocka_unit_test(test_aligned_getters_le_right_justify_results),
        cmocka_unit_test(test_put_u8_count_le_covers_supported_counts),
        cmocka_unit_test(test_make_u8_count_le_covers_supported_counts),
        cmocka_unit_test(test_extract_helpers_split_values),
        cmocka_unit_test(test_u16_helpers_preserve_16_bit_fields),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
