#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "sim_frontpanel.h"
#include "sim_sock.h"
#include "sim_time.h"

static int simh_test_frontpanel_write_sock(SOCKET sock, const char *msg,
                                           int nbytes);

#define sim_write_sock simh_test_frontpanel_write_sock
#include "sim_frontpanel.c"
#undef sim_write_sock

static int simh_test_frontpanel_clock_calls = 0;
static int simh_test_frontpanel_sleep_calls = 0;
static int simh_test_frontpanel_last_clock_id = -1;
static struct timespec simh_test_frontpanel_last_sleep_req = {0};
static PANEL *simh_test_frontpanel_active_panel = NULL;
static PANEL *simh_test_frontpanel_sleep_panel = NULL;
static const char *simh_test_frontpanel_command_output = "";
static int simh_test_frontpanel_prompt_status_echo = 1;
static int simh_test_frontpanel_write_count = 0;
static char simh_test_frontpanel_writes[4][4096];

/* Reset the frontpanel socket write recorder and completion hook. */
static void simh_test_frontpanel_reset_writes(void)
{
    simh_test_frontpanel_active_panel = NULL;
    simh_test_frontpanel_command_output = "";
    simh_test_frontpanel_prompt_status_echo = 1;
    simh_test_frontpanel_write_count = 0;
    memset(simh_test_frontpanel_writes, 0,
           sizeof(simh_test_frontpanel_writes));
}

/* Provide a PANEL with the locks and response storage used by send helpers. */
static void simh_test_frontpanel_init_panel(PANEL *panel)
{
    memset(panel, 0, sizeof(*panel));
    panel->sock = 1;
    assert_int_equal(pthread_mutex_init(&panel->io_lock, NULL), 0);
    assert_int_equal(pthread_mutex_init(&panel->io_send_lock, NULL), 0);
    assert_int_equal(pthread_mutex_init(&panel->io_command_lock, NULL), 0);
    assert_int_equal(pthread_cond_init(&panel->io_done, NULL), 0);
    assert_int_equal(pthread_cond_init(&panel->startup_done, NULL), 0);
    panel->io_response_size = 1;
    panel->io_response = malloc(panel->io_response_size);
    assert_non_null(panel->io_response);
    panel->io_response[0] = '\0';
}

/* Release the synchronization and response storage owned by a test PANEL. */
static void simh_test_frontpanel_destroy_panel(PANEL *panel)
{
    free(panel->io_response);
    pthread_cond_destroy(&panel->startup_done);
    pthread_cond_destroy(&panel->io_done);
    pthread_mutex_destroy(&panel->io_command_lock);
    pthread_mutex_destroy(&panel->io_send_lock);
    pthread_mutex_destroy(&panel->io_lock);
}

/* Store a simulated command response so _panel_sendf() can complete. */
static void simh_test_frontpanel_complete_command(PANEL *panel,
                                                 const char *msg)
{
    const char *status = strstr(msg, command_status);
    const char *status_prompt = simh_test_frontpanel_prompt_status_echo ?
                                sim_prompt : "";
    size_t command_len = status ? (size_t)(status - msg) : strlen(msg);
    int response_len;

    response_len = snprintf(NULL, 0, "%s%.*s\n%s%s%s\r\nStatus:00000000-\r\n",
                            sim_prompt, (int)command_len, msg,
                            simh_test_frontpanel_command_output, status_prompt,
                            command_status);
    assert_true(response_len >= 0);
    if ((size_t)response_len + 1 > panel->io_response_size) {
        char *response = realloc(panel->io_response, (size_t)response_len + 1);

        assert_non_null(response);
        panel->io_response = response;
        panel->io_response_size = (size_t)response_len + 1;
    }
    snprintf(panel->io_response, panel->io_response_size,
             "%s%.*s\n%s%s%s\r\nStatus:00000000-\r\n",
             sim_prompt, (int)command_len, msg,
             simh_test_frontpanel_command_output, status_prompt,
             command_status);
    panel->io_response_data = (size_t)response_len;
    panel->io_waiting = 0;
    pthread_cond_signal(&panel->io_done);
}

