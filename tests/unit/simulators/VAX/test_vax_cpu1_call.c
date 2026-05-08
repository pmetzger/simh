#include <stddef.h>

#include "test_cmocka.h"

#include "vax_cpu1_internal.h"

static int32 old_int_bool_call_frame_word(uint32 sp, int gs, int32 mask,
                                          uint32 psl)
{
    uint32 word;

    word =
        ((sp & (uint32)CALL_M_SPA) << CALL_V_SPA) | (uint32)(gs << CALL_V_S) |
        (((uint32)mask & (uint32)CALL_MASK) << CALL_V_MASK) | (psl & 0xFFE0u);
    return (int32)word;
}

static void test_call_frame_word_matches_old_int_bool_expression(void **state)
{
    static const struct {
        uint32 sp;
        int32 mask;
        uint32 psl;
    } cases[] = {
        {0x00000000u, 0x0000, 0x00000000u}, {0x00000001u, 0x0001, 0x00000020u},
        {0x00000002u, 0x0555, 0x0000FFE0u}, {0x00000003u, 0x0FFF, 0xFFFFFFFFu},
        {0x80000003u, 0xFFFF, 0x00F0FFE0u},
    };
    size_t i;

    (void)state;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert_int_equal(vax_call_frame_word(cases[i].sp, FALSE, cases[i].mask,
                                             cases[i].psl),
                         old_int_bool_call_frame_word(
                             cases[i].sp, 0, cases[i].mask, cases[i].psl));
        assert_int_equal(
            vax_call_frame_word(cases[i].sp, TRUE, cases[i].mask, cases[i].psl),
            old_int_bool_call_frame_word(cases[i].sp, 1, cases[i].mask,
                                         cases[i].psl));
    }
}

static void test_call_frame_word_sets_s_flag_only_for_calls(void **state)
{
    int32 callg_word;
    int32 calls_word;

    (void)state;

    callg_word = vax_call_frame_word(0, FALSE, 0, 0);
    calls_word = vax_call_frame_word(0, TRUE, 0, 0);

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
