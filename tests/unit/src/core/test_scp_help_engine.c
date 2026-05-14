#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_tempfile.h"
#include "scp.h"
#include "scp_help.h"
#include "scp_help_engine.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"
#include "test_support.h"

static t_value help_register_value = 0;
static REG help_registers[] = {{ORDATA(ACC, help_register_value, 16)}, {NULL}};

struct scp_help_engine_fixture {
    char temp_dir[1024];
    char help_file[1024];
    DEVICE device;
    UNIT unit;
};

static int setup_scp_help_engine_fixture(void **state)
{
    struct scp_help_engine_fixture *fixture;
    DEVICE *devices[2];

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    assert_int_equal(simh_test_make_temp_dir(fixture->temp_dir,
                                             sizeof(fixture->temp_dir),
                                             "scp-help-engine"),
                     0);
    assert_int_equal(simh_test_join_path(fixture->help_file,
                                         sizeof(fixture->help_file),
                                         fixture->temp_dir, "sample.hlp"),
                     0);

    simh_test_init_device_unit(&fixture->device, &fixture->unit, "CPU", "CPU0",
                               0, 0, 16, 1);
    fixture->device.registers = help_registers;
    devices[0] = &fixture->device;
    devices[1] = NULL;
    assert_int_equal(
        simh_test_install_devices("zimh-unit-scp-help-engine", devices), 0);

    *state = fixture;
    return 0;
}

static int teardown_scp_help_engine_fixture(void **state)
{
    struct scp_help_engine_fixture *fixture = *state;

    assert_int_equal(simh_test_remove_path(fixture->temp_dir), 0);
    simh_test_reset_simulator_state();
    free(fixture);
    *state = NULL;
    return 0;
}

/* Verify the top-level HELP summary prints the expected guidance text. */
static void test_fprint_help_prints_general_help_summary(void **state)
{
    FILE *stream;
    char *text;
    size_t size;

    (void)state;

    stream = sim_tmpfile();
    assert_non_null(stream);
    fprint_help(stream);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_non_null(strstr(text, "Help is available for devices"));
    assert_non_null(strstr(text, "HELP dev"));
    free(text);
    fclose(stream);
}

/* Verify register-help rendering lists the visible register names. */
static void test_fprint_reg_help_lists_device_registers(void **state)
{
    struct scp_help_engine_fixture *fixture = *state;
    FILE *stream;
    char *text;
    size_t size;

    stream = sim_tmpfile();
    assert_non_null(stream);
    fprint_reg_help(stream, &fixture->device);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_non_null(strstr(text, "ACC"));
    free(text);
    fclose(stream);
}

/* Verify structured help flattens a simple topic tree non-interactively. */
static void test_scp_help_flattens_simple_help_text(void **state)
{
    struct scp_help_engine_fixture *fixture = *state;
    const char *help =
        SCP_HELP_L(Main topic summary)
        SCP_HELP_T(1, DETAILS)
        SCP_HELP_L(Detail topic text);
    FILE *stream;
    char *text;
    size_t size;

    stream = sim_tmpfile();
    assert_non_null(stream);
    assert_int_equal(scp_help(stream, &fixture->device, &fixture->unit,
                              SCP_HELP_FLAT | SCP_HELP_ONECMD, help, ""),
                     SCPE_OK);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_non_null(strstr(text, "Main topic summary"));
    assert_non_null(strstr(text, "DETAILS"));
    assert_non_null(strstr(text, "Detail topic text"));
    free(text);
    fclose(stream);
}

/* Verify magic topics render generated help through the help engine. */
static void test_scp_help_flattens_magic_register_topic(void **state)
{
    struct scp_help_engine_fixture *fixture = *state;
    const char *help = SCP_HELP_L(Main topic summary)
        SCP_HELP_T(1, $Registers)
        SCP_HELP_L(Additional register guidance);
    FILE *stream;
    char *text;
    size_t size;

    stream = sim_tmpfile();
    assert_non_null(stream);
    assert_int_equal(scp_help(stream, &fixture->device, &fixture->unit,
                              SCP_HELP_FLAT | SCP_HELP_ONECMD, help, ""),
                     SCPE_OK);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_non_null(strstr(text, "Registers"));
    assert_non_null(strstr(text, "ACC"));
    assert_non_null(strstr(text, "Additional register guidance"));
    free(text);
    fclose(stream);
}

/* Verify help loaded from a host file is parsed and rendered the same way. */
static void test_scp_help_from_file_reads_and_renders_help_text(void **state)
{
    struct scp_help_engine_fixture *fixture = *state;
    static const char help_file_contents[] = " Main file topic\n"
                                             "1 DETAILS\n"
                                             " Detail from file\n";
    FILE *stream;
    char *text;
    size_t size;

    assert_int_equal(simh_test_write_file(fixture->help_file,
                                          help_file_contents,
                                          sizeof(help_file_contents) - 1),
                     0);

    stream = sim_tmpfile();
    assert_non_null(stream);
    assert_int_equal(scp_helpFromFile(stream, &fixture->device, &fixture->unit,
                                      SCP_HELP_FLAT | SCP_HELP_ONECMD,
                                      fixture->help_file, ""),
                     SCPE_OK);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_non_null(strstr(text, "Main file topic"));
    assert_non_null(strstr(text, "Detail from file"));
    free(text);
    fclose(stream);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_fprint_help_prints_general_help_summary,
            setup_scp_help_engine_fixture, teardown_scp_help_engine_fixture),
        cmocka_unit_test_setup_teardown(
            test_fprint_reg_help_lists_device_registers,
            setup_scp_help_engine_fixture, teardown_scp_help_engine_fixture),
        cmocka_unit_test_setup_teardown(test_scp_help_flattens_simple_help_text,
                                        setup_scp_help_engine_fixture,
                                        teardown_scp_help_engine_fixture),
        cmocka_unit_test_setup_teardown(
            test_scp_help_flattens_magic_register_topic,
            setup_scp_help_engine_fixture, teardown_scp_help_engine_fixture),
        cmocka_unit_test_setup_teardown(
            test_scp_help_from_file_reads_and_renders_help_text,
            setup_scp_help_engine_fixture, teardown_scp_help_engine_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
