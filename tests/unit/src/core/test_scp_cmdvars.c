#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(_WIN32)
#include <unistd.h>
#else
#include "sim_win32_compat.h"
#endif

#if !defined(_WIN32)
#include <sys/utsname.h>
#endif

#include "test_cmocka.h"

#include "sim_tempfile.h"
#include "scp.h"
#include "sim_console.h"
#include "sim_host_path_internal.h"
#include "test_simh_personality.h"
#include "test_support.h"
#include "zimh_version.h"

static const char simh_test_prog_name[] = "/test/bin/zimh-unit-scp-cmdvars";
static int simh_test_uname_probe_calls = 0;
static int simh_test_localtime_calls = 0;
static struct timespec simh_test_cmdvars_time = {0};
static int simh_test_cmdvars_time_status = 0;

struct simh_test_saved_env {
    const char *name;
    char *value;
};

extern bool sim_runlimit_enabled;
extern int32_t sim_runlimit_value;
extern const char *sim_runlimit_units;
extern t_stat set_verify(int32_t flag, const char *cptr);

static int simh_test_cmdvars_clock_gettime(int clock_id, struct timespec *tp)
{
    (void)clock_id;

    if (tp != NULL)
        *tp = simh_test_cmdvars_time;
    if (simh_test_cmdvars_time_status != 0)
        errno = EINVAL;
    return simh_test_cmdvars_time_status;
}

#if !defined(_WIN32)
static int simh_test_uname_hook_fail(struct utsname *utsname_info)
{
    (void)utsname_info;

    ++simh_test_uname_probe_calls;
    errno = EINVAL;
    return -1;
}

static int simh_test_uname_hook_cached(struct utsname *utsname_info)
{
    ++simh_test_uname_probe_calls;
    memset(utsname_info, 0, sizeof(*utsname_info));
    strlcpy(utsname_info->sysname, "ProbeOS", sizeof(utsname_info->sysname));
    return 0;
}
#endif

static bool simh_test_localtime_hook_fail(time_t now, struct tm *tmnow)
{
    (void)now;
    (void)tmnow;

    ++simh_test_localtime_calls;
    return false;
}

#if defined(_WIN32)
static unsigned long simh_test_win32_temp_path_success(unsigned long size,
                                                       char *buf)
{
    const char *path = "C:\\portable\\tmp\\";

    if (strlen(path) + 1 > size)
        return (unsigned long)(strlen(path) + 1);
    strlcpy(buf, path, size);
    return (unsigned long)strlen(path);
}
#endif

/* Keep command-variable tests isolated from inherited process state. */
static int setup_scp_cmdvars_fixture(void **state)
{
    (void)state;

    unsetenv("ZIMH_TEST_EXTERNAL");
    unsetenv("ZIMH_TEST_SET");
    unsetenv("ZIMH_ALIAS");
    unsetenv("SIM_SEND_DELAY_CONSOLE");
    unsetenv("_FILE_COMPARE_DIFF_OFFSET");
    unsetenv("SIM_OSTYPE");
    unsetenv("SIM_NAME");
    unsetenv("SIM_BIN_NAME");
    unsetenv("SIM_BIN_PATH");
    unsetenv("SIM_DELTA");
    unsetenv("SIM_VERSION_MODE");
    unsetenv("SIM_NULL_DEVICE");
    unsetenv("SIM_TMPDIR");
#if defined(_WIN32)
    sim_host_path_reset_test_hooks();
#endif
    simh_test_reset_simulator_state();
    sim_prog_name = simh_test_prog_name;
    sim_switches = 0;
    sim_rem_cmd_active_line = -1;
    simh_test_uname_probe_calls = 0;
    simh_test_localtime_calls = 0;
    simh_test_cmdvars_time = (struct timespec){0};
    simh_test_cmdvars_time_status = 0;
    sim_time_reset_test_hooks();
#if !defined(_WIN32)
    sim_cmdvars_set_test_uname_hook(NULL);
#endif
    sim_cmdvars_set_test_localtime_hook(NULL);
    return 0;
}

/* Remove test variables and reset SCP globals after each test. */
static int teardown_scp_cmdvars_fixture(void **state)
{
    (void)state;

    unsetenv("ZIMH_TEST_EXTERNAL");
    unsetenv("ZIMH_TEST_SET");
    unsetenv("ZIMH_ALIAS");
    unsetenv("SIM_SEND_DELAY_CONSOLE");
    unsetenv("_FILE_COMPARE_DIFF_OFFSET");
    unsetenv("SIM_OSTYPE");
    unsetenv("SIM_NAME");
    unsetenv("SIM_BIN_NAME");
    unsetenv("SIM_BIN_PATH");
    unsetenv("SIM_DELTA");
    unsetenv("SIM_VERSION_MODE");
    unsetenv("SIM_NULL_DEVICE");
    unsetenv("SIM_TMPDIR");
#if defined(_WIN32)
    sim_host_path_reset_test_hooks();
#endif
    simh_test_reset_simulator_state();
    sim_switches = 0;
    sim_rem_cmd_active_line = -1;
    simh_test_uname_probe_calls = 0;
    simh_test_localtime_calls = 0;
    sim_time_reset_test_hooks();
#if !defined(_WIN32)
    sim_cmdvars_set_test_uname_hook(NULL);
#endif
    sim_cmdvars_set_test_localtime_hook(NULL);
    return 0;
}

/* Save one environment variable so a test can restore process state. */
static struct simh_test_saved_env save_env(const char *name)
{
    const char *value = getenv(name);
    struct simh_test_saved_env saved = {name, value ? strdup(value) : NULL};

    return saved;
}

/* Restore one environment variable saved with save_env. */
static void restore_env(struct simh_test_saved_env *saved)
{
    if (saved->value != NULL) {
        assert_int_equal(setenv(saved->name, saved->value, 1), 0);
        free(saved->value);
        saved->value = NULL;
    } else
        unsetenv(saved->name);
}

/* Expand one SCP command string with an otherwise empty argument vector. */
static void expand_command(const char *input, char *output, size_t size)
{
    char *do_arg[10] = {0};

    strlcpy(output, input, size);
    sim_sub_args(output, size, do_arg);
}

/* Expand one SCP command string with a caller-supplied DO argument vector. */
static void expand_command_with_args(const char *input, char *output,
                                     size_t size, char *do_arg[])
{
    strlcpy(output, input, size);
    sim_sub_args(output, size, do_arg);
}

/* Run one top-level SCP command through its registered command action. */
static t_stat run_scp_command(const char *name, const char *args)
{
    CTAB *cmd = find_cmd(name);

    assert_non_null(cmd);
    return cmd->action(cmd->arg, args);
}

/* Create a small temporary file containing the provided bytes. */
static void write_temp_file(char *path, size_t size, const char *prefix,
                            const char *suffix, const char *contents)
{
    FILE *stream;
    size_t contents_size;

    contents_size = strlen(contents);
    stream = sim_tempfile_open_stream(path, size, prefix, suffix, "wb");
    assert_non_null(stream);
    assert_int_equal(fwrite(contents, 1, contents_size, stream),
                     contents_size);
    assert_int_equal(fclose(stream), 0);
}

/* Create one unique temporary path for tests that pass a filename onward. */
static void write_empty_temp_file(char *path, size_t size, const char *prefix,
                                  const char *suffix)
{
    write_temp_file(path, size, prefix, suffix, "");
}

/* Read one small text file into a fixed local buffer. */
static void read_text_file(const char *path, char *buffer, size_t size)
{
    void *data;
    size_t bytes;

    assert_int_equal(simh_test_read_file(path, &data, &bytes), 0);
    assert_true(bytes < size);
    memcpy(buffer, data, bytes + 1);
    free(data);
}