/* Capture panel writes and complete synchronous command requests. */
static int simh_test_frontpanel_write_sock(SOCKET sock, const char *msg,
                                           int nbytes)
{
    size_t copy_len;

    (void)sock;

    assert_true(nbytes >= 0);
    assert_true(simh_test_frontpanel_write_count <
                (int)(sizeof(simh_test_frontpanel_writes) /
                      sizeof(simh_test_frontpanel_writes[0])));
    copy_len = (size_t)nbytes;
    assert_true(copy_len < sizeof(simh_test_frontpanel_writes[0]));
    memcpy(simh_test_frontpanel_writes[simh_test_frontpanel_write_count],
           msg, copy_len);
    simh_test_frontpanel_writes[simh_test_frontpanel_write_count]
                               [copy_len] = '\0';
    simh_test_frontpanel_write_count++;

    if (simh_test_frontpanel_active_panel &&
        simh_test_frontpanel_active_panel->io_waiting)
        simh_test_frontpanel_complete_command(
            simh_test_frontpanel_active_panel, msg);

    return nbytes;
}

/* Verify _panel_sendf() returns only command output, not protocol echoes. */
static void test_frontpanel_sendf_returns_command_output_only(void **state)
{
    PANEL panel;
    char *response = NULL;
    int cmd_stat = -1;

    (void)state;

    simh_test_frontpanel_init_panel(&panel);
    simh_test_frontpanel_active_panel = &panel;

    assert_int_equal(_panel_sendf(&panel, &cmd_stat, &response, "SHOW FOO"),
                     0);

    assert_int_equal(cmd_stat, 0);
    assert_non_null(response);
    assert_string_equal(response, "");
    assert_int_equal(simh_test_frontpanel_write_count, 1);
    assert_string_equal(simh_test_frontpanel_writes[0],
                        "SHOW FOO\r"
                        "ECHO Status:%STATUS%-%TSTATUS%\r# COMMAND-DONE\r");

    free(response);
    response = NULL;
    simh_test_frontpanel_command_output = "first output line\r\n";

    assert_int_equal(_panel_sendf(&panel, &cmd_stat, &response, "SHOW BAR"),
                     0);

    assert_int_equal(cmd_stat, 0);
    assert_non_null(response);
    assert_string_equal(response, "first output line\r\n");
    assert_int_equal(simh_test_frontpanel_write_count, 2);
    assert_string_equal(simh_test_frontpanel_writes[1],
                        "SHOW BAR\r"
                        "ECHO Status:%STATUS%-%TSTATUS%\r# COMMAND-DONE\r");

    free(response);
    response = NULL;
    simh_test_frontpanel_command_output = "promptless output line\r\n";
    simh_test_frontpanel_prompt_status_echo = 0;

    assert_int_equal(_panel_sendf(&panel, &cmd_stat, &response, "SHOW BAZ"),
                     0);

    assert_int_equal(cmd_stat, 0);
    assert_non_null(response);
    assert_string_equal(response, "promptless output line\r\n");
    assert_int_equal(simh_test_frontpanel_write_count, 3);
    assert_string_equal(simh_test_frontpanel_writes[2],
                        "SHOW BAZ\r"
                        "ECHO Status:%STATUS%-%TSTATUS%\r# COMMAND-DONE\r");

    free(response);
    simh_test_frontpanel_active_panel = NULL;
    simh_test_frontpanel_destroy_panel(&panel);
}

