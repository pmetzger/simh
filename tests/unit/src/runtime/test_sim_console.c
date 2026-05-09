#include <stddef.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_console.h"

static void test_sim_fd_isatty_rejects_invalid_fd(void **state)
{
    (void)state;

    assert_false(sim_fd_isatty(-1));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sim_fd_isatty_rejects_invalid_fd),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
