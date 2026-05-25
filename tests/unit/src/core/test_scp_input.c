#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "scp_internal.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"

struct scp_input_fixture {
    char *saved_prompt;
    char *(*saved_vm_read)(char *ptr, int32_t size, FILE *stream);
};

static const char *fake_input_lines[4];
static size_t fake_input_count;
static size_t fake_input_index;

/*
 * Return command input from a fixed test sequence.  A NULL entry simulates
 * EOF or a failed command read.
 */
static char *fake_vm_read(char *ptr, int32_t size, FILE *stream)
{
    const char *line;

    (void)stream;
    assert_true(fake_input_index < fake_input_count);
    line = fake_input_lines[fake_input_index++];
    if (line == NULL)
        return NULL;
    strlcpy(ptr, line, (size_t)size);
    return ptr;
}

/* Install enough SCP state to exercise the command prompt loop directly. */
static int setup_scp_input_fixture(void **state)
{
    struct scp_input_fixture *fixture;
    DEVICE *devices[] = {NULL};

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    assert_int_equal(simh_test_install_devices("zimh-unit-scp-input", devices),
                     0);
    fixture->saved_prompt = sim_prompt;
    fixture->saved_vm_read = sim_vm_read;
    sim_prompt = "sim> ";
    sim_vm_read = fake_vm_read;
    *state = fixture;
    return 0;
}

/* Restore global SCP state changed by the input tests. */
static int teardown_scp_input_fixture(void **state)
{
    struct scp_input_fixture *fixture = *state;

    sim_prompt = fixture->saved_prompt;
    sim_vm_read = fixture->saved_vm_read;
    simh_test_reset_simulator_state();
    free(fixture);
    *state = NULL;
    return 0;
}

/* EOF or a read error at the command prompt should exit the command loop. */
static void test_missing_command_line_exits_prompt_loop(void **state)
{
    char *argv[] = {NULL};

    (void)state;
    fake_input_lines[0] = NULL;
    fake_input_count = 1;
    fake_input_index = 0;

    assert_int_equal(process_stdin_commands(SCPE_OK, argv, false), SCPE_OK);
    assert_int_equal(fake_input_index, 1);
}

/* Blank command lines remain no-ops and the prompt continues reading. */
static void test_blank_command_line_does_not_exit_prompt_loop(void **state)
{
    char *argv[] = {NULL};

    (void)state;
    fake_input_lines[0] = "";
    fake_input_lines[1] = NULL;
    fake_input_count = 2;
    fake_input_index = 0;

    assert_int_equal(process_stdin_commands(SCPE_OK, argv, false), SCPE_OK);
    assert_int_equal(fake_input_index, 2);
}

/* Non-empty command lines are still dispatched normally. */
static void test_nonblank_command_line_is_processed(void **state)
{
    char *argv[] = {NULL};

    (void)state;
    fake_input_lines[0] = "exit";
    fake_input_count = 1;
    fake_input_index = 0;

    assert_int_equal(process_stdin_commands(SCPE_OK, argv, false), SCPE_EXIT);
    assert_int_equal(fake_input_index, 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_missing_command_line_exits_prompt_loop,
            setup_scp_input_fixture, teardown_scp_input_fixture),
        cmocka_unit_test_setup_teardown(
            test_blank_command_line_does_not_exit_prompt_loop,
            setup_scp_input_fixture, teardown_scp_input_fixture),
        cmocka_unit_test_setup_teardown(test_nonblank_command_line_is_processed,
                                        setup_scp_input_fixture,
                                        teardown_scp_input_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