/* Verify long formatted commands survive heap buffering. */
static void test_frontpanel_sendf_handles_long_formatted_command(void **state)
{
    PANEL panel;
    char payload[1500];
    char *response = NULL;
    const char *suffix =
        "\rECHO Status:%STATUS%-%TSTATUS%\r# COMMAND-DONE\r";
    size_t payload_len;
    int cmd_stat = -1;

    (void)state;

    memset(payload, 'A', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';
    payload_len = strlen(payload);

    simh_test_frontpanel_init_panel(&panel);
    simh_test_frontpanel_active_panel = &panel;

    assert_int_equal(_panel_sendf(&panel, &cmd_stat, &response, "%s",
                                  payload), 0);

    assert_int_equal(cmd_stat, 0);
    assert_non_null(response);
    assert_string_equal(response, "");
    assert_int_equal(simh_test_frontpanel_write_count, 1);
    assert_int_equal(strlen(simh_test_frontpanel_writes[0]),
                     payload_len + strlen(suffix));
    assert_int_equal(memcmp(simh_test_frontpanel_writes[0], payload,
                            payload_len), 0);
    assert_string_equal(simh_test_frontpanel_writes[0] + payload_len, suffix);

    free(response);
    simh_test_frontpanel_active_panel = NULL;
    simh_test_frontpanel_destroy_panel(&panel);
}

/* Return a deterministic timestamp through the shared time wrapper. */
static int simh_test_frontpanel_clock(int clock_id, struct timespec *tp)
{
    simh_test_frontpanel_clock_calls++;
    simh_test_frontpanel_last_clock_id = clock_id;
    if (tp != NULL)
        *tp = (struct timespec){.tv_sec = 1234, .tv_nsec = 567800000L};
    return 0;
}

/* Record sleep requests routed through the shared time wrapper. */
static int simh_test_frontpanel_sleep(const struct timespec *req,
                                      struct timespec *rem)
{
    simh_test_frontpanel_sleep_calls++;
    if (req != NULL)
        simh_test_frontpanel_last_sleep_req = *req;
    if (rem != NULL)
        *rem = (struct timespec){0};
    return 0;
}

/* Reset the shared clock hook and create the thread-name key used by debug. */
static int setup_sim_frontpanel_fixture(void **state)
{
    (void)state;

    simh_test_frontpanel_clock_calls = 0;
    simh_test_frontpanel_sleep_calls = 0;
    simh_test_frontpanel_last_clock_id = -1;
    simh_test_frontpanel_last_sleep_req = (struct timespec){0};
    simh_test_frontpanel_sleep_panel = NULL;
    simh_test_frontpanel_reset_writes();
    sim_panel_clear_error();
    sim_time_reset_test_hooks();
    assert_int_equal(pthread_key_create(&panel_thread_id, NULL), 0);
    return 0;
}

/* Clear the debug thread-name key and shared clock hook. */
static int teardown_sim_frontpanel_fixture(void **state)
{
    (void)state;

    pthread_key_delete(panel_thread_id);
    sim_panel_clear_error();
    simh_test_frontpanel_reset_writes();
    sim_time_reset_test_hooks();
    return 0;
}

/* Verify frontpanel debug timestamps come from the shared time wrapper. */
static void test_frontpanel_debug_uses_shared_clock_wrapper(void **state)
{
    PANEL panel = {0};
    char output[128];

    (void)state;

    panel.Debug = tmpfile();
    assert_non_null(panel.Debug);
    panel.debug = DBG_APP;
    sim_time_set_test_hooks(simh_test_frontpanel_clock, NULL);

    sim_panel_debug(&panel, "frontpanel event");

    fflush(panel.Debug);
    rewind(panel.Debug);
    assert_non_null(fgets(output, sizeof(output), panel.Debug));
    fclose(panel.Debug);

    assert_int_equal(simh_test_frontpanel_clock_calls, 1);
    assert_int_equal(simh_test_frontpanel_last_clock_id, CLOCK_REALTIME);
    assert_string_equal(output, "1234.567 CPU: frontpanel event\n");
}

/* Verify debug output expands telnet and control bytes predictably. */
static void test_frontpanel_debug_expands_telnet_bytes(void **state)
{
    PANEL panel = {0};
    char output[160];
    char bytes[] = {'A', TN_CR, TN_IAC, TN_WILL, TN_ECHO, 1};

    (void)state;

    panel.Debug = tmpfile();
    assert_non_null(panel.Debug);
    panel.debug = DBG_RCV;
    sim_time_set_test_hooks(simh_test_frontpanel_clock, NULL);

    _panel_debug(&panel, DBG_RCV, "bytes: ", bytes, sizeof(bytes));

    fflush(panel.Debug);
    rewind(panel.Debug);
    assert_non_null(fgets(output, sizeof(output), panel.Debug));
    fclose(panel.Debug);

    assert_string_equal(output,
                        "1234.567 CPU: bytes: "
                        "A_TN_CR__TN_IAC__TN_WILL__TN_ECHO__^A_\n");
}

/* Verify debug output handles a truncated telnet command byte safely. */
static void test_frontpanel_debug_handles_truncated_telnet_byte(void **state)
{
    PANEL panel = {0};
    char output[80];
    char bytes[] = {TN_IAC};

    (void)state;

    panel.Debug = tmpfile();
    assert_non_null(panel.Debug);
    panel.debug = DBG_RCV;
    sim_time_set_test_hooks(simh_test_frontpanel_clock, NULL);

    _panel_debug(&panel, DBG_RCV, "bytes: ", bytes, sizeof(bytes));

    fflush(panel.Debug);
    rewind(panel.Debug);
    assert_non_null(fgets(output, sizeof(output), panel.Debug));
    fclose(panel.Debug);

    assert_string_equal(output, "1234.567 CPU: bytes: _TN_IAC_\n");
}

/* Verify debug output preserves messages larger than the old stack buffer. */
static void test_frontpanel_debug_handles_long_message(void **state)
{
    PANEL panel = {0};
    char payload[12000];
    char *output;
    size_t output_size;
    const char *prefix = "1234.567 CPU: ";

    (void)state;

    memset(payload, 'D', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';
    output_size = strlen(prefix) + strlen(payload) + 2;
    output = (char *)malloc(output_size);
    assert_non_null(output);

    panel.Debug = tmpfile();
    assert_non_null(panel.Debug);
    panel.debug = DBG_APP;
    sim_time_set_test_hooks(simh_test_frontpanel_clock, NULL);

    sim_panel_debug(&panel, "%s", payload);

    fflush(panel.Debug);
    rewind(panel.Debug);
    assert_non_null(fgets(output, output_size, panel.Debug));
    fclose(panel.Debug);

    assert_int_equal(strlen(output), output_size - 1);
    assert_int_equal(memcmp(output, prefix, strlen(prefix)), 0);
    assert_int_equal(memcmp(output + strlen(prefix), payload,
                            strlen(payload)), 0);
    assert_string_equal(output + output_size - 2, "\n");

    free(output);
}

/* Verify frontpanel wait paths use the shared millisecond sleep wrapper. */
static void test_frontpanel_debug_flusher_uses_shared_sleep(void **state)
{
    PANEL panel = {0};

    (void)state;

    panel.sock = INVALID_SOCKET;
    assert_int_equal(pthread_mutex_init(&panel.io_lock, NULL), 0);
    assert_int_equal(pthread_cond_init(&panel.startup_done, NULL), 0);
    sim_time_set_test_hooks(NULL, simh_test_frontpanel_sleep);

    (void)_panel_debugflusher(&panel);

    pthread_cond_destroy(&panel.startup_done);
    pthread_mutex_destroy(&panel.io_lock);

    assert_int_equal(simh_test_frontpanel_sleep_calls, 1);
    assert_int_equal(simh_test_frontpanel_last_sleep_req.tv_sec, 0);
    assert_int_equal(simh_test_frontpanel_last_sleep_req.tv_nsec, 100000000L);
}

/* Verify setting an error records text and marks a panel failed. */
static void test_frontpanel_set_error_records_text_and_state(void **state)
{
    PANEL panel = {0};

    (void)state;

    assert_int_equal(pthread_mutex_init(&panel.io_lock, NULL), 0);

    assert_int_equal(sim_panel_set_error(&panel, "frontpanel %s %d",
                                         "error", 17), -1);

    assert_int_equal(panel.State, Error);
    assert_string_equal(sim_panel_get_error(), "frontpanel error 17");

    pthread_mutex_destroy(&panel.io_lock);
}

/* Verify long formatted error text survives buffer growth. */
static void test_frontpanel_set_error_handles_long_message(void **state)
{
    char payload[5000];
    char expected[sizeof(payload) + 16];

    (void)state;

    memset(payload, 'E', sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';
    snprintf(expected, sizeof(expected), "prefix:%s", payload);

    assert_int_equal(sim_panel_set_error(NULL, "prefix:%s", payload), -1);

    assert_string_equal(sim_panel_get_error(), expected);
}

/* Verify register bit collection sends only sampled bit registers. */
static void test_frontpanel_establishes_register_bit_collection(void **state)
{
    PANEL panel;
    int cpu_bits[2] = {0};
    int mem_bits[2] = {0};
    REG regs[] = {
        {.name = "PC"},
        {.name = "FLAGS", .device_name = "CPU", .bits = cpu_bits,
         .bit_count = 2},
        {.name = "IND", .device_name = "M", .indirect = 1, .bits = mem_bits,
         .bit_count = 2},
    };

    (void)state;

    simh_test_frontpanel_init_panel(&panel);
    panel.reg_count = sizeof(regs) / sizeof(regs[0]);
    panel.regs = regs;
    panel.sample_depth = 8;
    panel.sample_frequency = 125;
    panel.sample_dither_pct = 3;
    simh_test_frontpanel_active_panel = &panel;

    assert_int_equal(_panel_establish_register_bits_collection(&panel), 0);

    assert_int_equal(simh_test_frontpanel_write_count, 1);
    assert_string_equal(
        simh_test_frontpanel_writes[0],
        "collect 8 samples every 125 cycles dither 3 percent "
        "CPU FLAGS,-I M IND\r"
        "ECHO Status:%STATUS%-%TSTATUS%\r# COMMAND-DONE\r");

    simh_test_frontpanel_active_panel = NULL;
    simh_test_frontpanel_destroy_panel(&panel);
}

/* Verify one-shot register queries are converted to repeat queries. */
static void test_frontpanel_repeat_query_string_rewrites_markers(void **state)
{
    char *repeat;

    (void)state;

    repeat = _panel_repeat_query_string(
        "EXECUTE # REGISTERS-START;show time;E -16 CPU PC;"
        "# REGISTERS-DONE\r",
        125000);

    assert_non_null(repeat);
    assert_string_equal(repeat,
                        "repeat every 125000 usecs "
                        "# REGISTERS-REPEAT-START;"
                        "show time;E -16 CPU PC;"
                        "# REGISTERS-REPEAT-DONE");

    free(repeat);
}

/* Verify malformed one-shot register queries are rejected. */
static void test_frontpanel_repeat_query_string_rejects_missing_markers(
    void **state)
{
    (void)state;

    assert_null(_panel_repeat_query_string("show time;# REGISTERS-DONE\r",
                                           125000));
    assert_null(_panel_repeat_query_string("# REGISTERS-START;show time\r",
                                           125000));
}

/* Verify register query construction covers devices, arrays, and indirects. */
static void test_frontpanel_register_query_string_builds_full_query(
    void **state)
{
    PANEL panel;
    int bit_values[1] = {0};
    REG regs[] = {
        {.name = "PC"},
        {.name = "AC", .element_count = 3},
        {.name = "STATUS", .device_name = "CPU"},
        {.name = "IND", .device_name = "MEM", .indirect = 1},
        {.name = "FLAGS", .device_name = "CPU", .bits = bit_values,
         .bit_count = 1},
    };
    char *query = NULL;
    size_t query_size = 0;

    (void)state;

    simh_test_frontpanel_init_panel(&panel);
    panel.reg_count = sizeof(regs) / sizeof(regs[0]);
    panel.regs = regs;

    assert_int_equal(_panel_register_query_string(&panel, &query,
                                                  &query_size), 0);

    assert_string_equal(
        query,
        "EXECUTE # REGISTERS-START;show time;E -16  PC,AC[0:2];"
        "# REGISTERS-FOR-DEVICE:CPU;E -16 CPU STATUS;"
        "# REGISTER-INDIRECT:IND;E -16 MEM IND,$;"
        "sampleout;# REGISTERS-DONE\r");
    assert_int_equal(query_size, strlen(query));

    free(query);
    simh_test_frontpanel_destroy_panel(&panel);
}

/* Stop callbacks during sleep to verify repeat setup keeps its interval. */
static int simh_test_frontpanel_stop_callback_sleep(const struct timespec *req,
                                                    struct timespec *rem)
{
    simh_test_frontpanel_sleep_calls++;
    if (req != NULL)
        simh_test_frontpanel_last_sleep_req = *req;
    if (rem != NULL)
        *rem = (struct timespec){0};
    if ((simh_test_frontpanel_sleep_calls == 2) &&
        (simh_test_frontpanel_sleep_panel != NULL)) {
        pthread_mutex_lock(&simh_test_frontpanel_sleep_panel->io_lock);
        simh_test_frontpanel_sleep_panel->usecs_between_callbacks = 0;
        pthread_mutex_unlock(&simh_test_frontpanel_sleep_panel->io_lock);
    }
    return 0;
}

/* Verify callback repeat setup uses the interval captured before sleeping. */
static void test_frontpanel_callback_repeat_uses_captured_interval(void **state)
{
    PANEL panel;
    REG regs[] = {
        {.name = "PC"},
    };

    (void)state;

    simh_test_frontpanel_init_panel(&panel);
    panel.reg_count = sizeof(regs) / sizeof(regs[0]);
    panel.regs = regs;
    panel.usecs_between_callbacks = 250000;
    panel.new_register = 1;
    panel.State = Run;
    simh_test_frontpanel_active_panel = &panel;
    simh_test_frontpanel_sleep_panel = &panel;
    sim_time_set_test_hooks(NULL, simh_test_frontpanel_stop_callback_sleep);

    (void)_panel_callback(&panel);

    assert_int_equal(simh_test_frontpanel_sleep_calls, 2);
    assert_int_equal(simh_test_frontpanel_write_count, 2);
    assert_string_equal(simh_test_frontpanel_writes[0],
                        "repeat every 250000 usecs "
                        "# REGISTERS-REPEAT-START;"
                        "show time;E -16  PC;"
                        "# REGISTERS-REPEAT-DONE\r"
                        "ECHO Status:%STATUS%-%TSTATUS%\r"
                        "# COMMAND-DONE\r");
    assert_null(strstr(simh_test_frontpanel_writes[0],
                       "repeat every 0 usecs "));
    assert_string_equal(simh_test_frontpanel_writes[1],
                        "repeat stop all\r"
                        "ECHO Status:%STATUS%-%TSTATUS%\r# COMMAND-DONE\r");

    simh_test_frontpanel_active_panel = NULL;
    simh_test_frontpanel_sleep_panel = NULL;
    simh_test_frontpanel_destroy_panel(&panel);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_frontpanel_debug_uses_shared_clock_wrapper,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_debug_expands_telnet_bytes,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_debug_handles_truncated_telnet_byte,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_debug_handles_long_message,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_debug_flusher_uses_shared_sleep,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_set_error_records_text_and_state,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_set_error_handles_long_message,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_sendf_returns_command_output_only,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_sendf_handles_long_formatted_command,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_establishes_register_bit_collection,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_repeat_query_string_rewrites_markers,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_repeat_query_string_rejects_missing_markers,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_register_query_string_builds_full_query,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
        cmocka_unit_test_setup_teardown(
            test_frontpanel_callback_repeat_uses_captured_interval,
            setup_sim_frontpanel_fixture, teardown_sim_frontpanel_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