static void make_env_capture_command(char *command, size_t size,
                                     const char *name, const char *path)
{
#if defined(_WIN32)
    assert_int_equal(snprintf(command, size,
                              "cmd /c \"\"%s\" \"%s\" \"%s\"\"",
                              SIMH_CMDVARS_ENV_HELPER, name, path) <
                         (int)size,
                     1);
#else
    assert_int_equal(snprintf(command, size,
                              "\"%s\" \"%s\" \"%s\"",
                              SIMH_CMDVARS_ENV_HELPER, name, path) <
                         (int)size,
                     1);
#endif
}

static const char *successful_system_command(void)
{
#if defined(_WIN32)
    return "cmd /c exit /b 0";
#else
    return "/usr/bin/true";
#endif
}

/* Temporarily replace stdin with a small prepared input file. */
static void with_redirected_stdin(const char *contents, void (*fn)(void *ctx),
                                  void *ctx)
{
    FILE *stream;
    int saved_stdin;
    int fd;

    stream = sim_tmpfile();
    assert_non_null(stream);
    assert_int_equal(fwrite(contents, 1, strlen(contents), stream),
                     strlen(contents));
    assert_int_equal(fflush(stream), 0);
    rewind(stream);
    fd = fileno(stream);
    assert_true(fd >= 0);

    saved_stdin = dup(STDIN_FILENO);
    assert_true(saved_stdin >= 0);
    assert_int_equal(dup2(fd, STDIN_FILENO), STDIN_FILENO);

    fn(ctx);

    assert_int_equal(dup2(saved_stdin, STDIN_FILENO), STDIN_FILENO);
    close(saved_stdin);
    assert_int_equal(fclose(stream), 0);
}

/* Run one callback with TZ set to a known value, then restore it. */
static void with_timezone(const char *timezone, void (*fn)(void *ctx),
                          void *ctx)
{
    const char *saved_tz = getenv("TZ");
    char *saved_copy = saved_tz ? strdup(saved_tz) : NULL;

    assert_int_equal(setenv("TZ", timezone, 1), 0);
    tzset();
    fn(ctx);
    if (saved_copy != NULL) {
        assert_int_equal(setenv("TZ", saved_copy, 1), 0);
        free(saved_copy);
    } else {
        unsetenv("TZ");
    }
    tzset();
}

/* Verify ordinary host environment variables still expand normally. */
static void test_sim_sub_args_expands_external_environment(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_TEST_EXTERNAL", "value", 1), 0);

    expand_command("A%ZIMH_TEST_EXTERNAL%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AvalueB");

    expand_command("ZIMH_TEST_EXTERNAL tail", expanded, sizeof(expanded));
    assert_string_equal(expanded, "value tail");
}

/* Verify lowercase names still fall back to uppercase host environment. */
static void
test_sim_sub_args_expands_uppercased_external_environment(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_TEST_UPPER", "upper", 1), 0);

    expand_command("A%zimh_test_upper%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AupperB");
}

/* Verify malformed :~ substring modifiers leave the value unchanged. */
static void test_sim_sub_args_handles_malformed_substring_modifier(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_TEST_EXTERNAL", "value", 1), 0);

    expand_command("A%ZIMH_TEST_EXTERNAL:~bogus%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AvalueB");
}

/* Verify %VAR still expands if the closing percent is omitted. */
static void
test_sim_sub_args_expands_variable_without_trailing_percent(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    expand_command("A%SIM_NAME", expanded, sizeof(expanded));
    assert_string_equal(expanded, "Azimh-unit");
}

/* Verify SCP-owned substitution variables can be set and removed directly. */
static void test_sim_sub_vars_set_get_and_unset(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_sub_var_set("ZIMH_LOCAL", "hello");
    assert_string_equal(
        _sim_get_env_special("ZIMH_LOCAL", value, sizeof(value)), "hello");

    sim_sub_var_set("ZIMH_LOCAL", "updated");
    assert_string_equal(
        _sim_get_env_special("ZIMH_LOCAL", value, sizeof(value)), "updated");

    sim_sub_var_unset("ZIMH_LOCAL");
    assert_null(_sim_get_env_special("ZIMH_LOCAL", value, sizeof(value)));
}

/* Verify prefix-based clearing removes only the matching variables. */
static void test_sim_sub_var_clear_prefix(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_sub_var_set("_EXPECT_MATCH_GROUP_0", "first");
    sim_sub_var_set("_EXPECT_MATCH_GROUP_1", "second");
    sim_sub_var_set("ZIMH_LOCAL", "keep");

    sim_sub_var_clear_prefix("_EXPECT_MATCH_");

    assert_null(
        _sim_get_env_special("_EXPECT_MATCH_GROUP_0", value, sizeof(value)));
    assert_null(
        _sim_get_env_special("_EXPECT_MATCH_GROUP_1", value, sizeof(value)));
    assert_string_equal(
        _sim_get_env_special("ZIMH_LOCAL", value, sizeof(value)), "keep");
}

/* Verify the documented date, time, and status built-ins still expand. */
static void test_sim_get_env_special_expands_documented_builtins(void **state)
{
    char seeded[CBUFSIZE];
    char value[CBUFSIZE];
    const char *always_nonempty[] = {
        "DATE",           "TIME",    "DATETIME",   "LDATE",      "LTIME",
        "CTIME",          "UTIME",   "DATE_YYYY",  "DATE_YY",    "DATE_19XX_YY",
        "DATE_19XX_YYYY", "DATE_MM", "DATE_MMM",   "DATE_MONTH", "DATE_DD",
        "DATE_D",         "DATE_WW", "DATE_WYYYY", "DATE_JJJ",   "TIME_HH",
        "TIME_MM",        "TIME_SS", "TIME_MSEC",  "STATUS",     "TSTATUS",
    };
    size_t i;

    (void)state;

    expand_command("seed", seeded, sizeof(seeded));
    sim_last_cmd_stat = SCPE_ARG;

    for (i = 0; i < sizeof(always_nonempty) / sizeof(always_nonempty[0]); ++i) {
        assert_non_null(
            _sim_get_env_special(always_nonempty[i], value, sizeof(value)));
        assert_true(value[0] != '\0');
    }
}

struct simh_test_cmdvars_time_case {
    const char *input;
    const char *expected;
};

struct simh_test_cmdvars_iso_case {
    time_t epoch;
    const char *expected_week;
    const char *expected_year;
};

/* Expand one command under a fixed clock and compare with the expectation. */
static void run_fixed_time_expansion_case(void *ctx)
{
    const struct simh_test_cmdvars_time_case *test_case = ctx;
    char expanded[CBUFSIZE];

    expand_command(test_case->input, expanded, sizeof(expanded));
    assert_string_equal(expanded, test_case->expected);
}

/* Verify date and time variables honor the injected realtime clock. */
static void test_sim_sub_args_expands_fixed_datetime_values(void **state)
{
    const struct simh_test_cmdvars_time_case cases[] = {
        {"A%DATE%B", "A2021-01-01B"},
        {"A%TIME%B", "A12:34:56B"},
        {"A%DATETIME%B", "A2021-01-01T12:34:56B"},
        {"A%UTIME%B", "A1609504496B"},
        {"A%TIME_MSEC%B", "A789B"},
        {"A%DATE_D%B", "A5B"},
        {"A%DATE_JJJ%B", "A001B"},
    };
    size_t i;

    (void)state;

    simh_test_cmdvars_time =
        (struct timespec){.tv_sec = 1609504496, .tv_nsec = 789000000};
    sim_time_set_test_hooks(simh_test_cmdvars_clock_gettime, NULL);

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        with_timezone("UTC", run_fixed_time_expansion_case, (void *)&cases[i]);
}

