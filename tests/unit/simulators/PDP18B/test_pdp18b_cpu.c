#include <stdint.h>

#include "test_cmocka.h"

#include "pdp18b_cpu_internal.h"
#include "sim_types.h"

/* Verify RAR moves the link into AC sign without signed host overflow. */
static void test_lac_rotate_right_moves_link_to_sign(void **state)
{
    (void)state;

    assert_int_equal(pdp18b_lac_rotate_right(LINK, 1), SIGN);
}

/* Verify RAR wraps AC bit 0 into the link bit. */
static void test_lac_rotate_right_wraps_bit_zero_to_link(void **state)
{
    (void)state;

    assert_int_equal(pdp18b_lac_rotate_right(1, 1), LINK);
}

/* Verify RTR wraps the low two AC bits into AC sign and link. */
static void test_lac_rotate_right_two_places_wraps_low_bits(void **state)
{
    (void)state;

    assert_int_equal(pdp18b_lac_rotate_right(03, 2), LINK | SIGN);
}

/* Verify left and right rotations are inverses across the 19-bit field. */
static void test_lac_rotates_preserve_one_hot_bits(void **state)
{
    (void)state;

    for (uint_t bit = 0; bit < 19; ++bit) {
        uint32_t value = (uint32_t)1 << bit;

        for (uint_t count = 0; count < 19; ++count) {
            assert_int_equal(
                pdp18b_lac_rotate_right(
                    pdp18b_lac_rotate_left(value, count), count),
                value);
            assert_int_equal(
                pdp18b_lac_rotate_left(
                    pdp18b_lac_rotate_right(value, count), count),
                value);
        }
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_lac_rotate_right_moves_link_to_sign),
        cmocka_unit_test(test_lac_rotate_right_wraps_bit_zero_to_link),
        cmocka_unit_test(test_lac_rotate_right_two_places_wraps_low_bits),
        cmocka_unit_test(test_lac_rotates_preserve_one_hot_bits),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
