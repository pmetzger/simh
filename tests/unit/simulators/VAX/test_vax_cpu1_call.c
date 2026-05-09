#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "vax_cpu1_internal.h"

static int32_t old_int_bool_call_frame_word(uint32_t sp, int gs, int32_t mask,
                                          uint32_t psl)
{
    uint32_t word;

    word =
        ((sp & (uint32_t)CALL_M_SPA) << CALL_V_SPA) | (uint32_t)(gs << CALL_V_S) |
        (((uint32_t)mask & (uint32_t)CALL_MASK) << CALL_V_MASK) | (psl & 0xFFE0u);
    return (int32_t)word;
}

static void test_call_frame_word_matches_old_int_bool_expression(void **state)
{
    static const struct {
        uint32_t sp;
        int32_t mask;
        uint32_t psl;
    } cases[] = {
        {0x00000000u, 0x0000, 0x00000000u}, {0x00000001u, 0x0001, 0x00000020u},
        {0x00000002u, 0x0555, 0x0000FFE0u}, {0x00000003u, 0x0FFF, 0xFFFFFFFFu},
        {0x80000003u, 0xFFFF, 0x00F0FFE0u},
    };
    size_t i;

    (void)state;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert_int_equal(vax_call_frame_word(cases[i].sp, false, cases[i].mask,
                                             cases[i].psl),
                         old_int_bool_call_frame_word(
                             cases[i].sp, 0, cases[i].mask, cases[i].psl));
        assert_int_equal(
            vax_call_frame_word(cases[i].sp, true, cases[i].mask, cases[i].psl),
            old_int_bool_call_frame_word(cases[i].sp, 1, cases[i].mask,
                                         cases[i].psl));
    }
}

static void test_call_frame_word_sets_s_flag_only_for_calls(void **state)
{
    int32_t callg_word;
    int32_t calls_word;

    (void)state;

    callg_word = vax_call_frame_word(0, false, 0, 0);
    calls_word = vax_call_frame_word(0, true, 0, 0);

    assert_int_equal(callg_word & CALL_S, 0);
    assert_int_equal(calls_word & CALL_S, CALL_S);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_call_frame_word_matches_old_int_bool_expression),
        cmocka_unit_test(test_call_frame_word_sets_s_flag_only_for_calls),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