/* Verify DATE_19XX_* covers leap-year branches in its calendar mapping. */
static void test_sim_sub_args_expands_date_19xx_leap_years(void **state)
{
    struct simh_test_cmdvars_time_case case_2000_yy = {"A%DATE_19XX_YY%B",
                                                       "A84B"};
    struct simh_test_cmdvars_time_case case_2000_yyyy = {"A%DATE_19XX_YYYY%B",
                                                         "A1984B"};
    struct simh_test_cmdvars_time_case case_2004_yy = {"A%DATE_19XX_YY%B",
                                                       "A76B"};
    struct simh_test_cmdvars_time_case case_2004_yyyy = {"A%DATE_19XX_YYYY%B",
                                                         "A1976B"};
    struct simh_test_cmdvars_time_case case_2100_yy = {"A%DATE_19XX_YY%B",
                                                       "A99B"};
    struct simh_test_cmdvars_time_case case_2100_yyyy = {"A%DATE_19XX_YYYY%B",
                                                         "A1999B"};

    (void)state;

    simh_test_cmdvars_time =
        (struct timespec){.tv_sec = 946684800, .tv_nsec = 0};
    sim_time_set_test_hooks(simh_test_cmdvars_clock_gettime, NULL);
    with_timezone("UTC", run_fixed_time_expansion_case, &case_2000_yy);
    with_timezone("UTC", run_fixed_time_expansion_case, &case_2000_yyyy);

    simh_test_cmdvars_time =
        (struct timespec){.tv_sec = 1072915200, .tv_nsec = 0};
    sim_time_set_test_hooks(simh_test_cmdvars_clock_gettime, NULL);
    with_timezone("UTC", run_fixed_time_expansion_case, &case_2004_yy);
    with_timezone("UTC", run_fixed_time_expansion_case, &case_2004_yyyy);

    simh_test_cmdvars_time =
        (struct timespec){.tv_sec = 4102444800, .tv_nsec = 0};
    sim_time_set_test_hooks(simh_test_cmdvars_clock_gettime, NULL);
    with_timezone("UTC", run_fixed_time_expansion_case, &case_2100_yy);
    with_timezone("UTC", run_fixed_time_expansion_case, &case_2100_yyyy);
}

