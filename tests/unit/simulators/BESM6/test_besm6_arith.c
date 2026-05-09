#include <stdint.h>

#include "test_cmocka.h"

#include "besm6_arith_internal.h"

uint32_t RAU;
t_value ACC, RMR;
jmp_buf cpu_halt;

static t_value make_besm6_word(unsigned exponent, uint64_t mantissa)
{
    return ((t_value)(exponent & BITS(7)) << 41) | (mantissa & BITS41);
}

static uint64_t make_alu_mantissa(uint64_t word_mantissa)
{
    if (word_mantissa & BIT41)
        return word_mantissa | BIT42;
    return word_mantissa & BITS41;
}

/*
 * BESM-6 addition shifts the operand with the smaller exponent before adding
 * the mantissas. Negative operands must be filled with one bits in the high
 * end of the 42-bit internal ALU mantissa, not with whatever a host compiler
 * happens to do for a signed negative left shift.
 */
static void expect_right_normalized(unsigned *exponent, uint64_t *mantissa,
                                    uint64_t *mr, int *round_requested)
{
    switch ((*mantissa >> 40) & 3) {
    case 2:
    case 1:
        *round_requested |= *mantissa & 1;
        *mr = (*mr >> 1) | ((*mantissa & 1) << 39);
        *mantissa >>= 1;
        ++*exponent;
    }
}

static void reset_add_state(t_value acc)
{
    RAU = RAU_NORM_DISABLE | RAU_ROUND_DISABLE | RAU_OVF_DISABLE;
    ACC = acc;
    RMR = 0;
}

static void call_besm6_add(t_value value)
{
    int halt_code = setjmp(cpu_halt);

    assert_int_equal(halt_code, 0);
    if (halt_code == 0)
        besm6_add(value, 0, 0);
}

/*
 * Check the private fill helper directly so the warning fix has a small,
 * exact test for the mask that replaces the undefined signed shift.
 */
static void test_alu_sign_extension_mask_sets_high_bits(void **state)
{
    (void)state;

    assert_int_equal(besm6_alu_sign_extension_mask(0), BITS42);
    assert_int_equal(besm6_alu_sign_extension_mask(1), BITS42 & ~1);
    assert_int_equal(besm6_alu_sign_extension_mask(36),
                     BITS42 & ~(((uint64_t)1 << 36) - 1));
    assert_int_equal(besm6_alu_sign_extension_mask(39),
                     BITS42 & ~(((uint64_t)1 << 39) - 1));
}

/*
 * A negative addend whose exponent is within 40 of ACC contributes some bits
 * to the main mantissa and the rest to RMR. This exercises the first
 * sign-extension path in besm6_add().
 */
static void test_add_sign_extends_negative_close_exponent(void **state)
{
    const unsigned acc_exponent = 64;
    const unsigned value_exponent = 60;
    const unsigned diff = acc_exponent - value_exponent;
    const uint64_t value_mantissa = BIT41 | 0123456701234LL;
    const uint64_t alu_mantissa = make_alu_mantissa(value_mantissa);
    uint64_t expected_mantissa;
    uint64_t expected_mr;
    int expected_round;
    unsigned expected_exponent;

    (void)state;

    expected_mr = (alu_mantissa << (40 - diff)) & BITS40;
    expected_round = expected_mr != 0;
    expected_mantissa =
        ((alu_mantissa >> diff) | besm6_alu_sign_extension_mask(40 - diff)) &
        BITS42;
    expected_exponent = acc_exponent;
    expect_right_normalized(&expected_exponent, &expected_mantissa,
                            &expected_mr, &expected_round);

    reset_add_state(make_besm6_word(acc_exponent, 0));
    call_besm6_add(make_besm6_word(value_exponent, value_mantissa));

    assert_int_equal(ACC,
                     make_besm6_word(expected_exponent, expected_mantissa));
    assert_int_equal(RMR, expected_mr);
}

/*
 * A negative addend whose exponent is more than 40 away still leaves a signed
 * residue in RMR. This exercises the second sign-extension path in
 * besm6_add().
 */
static void test_add_sign_extends_negative_far_exponent(void **state)
{
    const unsigned acc_exponent = 64;
    const unsigned value_exponent = 10;
    const unsigned diff = acc_exponent - value_exponent - 40;
    const uint64_t value_mantissa = BIT41 | 076543210123LL;
    const uint64_t alu_mantissa = make_alu_mantissa(value_mantissa);
    uint64_t expected_mantissa;
    uint64_t expected_mr;
    int expected_round;
    unsigned expected_exponent;

    (void)state;

    expected_mr =
        ((alu_mantissa >> diff) | besm6_alu_sign_extension_mask(40 - diff)) &
        BITS40;
    expected_round = alu_mantissa != 0;
    expected_mantissa = BITS42;
    expected_exponent = acc_exponent;
    expect_right_normalized(&expected_exponent, &expected_mantissa,
                            &expected_mr, &expected_round);

    reset_add_state(make_besm6_word(acc_exponent, 0));
    call_besm6_add(make_besm6_word(value_exponent, value_mantissa));

    assert_int_equal(ACC,
                     make_besm6_word(expected_exponent, expected_mantissa));
    assert_int_equal(RMR, expected_mr);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_alu_sign_extension_mask_sets_high_bits),
        cmocka_unit_test(test_add_sign_extends_negative_close_exponent),
        cmocka_unit_test(test_add_sign_extends_negative_far_exponent),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
