#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_tempfile.h"
#include "scp.h"
#include "scp_breakpoint.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"
#include "test_support.h"

t_stat ssh_break(FILE *st, const char *cptr, int32_t flg);

static BRKTYPTAB breakpoint_types[] = {
    BRKTYPE('A', "Alpha breakpoint"),
    BRKTYPE('B', "Beta breakpoint"),
    {0, NULL},
};

struct scp_breakpoint_fixture {
    DEVICE device;
    UNIT unit;
};

static int setup_scp_breakpoint_fixture(void **state)
{
    struct scp_breakpoint_fixture *fixture;
    DEVICE *devices[2];

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    simh_test_init_device_unit(&fixture->device, &fixture->unit, "CPU", "CPU0",
                               0, 0, 16, 1);
    fixture->device.aradix = 16;
    fixture->device.awidth = 16;

    devices[0] = &fixture->device;
    devices[1] = NULL;
    assert_int_equal(
        simh_test_install_devices("zimh-unit-scp-breakpoint", devices), 0);

    sim_dflt_dev = &fixture->device;
    sim_brk_types = SWMASK('A') | SWMASK('B');
    sim_brk_dflt = SWMASK('A');
    sim_brk_type_desc = breakpoint_types;
    assert_int_equal(sim_brk_init(), SCPE_OK);

    *state = fixture;
    return 0;
}

static int teardown_scp_breakpoint_fixture(void **state)
{
    struct scp_breakpoint_fixture *fixture = *state;

    sim_brk_clrall(0);
    sim_brk_clract();
    sim_brk_type_desc = NULL;
    sim_brk_types = 0;
    sim_brk_dflt = 0;
    simh_test_reset_simulator_state();
    free(fixture);
    *state = NULL;
    return 0;
}

/* Verify default breakpoint creation, hit detection, and hit messaging. */
static void test_sim_brk_set_and_test_default_breakpoint(void **state)
{
    struct scp_breakpoint_fixture *fixture = *state;
    char action[CBUFSIZE];
    const char *message;

    (void)fixture;

    assert_int_equal(sim_brk_set(0x100, 0, 0, "echo hit"), SCPE_OK);
    assert_non_null(sim_brk_fnd(0x100));
    assert_int_equal(sim_brk_summ, SWMASK('A'));

    assert_int_equal(sim_brk_test(0x100, SWMASK('A')), SWMASK('A'));
    assert_int_equal(sim_brk_match_addr, 0x100);
    assert_int_equal(sim_brk_match_type, SWMASK('A'));
    assert_string_equal(sim_brk_getact(action, sizeof(action)), "echo hit");

    message = sim_brk_message();
    assert_non_null(strstr(message, "Alpha breakpoint"));
}

/* Verify fallback breakpoint hit messages include type and address text. */
static void test_sim_brk_message_formats_fallback_text(void **state)
{
    struct scp_breakpoint_fixture *fixture = *state;
    const char *message;

    (void)fixture;

    sim_brk_type_desc = NULL;
    sim_brk_match_type = SWMASK('B');
    sim_brk_match_addr = 0x2A;

    message = sim_brk_message();
    assert_non_null(strstr(message, "B Breakpoint at:"));
    assert_non_null(strstr(message, "2A"));
}

/* Verify countdown breakpoints ignore early hits and fire on the last one. */
static void test_sim_brk_countdown_defers_until_threshold(void **state)
{
    struct scp_breakpoint_fixture *fixture = *state;

    (void)fixture;

    assert_int_equal(sim_brk_set(0x200, SWMASK('B'), 2, NULL), SCPE_OK);

    assert_int_equal(sim_brk_test(0x200, SWMASK('B')), 0);
    sim_brk_npc(1);
    assert_int_equal(sim_brk_test(0x200, SWMASK('B')), SWMASK('B'));
}

/* Verify queued breakpoint actions are returned in last-in-first-out order. */
static void test_sim_brk_action_queue_preserves_push_order(void **state)
{
    struct scp_breakpoint_fixture *fixture = *state;
    char action[CBUFSIZE];

    (void)fixture;

    sim_brk_setact("first");
    sim_brk_setact("second");

    assert_string_equal(sim_brk_getact(action, sizeof(action)), "second");
    assert_string_equal(sim_brk_getact(action, sizeof(action)), "first");
    assert_null(sim_brk_getact(action, sizeof(action)));
}

/* Verify breakpoint SHOW and CLEAR paths reflect the configured state. */
static void test_sim_brk_show_and_clear_update_output_and_state(void **state)
{
    struct scp_breakpoint_fixture *fixture = *state;
    FILE *stream;
    char *text;
    size_t size;

    (void)fixture;

    assert_int_equal(sim_brk_set(0x20, SWMASK('B'), 0, "echo beta"), SCPE_OK);

    stream = sim_tmpfile();
    assert_non_null(stream);
    assert_int_equal(sim_brk_show(stream, 0x20, SWMASK('B')), SCPE_OK);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_non_null(strstr(text, "B"));
    assert_non_null(strstr(text, "echo beta"));
    free(text);
    fclose(stream);

    assert_int_equal(sim_brk_clr(0x20, SWMASK('B')), SCPE_OK);
    assert_null(sim_brk_fnd(0x20));
}

/*
 * Verify plain BREAK commands with no ";action" leave no queued action
 * on the installed breakpoint.
 */
static void test_ssh_break_without_action_leaves_breakpoint_action_empty(
    void **state)
{
    struct scp_breakpoint_fixture *fixture = *state;
    char action[CBUFSIZE];

    (void)fixture;

    sim_switches = SWMASK('B');
    assert_int_equal(ssh_break(NULL, "34", SSH_ST), SCPE_OK);
    assert_int_equal(sim_brk_test(0x34, SWMASK('B')), SWMASK('B'));
    assert_null(sim_brk_getact(action, sizeof(action)));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_sim_brk_set_and_test_default_breakpoint,
            setup_scp_breakpoint_fixture, teardown_scp_breakpoint_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_brk_message_formats_fallback_text,
            setup_scp_breakpoint_fixture, teardown_scp_breakpoint_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_brk_countdown_defers_until_threshold,
            setup_scp_breakpoint_fixture, teardown_scp_breakpoint_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_brk_action_queue_preserves_push_order,
            setup_scp_breakpoint_fixture, teardown_scp_breakpoint_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_brk_show_and_clear_update_output_and_state,
            setup_scp_breakpoint_fixture, teardown_scp_breakpoint_fixture),
        cmocka_unit_test_setup_teardown(
            test_ssh_break_without_action_leaves_breakpoint_action_empty,
            setup_scp_breakpoint_fixture, teardown_scp_breakpoint_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
