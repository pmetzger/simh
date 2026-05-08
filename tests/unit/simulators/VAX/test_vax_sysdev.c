#include <stddef.h>

#include "test_cmocka.h"

#include "vax_sysdev_internal.h"

/* Verify the VAX 3900 ROM byte writer preserves non-selected byte lanes. */
static void test_rom_replace_byte_covers_every_byte_lane(void **state)
{
    static const struct {
        uint32 pa;
        uint32 val;
        uint32 expected;
    } cases[] = {
        {0, 0x55, 0x12345655},
        {1, 0xaa, 0x1234aa78},
        {2, 0x5a, 0x125a5678},
        {3, 0xff, 0xff345678},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(vax_sysdev_replace_byte_lane(
                             0x12345678u, cases[i].pa, cases[i].val),
                         cases[i].expected);
}

/* Verify only the low eight bits of the replacement value are stored. */
static void test_rom_replace_byte_masks_replacement_value(void **state)
{
    (void)state;

    assert_int_equal(vax_sysdev_replace_byte_lane(0x12345678u, 3, 0x1a5),
                     0xa5345678u);
}

/* Verify word-lane replacement can set the sign bit without signed shifting. */
static void test_replace_masked_lane_covers_high_word(void **state)
{
    (void)state;

    assert_int_equal(
        vax_sysdev_replace_masked_lane(0x12345678u, 2, 0xffffu, 0xffffu),
        0xffff5678u);
}

/* Verify byte-lane packing can set the sign bit without signed shifting. */
static void test_pack_byte_lane_covers_all_shift_positions(void **state)
{
    static const struct {
        uint32 shift;
        uint32 expected;
    } cases[] = {
        {0, 0x000000ff},
        {8, 0x0000ff00},
        {16, 0x00ff0000},
        {24, 0xff000000},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(vax_sysdev_pack_byte_lane(0xff, cases[i].shift),
                         cases[i].expected);
}

/* Verify right-justified register writes can be shifted into the high lane. */
static void test_shift_lane_value_can_set_sign_bit(void **state)
{
    (void)state;

    assert_int_equal(vax_sysdev_shift_lane_value(0xffu, 3), 0xff000000u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rom_replace_byte_covers_every_byte_lane),
        cmocka_unit_test(test_rom_replace_byte_masks_replacement_value),
        cmocka_unit_test(test_replace_masked_lane_covers_high_word),
        cmocka_unit_test(test_pack_byte_lane_covers_all_shift_positions),
        cmocka_unit_test(test_shift_lane_value_can_set_sign_bit),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
