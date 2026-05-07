#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "scp_expr.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"
#include "test_support.h"

static t_value expr_accumulator = 0;
static REG expr_registers[] = {{ORDATA(ACC, expr_accumulator, 16)}, {NULL}};

struct scp_expr_fixture {
    DEVICE device;
    UNIT unit;
};

static int setup_scp_expr_fixture(void **state)
{
    struct scp_expr_fixture *fixture;
    DEVICE *devices[2];

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    simh_test_init_device_unit(&fixture->device, &fixture->unit, "CPU", "CPU0",
                               0, 0, 16, 1);
    fixture->device.registers = expr_registers;
    expr_accumulator = 5;

    devices[0] = &fixture->device;
    devices[1] = NULL;
    assert_int_equal(
        simh_test_install_devices("zimh-unit-scp-expr", devices), 0);

    sim_dfdev = &fixture->device;
    scp_set_exp_argv(NULL);
    sim_switches = 0;
    sim_switch_number = 0;

    *state = fixture;
    return 0;
}

static int teardown_scp_expr_fixture(void **state)
{
    struct scp_expr_fixture *fixture = *state;

    scp_set_exp_argv(NULL);
    simh_test_reset_simulator_state();
    free(fixture);
    *state = NULL;
    return 0;
}

/* Verify numeric expressions honor precedence and nested parentheses. */
static void test_sim_eval_expression_handles_numeric_precedence(void **state)
{
    t_svalue value;
    t_stat status;
    const char *rest;

    (void)state;

    rest = sim_eval_expression("1 + 2 * (3 + 4)", &value, FALSE, &status);

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(value, 15);
    assert_string_equal(rest, "");
}

/* Verify paren-required parsing stops after one balanced expression. */
static void
test_sim_eval_expression_returns_rest_after_paren_group(void **state)
{
    t_svalue value;
    t_stat status;
    const char *rest;

    (void)state;

    rest = sim_eval_expression("(1 + 2) trailing", &value, TRUE, &status);

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(value, 3);
    assert_string_equal(rest, "trailing");
}

/* Verify register names and qualified register names resolve correctly. */
static void test_sim_eval_expression_uses_register_values(void **state)
{
    struct scp_expr_fixture *fixture = *state;
    t_svalue value;
    t_stat status;

    (void)fixture;

    assert_string_equal(sim_eval_expression("ACC + 1", &value, FALSE, &status),
                        "");
    assert_int_equal(status, SCPE_OK);
    assert_int_equal(value, 6);

    assert_string_equal(
        sim_eval_expression("CPU.ACC * 2", &value, FALSE, &status), "");
    assert_int_equal(status, SCPE_OK);
    assert_int_equal(value, 10);
}

/* Verify environment variables participate in expression evaluation. */
static void test_sim_eval_expression_uses_environment_values(void **state)
{
    t_svalue value;
    t_stat status;

    (void)state;

    setenv("SIMH_TEST_EXPR_VALUE", "41", 1);
    assert_string_equal(
        sim_eval_expression("SIMH_TEST_EXPR_VALUE + 1", &value, FALSE, &status),
        "");
    assert_int_equal(status, SCPE_OK);
    assert_int_equal(value, 42);
    unsetenv("SIMH_TEST_EXPR_VALUE");
}

/* Verify argument substitution feeds quoted-string comparisons. */
static void
test_sim_eval_expression_expands_argument_substitutions(void **state)
{
    char *argv[] = {"DO SOMETHING", "world", NULL};
    t_svalue value;
    t_stat status;

    (void)state;

    scp_set_exp_argv(argv);

    assert_string_equal(sim_eval_expression("\"hello %1\" == \"hello world\"",
                                            &value, FALSE, &status),
                        "");
    assert_int_equal(status, SCPE_OK);
    assert_int_equal(value, 1);
}

/* Verify string comparisons respect case-insensitive switch handling. */
static void
test_sim_eval_expression_honors_case_insensitive_string_compare(void **state)
{
    t_svalue value;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('I');
    assert_string_equal(
        sim_eval_expression("\"HELLO\" == \"hello\"", &value, FALSE, &status),
        "");
    assert_int_equal(status, SCPE_OK);
    assert_int_equal(value, 1);
}

/* Verify whitespace folding and case folding compose for string compares. */
static void
test_sim_eval_expression_honors_whitespace_case_string_compare(void **state)
{
    t_svalue value;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('W') | SWMASK('I');
    assert_string_equal(
        sim_eval_expression("\"ALPHA\t beta\" == \"alpha beta\"", &value,
                            FALSE, &status),
        "");
    assert_int_equal(status, SCPE_OK);
    assert_int_equal(value, 1);
}

/* Verify malformed expressions report an argument error and partial rest. */
static void test_sim_eval_expression_reports_invalid_syntax(void **state)
{
    t_svalue value;
    t_stat status;
    const char *rest;

    (void)state;

    rest = sim_eval_expression("1 + )", &value, FALSE, &status);

    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_ARG);
    assert_non_null(rest);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_sim_eval_expression_handles_numeric_precedence,
            setup_scp_expr_fixture, teardown_scp_expr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_eval_expression_returns_rest_after_paren_group,
            setup_scp_expr_fixture, teardown_scp_expr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_eval_expression_uses_register_values,
            setup_scp_expr_fixture, teardown_scp_expr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_eval_expression_uses_environment_values,
            setup_scp_expr_fixture, teardown_scp_expr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_eval_expression_expands_argument_substitutions,
            setup_scp_expr_fixture, teardown_scp_expr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_eval_expression_honors_case_insensitive_string_compare,
            setup_scp_expr_fixture, teardown_scp_expr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_eval_expression_honors_whitespace_case_string_compare,
            setup_scp_expr_fixture, teardown_scp_expr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_eval_expression_reports_invalid_syntax,
            setup_scp_expr_fixture, teardown_scp_expr_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
