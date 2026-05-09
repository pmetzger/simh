#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "h316_imp.h"

static void test_physical_port_switch_is_false_when_absent(void **state)
{
    (void)state;

    assert_false(h316_physical_port_switch_requested(0));
}

static void test_physical_port_switch_returns_predicate(void **state)
{
    int32_t switches = SWMASK('P') | SWMASK('X');

    (void)state;

    assert_true(h316_physical_port_switch_requested(switches));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_physical_port_switch_is_false_when_absent),
        cmocka_unit_test(test_physical_port_switch_returns_predicate),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
