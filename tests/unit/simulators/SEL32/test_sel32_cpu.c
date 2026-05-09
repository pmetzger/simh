#include <stdint.h>

#include "test_cmocka.h"

#include "sel32_arith_internal.h"

/* Verify signed word interpretation is explicit for all sign boundaries. */
static void test_word_as_signed_handles_boundaries(void **state)
{
    (void)state;

    assert_int_equal(sel32_word_as_signed(0x00000000), 0);
    assert_int_equal(sel32_word_as_signed(0x00000001), 1);
    assert_int_equal(sel32_word_as_signed(0x7fffffff), 2147483647);
    assert_int_equal(sel32_word_as_signed(0xffffffff), -1);
    assert_int_equal(sel32_word_as_signed(0xffff8000), -32768);
    assert_int_equal(sel32_word_as_signed(0x80000000), (-2147483647 - 1));
}

/* Verify CAMx word comparison uses signed ordering without subtraction. */
static void test_cam_word_result_handles_signed_extremes(void **state)
{
    static const struct {
        uint32_t left;
        uint32_t right;
        uint64_t expected;
    } cases[] = {
        {0x00000000, 0x00000000, 0},
        {0x00000001, 0x00000000, 1},
        {0xffffffff, 0xffffffff, 0},
        {0x80000000, 0x80000000, 0},
        {0xffffffff, 0x00000000, (uint64_t)-1},
        {0x7fffffff, 0xffff8000, 1},
        {0xffff8000, 0x7fffffff, (uint64_t)-1},
        {0x80000000, 0x7fffffff, (uint64_t)-1},
        {0x7fffffff, 0x80000000, 1},
        {0x80000000, 0xffffffff, (uint64_t)-1},
        {0xffffffff, 0x80000000, 1},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        assert_int_equal(sel32_cam_word_result(cases[i].left, cases[i].right),
                         cases[i].expected);
}

/* Verify CAMx doubleword comparison covers signed 64-bit edge values. */
static void test_cam_double_result_handles_signed_extremes(void **state)
{
    static const struct {
        uint64_t left;
        uint64_t right;
        uint64_t expected;
    } cases[] = {
        {0, 0, 0},
        {1, 0, 1},
        {0xffffffffffffffffULL, 0xffffffffffffffffULL, 0},
        {0x8000000000000000ULL, 0x8000000000000000ULL, 0},
        {(uint64_t)-1, 0, (uint64_t)-1},
        {0x7fffffffffffffffULL, 0xffffffffffff8000ULL, 1},
        {0xffffffffffff8000ULL, 0x7fffffffffffffffULL, (uint64_t)-1},
        {0x8000000000000000ULL, 0x7fffffffffffffffULL, (uint64_t)-1},
        {0x7fffffffffffffffULL, 0x8000000000000000ULL, 1},
        {0x8000000000000000ULL, 0xffffffffffffffffULL, (uint64_t)-1},
        {0xffffffffffffffffULL, 0x8000000000000000ULL, 1},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        assert_int_equal(sel32_cam_double_result(cases[i].left, cases[i].right),
                         cases[i].expected);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_word_as_signed_handles_boundaries),
        cmocka_unit_test(test_cam_word_result_handles_signed_extremes),
        cmocka_unit_test(test_cam_double_result_handles_signed_extremes),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
