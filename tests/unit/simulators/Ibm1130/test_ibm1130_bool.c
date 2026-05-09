#include <stdbool.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "ibm1130_bool_internal.h"

static void test_mask_values_are_normalized_to_predicates(void **state)
{
    (void)state;

    assert_false(ibm1130_mask_is_set(0, 010));
    assert_false(ibm1130_mask_is_set(020, 010));
    assert_true(ibm1130_mask_is_set(030, 010));
}

static void test_switch_predicate_ignores_unrelated_switches(void **state)
{
    (void)state;

    assert_false(ibm1130_switch_requested(0, SWMASK('G')));
    assert_false(ibm1130_switch_requested(SWMASK('M'), SWMASK('G')));
    assert_true(ibm1130_switch_requested(SWMASK('G'), SWMASK('G')));
    assert_true(
        ibm1130_switch_requested(SWMASK('G') | SWMASK('M'), SWMASK('G')));
}

static void test_timer_state_predicate_requires_running_mask(void **state)
{
    const uint32_t running = 1;
    const uint32_t inhibited = 2;
    const uint32_t timed_out = 4;

    (void)state;

    assert_false(ibm1130_any_state_mask_is_set(0, 0, 0, running));
    assert_false(
        ibm1130_any_state_mask_is_set(inhibited, timed_out, 0, running));
    assert_true(ibm1130_any_state_mask_is_set(running, 0, 0, running));
    assert_true(
        ibm1130_any_state_mask_is_set(0, inhibited | running, 0, running));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mask_values_are_normalized_to_predicates),
        cmocka_unit_test(test_switch_predicate_ignores_unrelated_switches),
        cmocka_unit_test(test_timer_state_predicate_requires_running_mask),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