/* Expand ISO week variables for one fixed epoch and compare the results. */
static void run_fixed_iso_week_case(void *ctx)
{
    const struct simh_test_cmdvars_iso_case *test_case = ctx;
    char expanded[CBUFSIZE];

    simh_test_cmdvars_time =
        (struct timespec){.tv_sec = test_case->epoch, .tv_nsec = 0};
    sim_time_set_test_hooks(simh_test_cmdvars_clock_gettime, NULL);

    expand_command("A%DATE_WW%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, test_case->expected_week);
    expand_command("A%DATE_WYYYY%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, test_case->expected_year);
}

/* Verify ISO week and year logic uses the injected realtime clock. */
static void test_sim_sub_args_expands_iso_week_boundary(void **state)
{
    const struct simh_test_cmdvars_iso_case cases[] = {
        {1451606400, "A53B", "A2015B"}, /* 2016-01-01 */
        {1483228800, "A52B", "A2016B"}, /* 2017-01-01 */
        {1514678400, "A52B", "A2017B"}, /* 2017-12-31 */
        {1514764800, "A01B", "A2018B"}, /* 2018-01-01 */
        {1577664000, "A01B", "A2020B"}, /* 2019-12-30 */
        {1609113600, "A53B", "A2020B"}, /* 2020-12-28 */
        {1609504496, "A53B", "A2020B"}, /* 2021-01-01 */
        {1640908800, "A52B", "A2021B"}, /* 2021-12-31 */
        {1641081600, "A52B", "A2021B"}, /* 2022-01-02 */
    };
    size_t i;

    (void)state;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        with_timezone("UTC", run_fixed_iso_week_case, (void *)&cases[i]);
}

/* Verify mode-reporting variables reflect current SCP state. */
static void test_sim_get_env_special_expands_mode_variables(void **state)
{
    char seeded[CBUFSIZE];
    char value[CBUFSIZE];

    (void)state;

    expand_command("seed", seeded, sizeof(seeded));

    sim_quiet = 1;
    sim_show_message = 0;

    assert_non_null(_sim_get_env_special("SIM_VERIFY", value, sizeof(value)));
    assert_non_null(_sim_get_env_special("SIM_VERBOSE", value, sizeof(value)));

    assert_non_null(_sim_get_env_special("SIM_QUIET", value, sizeof(value)));
    assert_string_equal(value, "-Q");

    assert_non_null(_sim_get_env_special("SIM_MESSAGE", value, sizeof(value)));
    assert_string_equal(value, "-Q");

    sim_quiet = 0;
    sim_show_message = 1;

    assert_non_null(_sim_get_env_special("SIM_QUIET", value, sizeof(value)));
    assert_string_equal(value, "");

    assert_non_null(_sim_get_env_special("SIM_MESSAGE", value, sizeof(value)));
    assert_string_equal(value, "");

    assert_int_equal(set_verify(1, NULL), SCPE_OK);
    assert_string_equal(
        _sim_get_env_special("SIM_VERIFY", value, sizeof(value)), "-V");
    assert_string_equal(
        _sim_get_env_special("SIM_VERBOSE", value, sizeof(value)), "-V");
}

/* Verify SIM_BIN_NAME handles plain and missing program-name values. */
static void
test_sim_get_env_special_handles_sim_bin_name_edge_cases(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_prog_name = "plain-name";
    assert_string_equal(
        _sim_get_env_special("SIM_BIN_NAME", value, sizeof(value)),
        "plain-name");

    sim_prog_name = "";
    assert_null(_sim_get_env_special("SIM_BIN_NAME", value, sizeof(value)));

    sim_prog_name = NULL;
    assert_null(_sim_get_env_special("SIM_BIN_NAME", value, sizeof(value)));
    assert_null(_sim_get_env_special("SIM_BIN_PATH", value, sizeof(value)));
}

/* Verify SIM_BIN_NAME strips Windows-style path prefixes too. */
static void test_sim_get_env_special_handles_windows_sim_bin_name(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_prog_name = "C:\\test\\bin\\zimh-unit.exe";
    assert_string_equal(
        _sim_get_env_special("SIM_BIN_NAME", value, sizeof(value)),
        "zimh-unit.exe");
}

/* Verify substring extraction and replacement modifiers still work. */
static void test_sim_sub_args_expands_substrings_and_replacements(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_TEST_EXTERNAL", "abcdef", 1), 0);

    expand_command("A%ZIMH_TEST_EXTERNAL:~1,3%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AbcdB");

    expand_command("A%ZIMH_TEST_EXTERNAL:cd=XY%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AabXYefB");
}

/* Verify an empty substitution fixup leaves the source value unchanged. */
static void test_sim_sub_args_preserves_value_for_empty_fixup(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_TEST_EXTERNAL", "abcdef", 1), 0);

    expand_command("A%ZIMH_TEST_EXTERNAL:%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AabcdefB");
}

/* Verify less common substring modifiers still behave as documented. */
static void
test_sim_sub_args_expands_negative_and_global_substring_modifiers(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_TEST_EXTERNAL", "abcdef", 1), 0);

    expand_command("A%ZIMH_TEST_EXTERNAL:~-2,2%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AefB");

    expand_command("A%ZIMH_TEST_EXTERNAL:~-2%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AefB");

    expand_command("A%ZIMH_TEST_EXTERNAL:~1,-1%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AbcdefB");

    expand_command("A%ZIMH_TEST_EXTERNAL:~%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AabcdefB");

    expand_command("A%ZIMH_TEST_EXTERNAL:*cd=XY%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AXYefB");
}

/* Verify substring modifiers clamp overlong ranges cleanly. */
static void test_sim_sub_args_clamps_overlong_substrings(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_TEST_EXTERNAL", "abcdef", 1), 0);

    expand_command("A%ZIMH_TEST_EXTERNAL:~2,99%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AcdefB");
}

/* Verify captured host aliases remain visible to SCP substitution. */
static void test_sim_get_env_special_expands_captured_aliases(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_ALIAS", "from-host", 1), 0);
    sim_cmdvars_capture_env_alias("ZIMH_ALIAS");

    assert_string_equal(
        _sim_get_env_special("ZIMH_ALIAS", value, sizeof(value)), "from-host");
}

/* Verify missing aliases are ignored instead of creating empty entries. */
static void
test_sim_cmdvars_capture_env_alias_ignores_missing_environment(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_cmdvars_capture_env_alias("ZIMH_ALIAS");
    assert_null(_sim_get_env_special("ZIMH_ALIAS", value, sizeof(value)));
}

/* Verify missing lookups skip over unrelated captured aliases cleanly. */
static void
test_sim_get_env_special_ignores_unmatched_captured_aliases(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_ALIAS", "from-host", 1), 0);
    sim_cmdvars_capture_env_alias("ZIMH_ALIAS");

    assert_null(
        _sim_get_env_special("ZIMH_MISSING_ALIAS", value, sizeof(value)));
}

/* Verify unsetting a missing SCP-owned variable leaves others untouched. */
static void test_sim_sub_var_unset_ignores_missing_name(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_sub_var_set("ZIMH_TEST_VAR", "value");
    sim_sub_var_unset("ZIMH_MISSING_VAR");

    assert_string_equal(
        _sim_get_env_special("ZIMH_TEST_VAR", value, sizeof(value)), "value");
}

/* Verify SCP does not publish SEND defaults unless it has a real value. */
static void test_sim_sub_args_omits_missing_send_default(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    expand_command("A%SIM_SEND_DELAY_CONSOLE%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");

    expand_command("A%sim_send_delay_console%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");

    expand_command("SIM_SEND_DELAY_CONSOLE tail", expanded, sizeof(expanded));
    assert_string_equal(expanded, "SIM_SEND_DELAY_CONSOLE tail");
}

/* Verify FILE COMPARE scratch state is absent until a compare sets it. */
static void test_sim_sub_args_omits_missing_file_compare_offset(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    expand_command("A%_FILE_COMPARE_DIFF_OFFSET%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");

    expand_command("_FILE_COMPARE_DIFF_OFFSET tail", expanded,
                   sizeof(expanded));
    assert_string_equal(expanded, "_FILE_COMPARE_DIFF_OFFSET tail");
}

/* Verify FILE COMPARE publishes the first differing offset via substitution. */
static void test_file_compare_sets_diff_offset_without_environment(void **state)
{
    char path1[CBUFSIZE];
    char path2[CBUFSIZE];
    char quoted1[CBUFSIZE];
    char quoted2[CBUFSIZE];
    char expanded[CBUFSIZE];
    int saved_switches;

    (void)state;

    write_temp_file(path1, sizeof(path1), "zimh-filecmp-1-", NULL, "abXd");
    write_temp_file(path2, sizeof(path2), "zimh-filecmp-2-", NULL, "abYd");

    snprintf(quoted1, sizeof(quoted1), "\"%s\"", path1);
    snprintf(quoted2, sizeof(quoted2), "\"%s\"", path2);

    saved_switches = sim_switches;
    sim_switches |= SWMASK('F');
    assert_int_not_equal(sim_cmp_string(quoted1, quoted2), 0);
    sim_switches = saved_switches;

    expand_command("A%_FILE_COMPARE_DIFF_OFFSET%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "A2B");

    assert_int_equal(simh_test_remove_path(path1), 0);
    assert_int_equal(simh_test_remove_path(path2), 0);
}

/* Verify RUNLIMIT is still available through substitution. */
static void test_runlimit_cmd_expands_without_environment(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    assert_int_equal(run_scp_command("RUNLIMIT", "5"), SCPE_OK);

    expand_command("A%SIM_RUNLIMIT%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "A5B");

    expand_command("A%SIM_RUNLIMIT_UNITS%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AinstructionsB");

    assert_int_equal(run_scp_command("NORUNLIMIT", ""), SCPE_OK);
    expand_command("A%SIM_RUNLIMIT%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
}

/* Verify UTIME remains available for scripts that depend on it. */
static void test_sim_sub_args_expands_utime(void **state)
{
    char expanded[CBUFSIZE];
    size_t i;

    (void)state;

    expand_command("A%UTIME%B", expanded, sizeof(expanded));
    assert_true(strlen(expanded) > 2);
    assert_true(expanded[0] == 'A');
    assert_true(expanded[strlen(expanded) - 1] == 'B');

    for (i = 1; i + 1 < strlen(expanded); ++i)
        assert_true(expanded[i] >= '0' && expanded[i] <= '9');
}

/* Verify documented simulator identity variables come from SCP state. */
static void test_sim_sub_args_expands_sim_identity_variables(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("SIM_NAME", "host-name", 1), 0);
    assert_int_equal(setenv("SIM_BIN_NAME", "host-bin", 1), 0);
    assert_int_equal(setenv("SIM_BIN_PATH", "/host/bin", 1), 0);

    expand_command("A%SIM_NAME%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "Azimh-unitB");

    expand_command("A%SIM_BIN_NAME%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "Azimh-unit-scp-cmdvarsB");

    expand_command("A%SIM_BIN_PATH%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "A/test/bin/zimh-unit-scp-cmdvarsB");

    expand_command("SIM_NAME tail", expanded, sizeof(expanded));
    assert_string_equal(expanded, "zimh-unit tail");
}

/* Verify portable host-path variables do not come from same-named env vars. */
static void test_sim_sub_args_expands_portable_path_variables(void **state)
{
    char expanded[CBUFSIZE];
    char value[CBUFSIZE];
    const char *temp_env_name;
    const char *temp_env_value;
    const char *expected_tmpdir;
    struct simh_test_saved_env saved_temp;

    (void)state;

#if defined(_WIN32)
    temp_env_name = "TMP";
    temp_env_value = "C:\\portable\\tmp";
    expected_tmpdir = "C:\\portable\\tmp\\";
    sim_host_path_set_test_win32_temp_hooks(simh_test_win32_temp_path_success,
                                            simh_test_win32_temp_path_success);
#else
    temp_env_name = "TMPDIR";
    temp_env_value = "/portable/tmp";
    expected_tmpdir = "/portable/tmp";
#endif
    saved_temp = save_env(temp_env_name);

    assert_int_equal(setenv("SIM_NULL_DEVICE", "host-null", 1), 0);
    assert_int_equal(setenv("SIM_TMPDIR", "host-tmpdir", 1), 0);
    assert_int_equal(setenv(temp_env_name, temp_env_value, 1), 0);

    expand_command("A%SIM_NULL_DEVICE%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "A" NULL_DEVICE "B");
    assert_string_equal(
        _sim_get_env_special("SIM_NULL_DEVICE", value, sizeof(value)),
        NULL_DEVICE);

    expand_command("A%SIM_TMPDIR%B", expanded, sizeof(expanded));
    snprintf(value, sizeof(value), "A%sB", expected_tmpdir);
    assert_string_equal(expanded, value);
    assert_string_equal(
        _sim_get_env_special("SIM_TMPDIR", value, sizeof(value)),
        expected_tmpdir);

    restore_env(&saved_temp);
}

/* Verify DO-file expansion can attach an output unit to the null device. */
static void test_do_file_attaches_output_unit_to_null_device(void **state)
{
    char temp_dir[1024];
    char script_path[1024];
    char nul_path[1024];
    char saved_cwd[1024];
    UNIT unit;
    DEVICE device;
    DEVICE *devices[2];
    t_stat status;

    (void)state;

    assert_non_null(getcwd(saved_cwd, sizeof(saved_cwd)));
    assert_int_equal(
        simh_test_make_temp_dir(temp_dir, sizeof(temp_dir), "scp-cmdvars"), 0);
    assert_int_equal(simh_test_join_path(script_path, sizeof(script_path),
                                         temp_dir, "attach-null.sim"),
                     0);
    assert_int_equal(
        simh_test_join_path(nul_path, sizeof(nul_path), temp_dir, "nul"), 0);
    assert_int_equal(
        simh_test_write_file(script_path, "attach OUT %SIM_NULL_DEVICE%\n",
                             strlen("attach OUT %SIM_NULL_DEVICE%\n")),
        0);

    simh_test_init_device_unit(&device, &unit, "OUT", "OUT0", 0,
                               UNIT_ATTABLE | UNIT_SEQ, 8, 1);
    devices[0] = &device;
    devices[1] = NULL;
    assert_int_equal(simh_test_set_devices(devices), 0);

    assert_int_equal(chdir(temp_dir), 0);
    status = do_cmd(1, script_path);
    assert_int_equal(chdir(saved_cwd), 0);
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_OK);

    assert_true((unit.flags & UNIT_ATT) != 0);
    assert_string_equal(unit.filename, NULL_DEVICE);
#if !defined(_WIN32)
    assert_int_equal(access(nul_path, F_OK), -1);
#endif

    assert_int_equal(detach_unit(&unit), SCPE_OK);
    assert_int_equal(simh_test_remove_path(temp_dir), 0);
}

/* Verify SIM_OSTYPE still expands while removed metadata stays absent. */
static void test_show_version_keeps_only_sim_ostype(void **state)
{
    char expanded[CBUFSIZE];
    FILE *stream;

    (void)state;

    stream = sim_tmpfile();
    assert_non_null(stream);

    assert_int_equal(show_version(stream, NULL, NULL, 0, NULL), SCPE_OK);
    fclose(stream);

    expand_command("A%SIM_OSTYPE%B", expanded, sizeof(expanded));
    assert_true(strlen(expanded) > 2);
    assert_true(expanded[0] == 'A');
    assert_true(expanded[strlen(expanded) - 1] == 'B');

    expand_command("A%DATE_YC%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_DELTA%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_VERSION_MODE%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_MAJOR%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_MINOR%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_PATCH%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_VM_RELEASE%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_GIT_COMMIT_ID%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_GIT_COMMIT_TIME%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_ARCHIVE_GIT_COMMIT_ID%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "AB");
    expand_command("A%SIM_ARCHIVE_GIT_COMMIT_TIME%B", expanded,
                   sizeof(expanded));
    assert_string_equal(expanded, "AB");
}

/* Verify show_version renders the public ZIMH version banner contract. */
static void test_show_version_prints_zimh_banner(void **state)
{
    char actual[CBUFSIZE];
    char expected[CBUFSIZE];
    FILE *stream;

    (void)state;

    stream = sim_tmpfile();
    assert_non_null(stream);

    assert_int_equal(show_version(stream, NULL, NULL, 1, NULL), SCPE_OK);
    rewind(stream);
    assert_non_null(fgets(actual, sizeof(actual), stream));
    fclose(stream);

    snprintf(expected, sizeof(expected), "%s simulator ZIMH %s\n", sim_name,
             ZIMH_VERSION);
    assert_string_equal(actual, expected);
}

/* Verify show_version reports deterministic build details. */
static void test_show_version_omits_compile_timestamp(void **state)
{
    char output[4096];
    size_t bytes;
    FILE *stream;

    (void)state;

    stream = sim_tmpfile();
    assert_non_null(stream);

    assert_int_equal(show_version(stream, NULL, NULL, 1, NULL), SCPE_OK);
    rewind(stream);
    bytes = fread(output, 1, sizeof(output) - 1, stream);
    fclose(stream);
    output[bytes] = '\0';

    assert_non_null(strstr(output, "Simulator Build:"));
    assert_null(strstr(output, "Simulator Compiled"));
}

#if !defined(_WIN32)
/* Verify SIM_OSTYPE cleanly reports no value when uname probing fails. */
static void
test_sim_get_env_special_ostype_handles_total_probe_failure(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_cmdvars_set_test_uname_hook(simh_test_uname_hook_fail);
    assert_null(_sim_get_env_special("SIM_OSTYPE", value, sizeof(value)));
    assert_int_equal(simh_test_uname_probe_calls, 1);
}

/* Verify SIM_OSTYPE caches the first discovered value. */
static void test_sim_get_env_special_ostype_uses_cached_value(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_cmdvars_set_test_uname_hook(simh_test_uname_hook_cached);
    assert_string_equal(
        _sim_get_env_special("SIM_OSTYPE", value, sizeof(value)), "ProbeOS");
    assert_int_equal(simh_test_uname_probe_calls, 1);

    sim_cmdvars_set_test_uname_hook(simh_test_uname_hook_fail);
    assert_string_equal(
        _sim_get_env_special("SIM_OSTYPE", value, sizeof(value)), "ProbeOS");
    assert_int_equal(simh_test_uname_probe_calls, 1);
}
#else
/* Verify SIM_OSTYPE reports the fixed Windows value. */
static void test_sim_get_env_special_ostype_reports_windows(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    assert_string_equal(
        _sim_get_env_special("SIM_OSTYPE", value, sizeof(value)), "Windows");
}
#endif

/* Verify time lookup degrades cleanly if localtime breakdown fails. */
static void test_sim_get_env_special_handles_localtime_failure(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_cmdvars_set_test_localtime_hook(simh_test_localtime_hook_fail);

    assert_null(_sim_get_env_special("DATE", value, sizeof(value)));
    assert_string_equal(_sim_get_env_special("SIM_NAME", value, sizeof(value)),
                        sim_name);
    assert_string_equal(_sim_get_env_special("UTIME", value, sizeof(value)),
                        "0");
    assert_int_equal(simh_test_localtime_calls, 3);
}

/* Verify TSTATUS prefers simulator stop-message text when present. */
static void test_sim_get_env_special_tstatus_prefers_stop_message(void **state)
{
    char value[CBUFSIZE];
    const char *saved_message;

    (void)state;

    saved_message = sim_stop_messages[1];
    sim_stop_messages[1] = "test stop message";

    sim_last_cmd_stat = 1;
    assert_string_equal(_sim_get_env_special("TSTATUS", value, sizeof(value)),
                        "test stop message");

    sim_stop_messages[1] = saved_message;
}

/* Verify TSTATUS falls back to generic error text when needed. */
static void
test_sim_get_env_special_tstatus_falls_back_to_error_text(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_last_cmd_stat = SCPE_UNATT;
    assert_string_equal(_sim_get_env_special("TSTATUS", value, sizeof(value)),
                        sim_error_text(SCPE_UNATT));
}

/* Verify TSTATUS falls back cleanly for the plain OK status too. */
static void test_sim_get_env_special_tstatus_handles_ok_status(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_last_cmd_stat = SCPE_OK;
    assert_string_equal(_sim_get_env_special("TSTATUS", value, sizeof(value)),
                        sim_error_text(SCPE_OK));
}

/* Verify TSTATUS falls back when an in-range stop message is absent. */
static void
test_sim_get_env_special_tstatus_handles_missing_stop_message(void **state)
{
    char value[CBUFSIZE];
    const char *saved_message;

    (void)state;

    saved_message = sim_stop_messages[2];
    sim_stop_messages[2] = NULL;

    sim_last_cmd_stat = 2;
    assert_string_equal(_sim_get_env_special("TSTATUS", value, sizeof(value)),
                        sim_error_text(2));

    sim_stop_messages[2] = saved_message;
}

/* Verify runlimit variables disappear cleanly in their edge cases. */
static void test_sim_get_env_special_handles_runlimit_edge_cases(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    sim_runlimit_enabled = true;
    sim_runlimit_value = 12;
    sim_runlimit_units = NULL;

    assert_string_equal(
        _sim_get_env_special("SIM_RUNLIMIT", value, sizeof(value)), "12");
    assert_null(
        _sim_get_env_special("SIM_RUNLIMIT_UNITS", value, sizeof(value)));

    sim_runlimit_enabled = false;
    assert_null(
        _sim_get_env_special("SIM_RUNLIMIT_UNITS", value, sizeof(value)));
}

/* Verify substitution fixups cope with a truncated caller buffer. */
static void
test_sim_get_env_special_handles_exhausted_fixup_buffer(void **state)
{
    char value[1];
    char truncated[6];

    (void)state;

    assert_int_equal(setenv("ZIMH_TEST_EXTERNAL", "abcdef", 1), 0);
    assert_string_equal(
        _sim_get_env_special("ZIMH_TEST_EXTERNAL:cd=1234567890123456789",
                             truncated, sizeof(truncated)),
        "ab123");
    assert_non_null(_sim_get_env_special(
        "ZIMH_TEST_EXTERNAL:cd=1234567890123456789", value, sizeof(value)));
    assert_string_equal(value, "");
}

/* Verify SET ENVIRONMENT stores a literal value. */
static void test_sim_set_environment_literal_value(void **state)
{
    (void)state;

    sim_switches = 0;
    assert_int_equal(sim_set_environment(0, "ZIMH_TEST_SET=hello"), SCPE_OK);
    assert_string_equal(getenv("ZIMH_TEST_SET"), "hello");
}

/* Verify SET ENVIRONMENT rejects a NULL input string. */
static void test_sim_set_environment_rejects_null_input(void **state)
{
    (void)state;

    sim_switches = 0;
    assert_int_equal(sim_set_environment(0, NULL), SCPE_2FARG);
}

/* Verify SET ENVIRONMENT /S decodes a quoted string value. */
static void test_sim_set_environment_decodes_quoted_string(void **state)
{
    (void)state;

    sim_switches = SWMASK('S');
    assert_int_equal(sim_set_environment(0, "ZIMH_TEST_SET=\"a\\tb\""),
                     SCPE_OK);
    assert_memory_equal(getenv("ZIMH_TEST_SET"), "a\tb", 3);
}

/* Verify SET ENVIRONMENT /A evaluates arithmetic expressions. */
static void test_sim_set_environment_evaluates_expression(void **state)
{
    (void)state;

    sim_switches = SWMASK('A');
    assert_int_equal(sim_set_environment(0, "ZIMH_TEST_SET=1+2"), SCPE_OK);
    assert_string_equal(getenv("ZIMH_TEST_SET"), "3");
}

/* Verify SET ENVIRONMENT /P uses the default value in non-interactive mode. */
static void test_sim_set_environment_prompt_uses_default(void **state)
{
    (void)state;

    sim_switches = SWMASK('P');
    sim_rem_cmd_active_line = 0;
    assert_int_equal(sim_set_environment(0, "\"Prompt\" ZIMH_TEST_SET=default"),
                     SCPE_OK);
    assert_string_equal(getenv("ZIMH_TEST_SET"), "default");
}

/* Verify /P strips a single-quoted prompt before use. */
static void
test_sim_set_environment_prompt_uses_single_quoted_prompt(void **state)
{
    (void)state;

    sim_switches = SWMASK('P');
    sim_rem_cmd_active_line = 0;
    assert_int_equal(sim_set_environment(0, "'Prompt' ZIMH_TEST_SET=default"),
                     SCPE_OK);
    assert_string_equal(getenv("ZIMH_TEST_SET"), "default");
}

/* Verify /P also accepts an unquoted prompt string. */
static void test_sim_set_environment_prompt_uses_unquoted_prompt(void **state)
{
    (void)state;

    sim_switches = SWMASK('P');
    sim_rem_cmd_active_line = 0;
    assert_int_equal(sim_set_environment(0, "Prompt ZIMH_TEST_SET=default"),
                     SCPE_OK);
    assert_string_equal(getenv("ZIMH_TEST_SET"), "default");
}

/* Verify captured host aliases are restored only while spawning a child. */
static void test_sim_cmdvars_system_restores_captured_alias(void **state)
{
    char path[CBUFSIZE];
    char command[CBUFSIZE];
    char contents[64];

    (void)state;

    write_empty_temp_file(path, sizeof(path), "zimh-cmdvars-system-", NULL);

    assert_int_equal(setenv("ZIMH_ALIAS", "from-host", 1), 0);
    sim_cmdvars_capture_env_alias("ZIMH_ALIAS");
    assert_null(getenv("ZIMH_ALIAS"));

    make_env_capture_command(command, sizeof(command), "ZIMH_ALIAS", path);
    assert_int_equal(sim_cmdvars_system(command), 0);

    read_text_file(path, contents, sizeof(contents));
    assert_string_equal(contents, "from-host");
    assert_null(getenv("ZIMH_ALIAS"));

    assert_int_equal(simh_test_remove_path(path), 0);
}

/* Verify sim_cmdvars_system also works when there are no captured aliases. */
static void test_sim_cmdvars_system_without_captured_aliases(void **state)
{
    (void)state;

    assert_int_equal(sim_cmdvars_system(successful_system_command()), 0);
}

/* Verify SET ENVIRONMENT replaces a captured alias with the new real value. */
static void test_sim_set_environment_replaces_captured_alias(void **state)
{
    char path[CBUFSIZE];
    char command[CBUFSIZE];
    char contents[64];

    (void)state;

    write_empty_temp_file(path, sizeof(path), "zimh-cmdvars-alias-", NULL);

    assert_int_equal(setenv("ZIMH_ALIAS", "from-host", 1), 0);
    sim_cmdvars_capture_env_alias("ZIMH_ALIAS");
    assert_null(getenv("ZIMH_ALIAS"));

    sim_switches = 0;
    assert_int_equal(sim_set_environment(0, "ZIMH_ALIAS=from-set"), SCPE_OK);
    assert_string_equal(getenv("ZIMH_ALIAS"), "from-set");

    make_env_capture_command(command, sizeof(command), "ZIMH_ALIAS", path);
    assert_int_equal(sim_cmdvars_system(command), 0);

    read_text_file(path, contents, sizeof(contents));
    assert_string_equal(contents, "from-set");

    assert_int_equal(simh_test_remove_path(path), 0);
}

/* Verify SET ENVIRONMENT leaves unrelated captured aliases alone. */
static void
test_sim_set_environment_keeps_unmatched_captured_alias(void **state)
{
    (void)state;

    assert_int_equal(setenv("ZIMH_ALIAS", "from-host", 1), 0);
    sim_cmdvars_capture_env_alias("ZIMH_ALIAS");
    assert_null(getenv("ZIMH_ALIAS"));

    sim_switches = 0;
    assert_int_equal(sim_set_environment(0, "ZIMH_TEST_SET=hello"), SCPE_OK);
    assert_null(getenv("ZIMH_ALIAS"));
    assert_string_equal(getenv("ZIMH_TEST_SET"), "hello");
}

struct prompt_test_context {
    const char *command;
    const char *expected_value;
};

/* Exercise one interactive /P read against redirected stdin. */
static void run_prompt_environment_read(void *ctx)
{
    struct prompt_test_context *prompt_ctx = ctx;

    sim_switches = SWMASK('P');
    sim_rem_cmd_active_line = -1;
    assert_int_equal(sim_set_environment(0, prompt_ctx->command), SCPE_OK);
    assert_string_equal(getenv("ZIMH_TEST_SET"), prompt_ctx->expected_value);
}

/* Verify /P reads from stdin in interactive mode. */
static void test_sim_set_environment_prompt_reads_stdin(void **state)
{
    struct prompt_test_context prompt_ctx = {"\"Prompt\" ZIMH_TEST_SET=default",
                                             "typed"};

    (void)state;

    with_redirected_stdin("typed\n", run_prompt_environment_read, &prompt_ctx);
}

/* Verify /P falls back to the default when interactive input is empty. */
static void
test_sim_set_environment_prompt_uses_default_after_empty_input(void **state)
{
    struct prompt_test_context prompt_ctx = {"\"Prompt\" ZIMH_TEST_SET=default",
                                             "default"};

    (void)state;

    with_redirected_stdin("\n", run_prompt_environment_read, &prompt_ctx);
}

/* Verify /P falls back to the default when stdin immediately hits EOF. */
static void test_sim_set_environment_prompt_uses_default_after_eof(void **state)
{
    struct prompt_test_context prompt_ctx = {"\"Prompt\" ZIMH_TEST_SET=default",
                                             "default"};

    (void)state;

    with_redirected_stdin("", run_prompt_environment_read, &prompt_ctx);
}

/* Verify % expansion still handles escapes, arguments, and filepath parts. */
static void test_sim_sub_args_handles_escape_and_do_arguments(void **state)
{
    char path[CBUFSIZE];
    char expanded[CBUFSIZE];
    char expected[CBUFSIZE];
    char *do_arg[10] = {0};
    char *path_only;
    char *name_only;
    char *ext_only;
    char *name_ext;

    (void)state;

    write_empty_temp_file(path, sizeof(path), "zimh-cmdvars-path-", ".txt");

    do_arg[0] = "script";
    do_arg[1] = path;

    expand_command("A%%B", expanded, sizeof(expanded));
    assert_string_equal(expanded, "A%B");

    expand_command("trail %", expanded, sizeof(expanded));
    assert_string_equal(expanded, "trail %");

    expand_command_with_args("%1 tail", expanded, sizeof(expanded), do_arg);
    snprintf(expected, sizeof(expected), "%s tail", path);
    assert_string_equal(expanded, expected);

    path_only = sim_filepath_parts(path, "p");
    name_only = sim_filepath_parts(path, "n");
    ext_only = sim_filepath_parts(path, "x");
    name_ext = sim_filepath_parts(path, "nx");

    expand_command_with_args("%~p1", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, path_only);

    expand_command_with_args("%~n1", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, name_only);

    expand_command_with_args("%~x1", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, ext_only);

    expand_command_with_args("%~nx1", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, name_ext);

    free(path_only);
    free(name_only);
    free(ext_only);
    free(name_ext);
    assert_int_equal(simh_test_remove_path(path), 0);
}

/* Verify missing DO arguments and %* expansion use the remaining args. */
static void
test_sim_sub_args_handles_missing_and_star_do_arguments(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    do_arg[0] = "script";
    do_arg[1] = "alpha";
    do_arg[2] = "beta";
    do_arg[4] = "late";

    expand_command_with_args("A%3B", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "AB");

    expand_command_with_args("A%*B", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "Aalpha betaB");
}

/* Verify clock failure falls back to a zeroed command timestamp. */
static void test_sim_sub_args_uses_zero_time_after_clock_failure(void **state)
{
    (void)state;

    simh_test_cmdvars_time = (struct timespec){.tv_sec = 123, .tv_nsec = 456};
    simh_test_cmdvars_time_status = -1;
    sim_time_set_test_hooks(simh_test_cmdvars_clock_gettime, NULL);

    with_timezone(
        "UTC", run_fixed_time_expansion_case,
        &(struct simh_test_cmdvars_time_case){
            "A%DATE% %UTIME% %TIME_MSEC% %DATE_D%B", "A1970-01-01 0 000 4B"});
}

/* Verify malformed overlong %~ parts strings fail closed cleanly. */
static void test_sim_sub_args_ignores_overlong_filepath_parts_spec(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    do_arg[0] = "script";
    do_arg[1] = "/tmp/example.bin";

    expand_command_with_args("A%~fpnxtzfpnxtzfpnxtzfpnxtzfpnxtzfpnxtz1B",
                             expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "AB");
}

/* Verify DATE_D maps Sunday to 7 rather than 0. */
static void test_sim_sub_args_expands_date_d_for_sunday(void **state)
{
    struct simh_test_cmdvars_time_case sunday_case = {"A%DATE_D%B", "A7B"};

    (void)state;

    simh_test_cmdvars_time =
        (struct timespec){.tv_sec = 1609632000, .tv_nsec = 0};
    sim_time_set_test_hooks(simh_test_cmdvars_clock_gettime, NULL);
    with_timezone("UTC", run_fixed_time_expansion_case, &sunday_case);
}

/* Verify %n disappears if an earlier DO argument is omitted. */
static void test_sim_sub_args_omits_sparse_do_arguments(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    do_arg[0] = "script";
    do_arg[2] = "late";

    expand_command_with_args("A%2B", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "AB");
}

/* Verify %* quotes spaced arguments and switches quote style if needed. */
static void test_sim_sub_args_star_quotes_spaced_arguments(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    do_arg[0] = "script";
    do_arg[1] = "two words";
    do_arg[2] = "he\"llo world";

    expand_command_with_args("A%*B", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "A\"two words\" 'he\"llo world'B");
}

/* Verify %* can consume all nine available DO arguments without early stop. */
static void test_sim_sub_args_star_handles_all_do_arguments(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    do_arg[0] = "script";
    do_arg[1] = "a1";
    do_arg[2] = "a2";
    do_arg[3] = "a3";
    do_arg[4] = "a4";
    do_arg[5] = "a5";
    do_arg[6] = "a6";
    do_arg[7] = "a7";
    do_arg[8] = "a8";
    do_arg[9] = "a9";

    expand_command_with_args("A%*B", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "Aa1 a2 a3 a4 a5 a6 a7 a8 a9B");
}

/* Verify %* with no remaining arguments expands to an empty string. */
static void test_sim_sub_args_star_handles_no_remaining_arguments(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    do_arg[0] = "script";

    expand_command_with_args("A%*B", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "AB");
}

/* Verify %* grows past its initial scratch size for long arguments. */
static void test_sim_sub_args_star_grows_for_long_arguments(void **state)
{
    char expanded[CBUFSIZE];
    char expected[CBUFSIZE];
    char long_arg[96];
    char *do_arg[10] = {0};

    (void)state;

    memset(long_arg, 'x', sizeof(long_arg) - 1);
    long_arg[sizeof(long_arg) - 1] = '\0';
    do_arg[0] = "script";
    do_arg[1] = long_arg;

    expand_command_with_args("A%*B", expanded, sizeof(expanded), do_arg);
    snprintf(expected, sizeof(expected), "A%sB", long_arg);
    assert_string_equal(expanded, expected);
}

/* Verify %* truncates only at the final command-buffer boundary. */
static void test_sim_sub_args_star_respects_final_output_limit(void **state)
{
    char expanded[10];
    char *do_arg[10] = {0};

    (void)state;

    do_arg[0] = "script";
    do_arg[1] = "alpha";
    do_arg[2] = "beta";
    do_arg[3] = "gamma";

    expand_command_with_args("A%*B", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "Aalpha b");
}

/* Verify a quoted whole-line command is decoded before substitution. */
static void test_sim_sub_args_decodes_single_quoted_argument(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    expand_command_with_args("\"a\\tb\"", expanded, sizeof(expanded), do_arg);
    assert_memory_equal(expanded, "a\tb", 3);
}

/* Verify whole-line quoted decode trims decoded leading whitespace. */
static void
test_sim_sub_args_decodes_single_quoted_argument_with_spaces(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    expand_command_with_args("\"  a\"", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "a");
}

/* Verify a single-quoted whole-line command is decoded too. */
static void
test_sim_sub_args_decodes_single_quoted_argument_single_quotes(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    expand_command_with_args("'a\\tb'", expanded, sizeof(expanded), do_arg);
    assert_memory_equal(expanded, "a\tb", 3);
}

/* Verify invalid whole-line quoted decode falls back to the original text. */
static void
test_sim_sub_args_preserves_invalid_single_argument_decode(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    expand_command_with_args("\"\\q\"", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "\"\\q\"");
}

/* Verify quoted-only commands with trailing tokens bypass decode cleanly. */
static void
test_sim_sub_args_preserves_quoted_command_with_trailing_text(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    expand_command_with_args("\"a\" tail", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "\"a\" tail");
}

/* Verify quoted commands skip intervening spaces before trailing text. */
static void
test_sim_sub_args_preserves_quoted_command_after_space_skip(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    expand_command_with_args("\"a\"   tail", expanded, sizeof(expanded),
                             do_arg);
    assert_string_equal(expanded, "\"a\"   tail");
}

/* Verify quoted commands with trailing spaces skip whitespace cleanly. */
static void
test_sim_sub_args_preserves_quoted_command_with_trailing_spaces(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    expand_command_with_args("\"a\"   ", expanded, sizeof(expanded), do_arg);
    assert_string_equal(expanded, "a");
}

/* Verify leading whitespace and quoted-only commands are preserved cleanly. */
static void
test_sim_sub_args_handles_leading_space_and_quoted_command(void **state)
{
    char expanded[CBUFSIZE];
    char *do_arg[10] = {0};

    (void)state;

    expand_command_with_args("  \"a\\tb\"   ", expanded, sizeof(expanded),
                             do_arg);
    assert_string_equal(expanded, "  a\tb");
}

/* Verify the expanded-string bookkeeping can map literal output back. */
static void test_sim_unsub_args_preserves_literal_positions(void **state)
{
    char expanded[CBUFSIZE];

    (void)state;

    expand_command("A literal tail", expanded, sizeof(expanded));
    assert_string_equal(expanded, "A literal tail");
    assert_string_equal(sim_unsub_args(strchr(expanded, ' ')), " literal tail");
    assert_string_equal(sim_unsub_args("outside"), "outside");
}

/* Verify SET ENVIRONMENT reports missing arguments and bad quoted strings. */
static void test_sim_set_environment_rejects_bad_inputs(void **state)
{
    (void)state;

    sim_switches = 0;
    assert_int_equal(sim_set_environment(0, ""), SCPE_2FARG);

    sim_switches = SWMASK('S');
    assert_int_equal(SCPE_BARE_STATUS(sim_set_environment(
                         0, "ZIMH_TEST_SET=\"unterminated")),
                     SCPE_ARG);

    sim_switches = SWMASK('A');
    assert_int_equal(
        SCPE_BARE_STATUS(sim_set_environment(0, "ZIMH_TEST_SET=1 + )")),
        SCPE_ARG);
}

/* Verify SET ENVIRONMENT /P covers the remaining non-interactive forms. */
static void test_sim_set_environment_prompt_reports_missing_fields(void **state)
{
    (void)state;

    sim_switches = SWMASK('P');
    sim_rem_cmd_active_line = 0;

    assert_int_equal(SCPE_BARE_STATUS(sim_set_environment(0, "   ")),
                     SCPE_2FARG);
    assert_int_equal(
        SCPE_BARE_STATUS(sim_set_environment(0, "\"\" ZIMH_TEST_SET=default")),
        SCPE_ARG);
    assert_int_equal(SCPE_BARE_STATUS(sim_set_environment(0, "\"Prompt\" ")),
                     SCPE_2FARG);

    assert_int_equal(sim_set_environment(0, "\"Prompt\" ZIMH_TEST_SET"),
                     SCPE_OK);
    assert_string_equal(getenv("ZIMH_TEST_SET"), "");
}

/* Verify replacing one alias leaves other captured aliases intact. */
static void test_sim_set_environment_keeps_other_captured_aliases(void **state)
{
    char value[CBUFSIZE];

    (void)state;

    assert_int_equal(setenv("ZIMH_ALIAS", "from-host", 1), 0);
    assert_int_equal(setenv("ZIMH_TEST_EXTERNAL", "keep-host", 1), 0);
    sim_cmdvars_capture_env_alias("ZIMH_ALIAS");
    sim_cmdvars_capture_env_alias("ZIMH_TEST_EXTERNAL");

    sim_switches = 0;
    assert_int_equal(sim_set_environment(0, "ZIMH_ALIAS=from-set"), SCPE_OK);

    assert_string_equal(getenv("ZIMH_ALIAS"), "from-set");
    assert_string_equal(
        _sim_get_env_special("ZIMH_TEST_EXTERNAL", value, sizeof(value)),
        "keep-host");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_external_environment,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_uppercased_external_environment,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_handles_malformed_substring_modifier,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_variable_without_trailing_percent,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(test_sim_sub_vars_set_get_and_unset,
                                        setup_scp_cmdvars_fixture,
                                        teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_expands_documented_builtins,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_fixed_datetime_values,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_date_19xx_leap_years,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_iso_week_boundary,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_expands_mode_variables,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_handles_sim_bin_name_edge_cases,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_handles_windows_sim_bin_name,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(test_sim_sub_var_clear_prefix,
                                        setup_scp_cmdvars_fixture,
                                        teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_substrings_and_replacements,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_preserves_value_for_empty_fixup,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_negative_and_global_substring_modifiers,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_clamps_overlong_substrings,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_expands_captured_aliases,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_cmdvars_capture_env_alias_ignores_missing_environment,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_ignores_unmatched_captured_aliases,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_var_unset_ignores_missing_name,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_omits_missing_send_default,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_omits_missing_file_compare_offset,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_file_compare_sets_diff_offset_without_environment,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_runlimit_cmd_expands_without_environment,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(test_sim_sub_args_expands_utime,
                                        setup_scp_cmdvars_fixture,
                                        teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_sim_identity_variables,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_portable_path_variables,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_do_file_attaches_output_unit_to_null_device,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(test_show_version_keeps_only_sim_ostype,
                                        setup_scp_cmdvars_fixture,
                                        teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(test_show_version_prints_zimh_banner,
                                        setup_scp_cmdvars_fixture,
                                        teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_show_version_omits_compile_timestamp,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
#if !defined(_WIN32)
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_ostype_handles_total_probe_failure,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_ostype_uses_cached_value,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
#else
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_ostype_reports_windows,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
#endif
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_handles_localtime_failure,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_tstatus_prefers_stop_message,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_tstatus_falls_back_to_error_text,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_tstatus_handles_ok_status,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_tstatus_handles_missing_stop_message,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_handles_runlimit_edge_cases,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_get_env_special_handles_exhausted_fixup_buffer,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(test_sim_set_environment_literal_value,
                                        setup_scp_cmdvars_fixture,
                                        teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_rejects_null_input,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_decodes_quoted_string,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_evaluates_expression,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_prompt_uses_default,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_prompt_uses_single_quoted_prompt,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_prompt_uses_unquoted_prompt,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_cmdvars_system_restores_captured_alias,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_cmdvars_system_without_captured_aliases,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_replaces_captured_alias,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_keeps_unmatched_captured_alias,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_prompt_reads_stdin,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_prompt_uses_default_after_empty_input,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_prompt_uses_default_after_eof,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_handles_escape_and_do_arguments,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_handles_missing_and_star_do_arguments,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_uses_zero_time_after_clock_failure,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_ignores_overlong_filepath_parts_spec,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_expands_date_d_for_sunday,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_omits_sparse_do_arguments,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_star_quotes_spaced_arguments,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_star_handles_all_do_arguments,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_star_handles_no_remaining_arguments,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_star_grows_for_long_arguments,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_star_respects_final_output_limit,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_decodes_single_quoted_argument,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_decodes_single_quoted_argument_with_spaces,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_decodes_single_quoted_argument_single_quotes,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_preserves_invalid_single_argument_decode,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_preserves_quoted_command_with_trailing_text,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_preserves_quoted_command_after_space_skip,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_preserves_quoted_command_with_trailing_spaces,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_sub_args_handles_leading_space_and_quoted_command,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_unsub_args_preserves_literal_positions,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_rejects_bad_inputs,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_prompt_reports_missing_fields,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_environment_keeps_other_captured_aliases,
            setup_scp_cmdvars_fixture, teardown_scp_cmdvars_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
