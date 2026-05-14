#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_cmocka.h"

#include "scp.h"
#include "sim_defs.h"
#include "sim_fio.h"
#include "sim_tape.h"
#include "sim_tape_internal.h"
#include "sim_types.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"
#include "test_support.h"

struct sim_tape_fixture {
    DEVICE device;
    UNIT unit;
    DEVICE *devices[2];
    char temp_dir[1024];
    char original_cwd[1024];
    char tape_path[1024];
};

struct sim_tape_capture {
    struct sim_tape_fixture *fixture;
    t_stat status;
};

struct sim_tape_attach_capture {
    UNIT *unit;
    const char *path;
    t_stat status;
};

struct sim_tape_callback_state {
    UNIT *unit;
    t_stat status;
    unsigned calls;
};

static struct sim_tape_callback_state *active_tape_callback_state;

static int setup_sim_tape_fixture(void **state)
{
    struct sim_tape_fixture *fixture;

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    simh_test_init_device_unit(&fixture->device, &fixture->unit, "TAPE",
                               "TAPE0", DEV_DISABLE, UNIT_ATTABLE, 8, 1);
    fixture->unit.dynflags = MTUF_F_STD << UNIT_V_TAPE_FMT;
    fixture->devices[0] = &fixture->device;
    fixture->devices[1] = NULL;

    assert_int_equal(simh_test_make_temp_dir(fixture->temp_dir,
                                             sizeof(fixture->temp_dir),
                                             "sim-tape"),
                     0);
    assert_non_null(
        getcwd(fixture->original_cwd, sizeof(fixture->original_cwd)));
    assert_int_equal(simh_test_join_path(fixture->tape_path,
                                         sizeof(fixture->tape_path),
                                         fixture->temp_dir, "sample.tap"),
                     0);
    assert_int_equal(
        simh_test_install_devices("zimh-unit-sim-tape", fixture->devices),
        0);
    assert_int_equal(sim_tape_init(), SCPE_OK);

    *state = fixture;
    return 0;
}

static int teardown_sim_tape_fixture(void **state)
{
    struct sim_tape_fixture *fixture = *state;

    if (fixture->unit.flags & UNIT_ATT)
        assert_int_equal(sim_tape_detach(&fixture->unit), SCPE_OK);
    assert_int_equal(chdir(fixture->original_cwd), 0);
    assert_int_equal(simh_test_remove_path(fixture->temp_dir), 0);
    simh_test_reset_simulator_state();
    free(fixture);
    *state = NULL;
    return 0;
}

static void assert_show_output(t_stat (*show_fn)(FILE *, UNIT *, int32_t,
                                                 const void *),
                               UNIT *uptr, const char *expected)
{
    FILE *stream;
    char *text;
    size_t size;

    stream = tmpfile();
    assert_non_null(stream);

    assert_int_equal(show_fn(stream, uptr, 0, NULL), SCPE_OK);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_string_equal(text, expected);

    free(text);
    fclose(stream);
}

static void assert_tape_record_equals(const uint8_t *actual, t_mtrlnt actual_len,
                                      const uint8_t *expected,
                                      t_mtrlnt expected_len)
{
    assert_int_equal(actual_len, expected_len);
    assert_memory_equal(actual, expected, expected_len);
}

static char *capture_tape_debug_output(struct sim_tape_fixture *fixture)
{
    FILE *saved_deb = sim_deb;
    int32_t saved_deb_switches = sim_deb_switches;
    uint32_t saved_unit_dctrl = fixture->unit.dctrl;
    FILE *capture;
    char *output = NULL;
    size_t output_size = 0;
    uint8_t record[] = {0x10, 0x11, 0x12, 0x13};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;

    capture = tmpfile();
    assert_non_null(capture);

    sim_deb = capture;
    sim_deb_switches |= SWMASK('F');
    fixture->unit.dctrl |= MTSE_DBG_STR;

    assert_int_equal(sim_tape_wrrecf(&fixture->unit, record, sizeof(record)),
                     MTSE_OK);
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, record,
                              sizeof(record));
    assert_int_equal(sim_tape_rdrecr(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, record,
                              sizeof(record));

    assert_int_equal(fflush(capture), 0);
    assert_int_equal(
        simh_test_read_stream(capture, &output, &output_size), 0);
    assert_int_equal(fclose(capture), 0);

    sim_deb = saved_deb;
    sim_deb_switches = saved_deb_switches;
    fixture->unit.dctrl = saved_unit_dctrl;

    return output;
}

/* Verify a counted tape image has matching leading and trailing lengths. */
static void assert_counted_tape_metadata(const uint8_t *actual,
                                         size_t payload_len,
                                         t_mtrlnt expected_len)
{
    t_mtrlnt leading;
    t_mtrlnt trailing;
    size_t header_len = sizeof(t_mtrlnt);

    memcpy(&leading, actual, sizeof(leading));
    memcpy(&trailing, &actual[header_len + payload_len], sizeof(trailing));
    sim_buf_swap_data(&leading, sizeof(leading), 1);
    sim_buf_swap_data(&trailing, sizeof(trailing), 1);
    assert_int_equal(leading, expected_len);
    assert_int_equal(trailing, expected_len);
}

/* Verify a standard tape image stores a record with any required pad byte. */
static void assert_standard_tape_file_padded_record(
    struct sim_tape_fixture *fixture, const uint8_t *expected,
    size_t expected_len)
{
    void *actual_data;
    uint8_t *actual;
    size_t actual_len;
    size_t header_len = sizeof(t_mtrlnt);
    size_t padded_len = (expected_len + 1) & ~(size_t)1;

    assert_int_equal(sim_tape_detach(&fixture->unit), SCPE_OK);
    assert_int_equal(
        simh_test_read_file(fixture->tape_path, &actual_data, &actual_len),
        0);
    actual = actual_data;
    assert_int_equal(actual_len, (2 * header_len) + padded_len);
    assert_counted_tape_metadata(actual, padded_len, (t_mtrlnt)expected_len);
    assert_memory_equal(&actual[header_len], expected, expected_len);
    if (padded_len != expected_len)
        assert_int_equal(actual[header_len + expected_len], 0);
    free(actual_data);
}

/* Verify a counted tape image stores exactly the expected record bytes. */
static void assert_counted_tape_file_payload(struct sim_tape_fixture *fixture,
                                             const char *path,
                                             const uint8_t *expected,
                                             size_t expected_len)
{
    void *actual_data;
    uint8_t *actual;
    size_t actual_len;
    size_t header_len = sizeof(t_mtrlnt);

    assert_int_equal(sim_tape_detach(&fixture->unit), SCPE_OK);
    assert_int_equal(simh_test_read_file(path, &actual_data, &actual_len), 0);
    actual = actual_data;
    assert_int_equal(actual_len, (2 * header_len) + expected_len);
    assert_counted_tape_metadata(actual, expected_len, (t_mtrlnt)expected_len);
    assert_memory_equal(&actual[header_len], expected, expected_len);
    free(actual_data);
}

/* Verify a tape image contains exactly the expected bytes. */
static void assert_tape_file_bytes(struct sim_tape_fixture *fixture,
                                   const uint8_t *expected, size_t expected_len)
{
    void *actual_data;
    size_t actual_len;

    assert_int_equal(sim_tape_detach(&fixture->unit), SCPE_OK);
    assert_int_equal(
        simh_test_read_file(fixture->tape_path, &actual_data, &actual_len),
        0);
    assert_int_equal(actual_len, expected_len);
    assert_memory_equal(actual_data, expected, expected_len);
    free(actual_data);
}

/* Read the next tape record into a caller-supplied buffer. */
static t_mtrlnt read_next_tape_record(UNIT *unit, uint8_t *record,
                                      size_t record_size)
{
    t_mtrlnt record_length;

    assert_int_equal(sim_tape_rdrecf(unit, record, &record_length,
                                     (t_mtrlnt)record_size),
                     MTSE_OK);
    return record_length;
}

/* Verify a fixed-position field in a tape record. */
static void assert_tape_record_field(const uint8_t *record,
                                     t_mtrlnt record_length, size_t offset,
                                     const char *expected)
{
    size_t expected_len = strlen(expected);

    assert_true(record_length >= offset + expected_len);
    assert_memory_equal(&record[offset], expected, expected_len);
}

/* Read the next tape record and verify one fixed-position field. */
static void assert_next_tape_record_field(UNIT *unit, size_t offset,
                                          const char *expected)
{
    uint8_t record[256] = {0};
    t_mtrlnt record_length;

    record_length = read_next_tape_record(unit, record, sizeof(record));
    assert_tape_record_field(record, record_length, offset, expected);
}

/* Write a binary file of a specific size without text line endings. */
static void write_repeated_binary_file(const char *path, size_t size)
{
    uint8_t *data;

    data = malloc(size);
    assert_non_null(data);
    memset(data, 0xaa, size);
    assert_int_equal(simh_test_write_file(path, data, size), 0);
    free(data);
}

static void assert_string_contains(const char *haystack, const char *needle)
{
    assert_non_null(strstr(haystack, needle));
}

static void record_tape_callback(UNIT *unit, t_stat status)
{
    assert_non_null(active_tape_callback_state);
    active_tape_callback_state->unit = unit;
    active_tape_callback_state->status = status;
    active_tape_callback_state->calls += 1;
}

static void reset_tape_callback_state(struct sim_tape_callback_state *state)
{
    memset(state, 0, sizeof(*state));
}

static void assert_tape_callback(struct sim_tape_callback_state *state,
                                 UNIT *unit, t_stat status)
{
    assert_int_equal(state->calls, 1);
    assert_ptr_equal(state->unit, unit);
    assert_int_equal(state->status, status);
    reset_tape_callback_state(state);
}

static void wait_for_tape_callback(struct sim_tape_callback_state *state)
{
    unsigned retry;

    for (retry = 0; (retry < 1000) && (state->calls == 0); ++retry) {
        assert_int_equal(sim_process_event(), SCPE_OK);
        usleep(1000);
    }
}

static void write_sim_tape_self_test_output(void *context)
{
    struct sim_tape_capture *capture = context;

    assert_int_equal(chdir(capture->fixture->temp_dir), 0);
    capture->status = sim_tape_test(&capture->fixture->device, NULL);
    assert_int_equal(chdir(capture->fixture->original_cwd), 0);
}

static void write_sim_tape_attach_output(void *context)
{
    struct sim_tape_attach_capture *capture = context;

    capture->status = sim_tape_attach(capture->unit, capture->path);
}

/* Verify the BOT/EOT/write-protect predicates track format, position,
   and tape flags. */
static void test_sim_tape_status_predicates_follow_unit_state(void **state)
{
    struct sim_tape_fixture *fixture = *state;

    fixture->unit.pos = 0;
    assert_true(sim_tape_bot(&fixture->unit));

    fixture->unit.pos = sizeof(t_mtrlnt);
    assert_false(sim_tape_bot(&fixture->unit));

    fixture->unit.pos = 0;
    MT_SET_INMRK(&fixture->unit);
    assert_false(sim_tape_bot(&fixture->unit));
    MT_CLR_INMRK(&fixture->unit);

    fixture->unit.capac = 0;
    assert_false(sim_tape_eot(&fixture->unit));

    fixture->unit.capac = 100;
    fixture->unit.pos = 99;
    assert_false(sim_tape_eot(&fixture->unit));
    fixture->unit.pos = 100;
    assert_true(sim_tape_eot(&fixture->unit));

    fixture->unit.flags = UNIT_ATTABLE;
    fixture->unit.dynflags = MTUF_F_STD << UNIT_V_TAPE_FMT;
    assert_false(sim_tape_wrp(&fixture->unit));

    fixture->unit.flags |= MTUF_WRP;
    assert_true(sim_tape_wrp(&fixture->unit));

    fixture->unit.flags = UNIT_ATTABLE;
    fixture->unit.dynflags = MTUF_F_TPC << UNIT_V_TAPE_FMT;
    assert_true(sim_tape_wrp(&fixture->unit));
}

/* Verify tape format changes update flags correctly and the show helper
   reports the selected format. */
static void test_sim_tape_set_and_show_format(void **state)
{
    struct sim_tape_fixture *fixture = *state;

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "TPC", NULL), SCPE_OK);
    assert_int_equal(MT_GET_FMT(&fixture->unit), MTUF_F_TPC);
    assert_true((fixture->unit.flags & UNIT_RO) != 0);
    assert_show_output(sim_tape_show_fmt, &fixture->unit, "TPC format");

    assert_true(sim_tape_set_fmt(&fixture->unit, 0, "UNKNOWN", NULL) !=
                SCPE_OK);
}

/* Verify ANSI memory-tape subformats select both the generic format and
   the ANSI subtype, and reject unknown variants. */
static void test_sim_tape_set_fmt_supports_ansi_variants(void **state)
{
    struct sim_tape_fixture *fixture = *state;

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "ANSI-RSTS", NULL),
                     SCPE_OK);
    assert_int_equal(MT_GET_FMT(&fixture->unit), MTUF_F_ANSI);
    assert_show_output(sim_tape_show_fmt, &fixture->unit, "ANSI-RSTS format");

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "ANSI-VMS", NULL),
                     SCPE_OK);
    assert_int_equal(MT_GET_FMT(&fixture->unit), MTUF_F_ANSI);
    assert_show_output(sim_tape_show_fmt, &fixture->unit, "ANSI-VMS format");

    assert_true(sim_tape_set_fmt(&fixture->unit, 0, "ANSI-BOGUS", NULL) !=
                SCPE_OK);
}

/* Verify capacity parsing stores megabytes and the show helper formats
   the stored size. */
static void test_sim_tape_set_and_show_capacity(void **state)
{
    struct sim_tape_fixture *fixture = *state;

    assert_int_equal(sim_tape_set_capac(&fixture->unit, 0, "25", NULL),
                     SCPE_OK);
    assert_int_equal(fixture->unit.capac, ((t_addr)25 * 1000000));
    assert_show_output(sim_tape_show_capac, &fixture->unit, "capacity=25MB");

    fixture->unit.flags |= UNIT_ATT;
    assert_int_equal(sim_tape_set_capac(&fixture->unit, 0, "5", NULL),
                     SCPE_ALATT);
}

/* Verify density parsing works for both direct and validated-string
   entry paths, and that supported-density formatting is stable. */
static void test_sim_tape_density_helpers_validate_and_render(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    int32_t valid_mask = MT_800_VALID | MT_1600_VALID | MT_6250_VALID;
    char density_list[64];

    assert_int_equal(
        sim_tape_set_dens(&fixture->unit, MT_DENS_1600, NULL, NULL), SCPE_OK);
    assert_int_equal(MT_DENS(fixture->unit.dynflags), MT_DENS_1600);
    assert_show_output(sim_tape_show_dens, &fixture->unit, "density=1600 bpi");

    assert_int_equal(sim_tape_set_dens(&fixture->unit, 0, "6250", &valid_mask),
                     SCPE_OK);
    assert_int_equal(MT_DENS(fixture->unit.dynflags), MT_DENS_6250);
    assert_show_output(sim_tape_show_dens, &fixture->unit, "density=6250 bpi");

    assert_int_equal(sim_tape_set_dens(&fixture->unit, 0, NULL, &valid_mask),
                     SCPE_MISVAL);
    assert_int_equal(sim_tape_set_dens(&fixture->unit, 0, "556", &valid_mask),
                     SCPE_ARG);

    assert_int_equal(sim_tape_density_supported(
                         density_list, sizeof(density_list), MT_1600_VALID),
                     SCPE_OK);
    assert_string_equal(density_list, "1600");

    assert_int_equal(sim_tape_density_supported(
                         density_list, sizeof(density_list), valid_mask),
                     SCPE_OK);
    assert_string_equal(density_list, "{800|1600|6250}");
}

/* Verify textual error translation covers named tape errors and generic
   numeric fallback. */
static void
test_sim_tape_error_text_covers_named_and_generic_errors(void **state)
{
    (void)state;

    assert_string_equal(sim_tape_error_text(MTSE_OK), "no error");
    assert_string_equal(sim_tape_error_text(MTSE_WRP), "write protected");
    assert_string_equal(sim_tape_error_text(MTSE_RUNAWAY), "tape runaway");
    assert_string_equal(sim_tape_error_text(MTSE_MAX_ERR + 5), "Error 16");
}

/* Verify attach-help text mentions the major switches, ANSI formats,
   and example commands. */
static void test_sim_tape_attach_help_describes_supported_usage(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    FILE *stream;
    char *text;
    size_t size;

    stream = tmpfile();
    assert_non_null(stream);

    assert_int_equal(
        sim_tape_attach_help(stream, &fixture->device, &fixture->unit, 0, NULL),
        SCPE_OK);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_string_contains(text, "TAPE Tape Attach Help");
    assert_string_contains(text, "-F          Open the indicated tape");
    assert_string_contains(text, "ANSI-VMS, ANSI-RT11");
    assert_string_contains(text, "sim> ATTACH TAPE -F DOS11");

    free(text);
    fclose(stream);
}

/* Verify the built-in sim_tape self-test succeeds in a temporary
   workspace and reports the API test run. */
static void test_sim_tape_self_test_succeeds(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    struct sim_tape_capture capture = {fixture, SCPE_IERR};
    char *text;
    size_t size;
    char generated_path[1024];

    assert_int_equal(simh_test_capture_stdout(write_sim_tape_self_test_output,
                                              &capture, &text, &size),
                     0);
    assert_int_equal(capture.status, SCPE_OK);
    assert_int_equal(simh_test_join_path(generated_path, sizeof(generated_path),
                                         fixture->temp_dir, "TapeTestFile1"),
                     0);
    assert_true(access(generated_path, F_OK) != 0);
    free(text);
}

/* Verify the built-in sim_tape self-test rejects attached units rather
   than mutating live tape state. */
static void test_sim_tape_self_test_rejects_attached_units(void **state)
{
    struct sim_tape_fixture *fixture = *state;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(SCPE_BARE_STATUS(sim_tape_test(&fixture->device, NULL)),
                     SCPE_ALATT);
}

/* Verify a minimal set of synchronous *_a wrappers returns the same
   status as the base operations and invokes the callback once. */
static void test_sim_tape_callback_wrappers_report_sync_status(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    struct sim_tape_callback_state callback_state;
    uint8_t record[] = {0x61, 0x62, 0x63};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;

    active_tape_callback_state = &callback_state;
    reset_tape_callback_state(&callback_state);

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(
        sim_tape_set_dens(&fixture->unit, MT_DENS_1600, NULL, NULL), SCPE_OK);

    assert_int_equal(sim_tape_wrgap_a(&fixture->unit, 0, record_tape_callback),
                     MTSE_OK);
    assert_tape_callback(&callback_state, &fixture->unit, MTSE_OK);

    assert_int_equal(sim_tape_wrrecf_a(&fixture->unit, record, sizeof(record),
                                       record_tape_callback),
                     MTSE_OK);
    assert_tape_callback(&callback_state, &fixture->unit, MTSE_OK);

    assert_int_equal(sim_tape_rewind_a(&fixture->unit, record_tape_callback),
                     MTSE_OK);
    assert_tape_callback(&callback_state, &fixture->unit, MTSE_OK);

    assert_int_equal(sim_tape_rdrecf_a(&fixture->unit, read_buffer,
                                       &record_length, sizeof(read_buffer),
                                       record_tape_callback),
                     MTSE_OK);
    assert_tape_callback(&callback_state, &fixture->unit, MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, record,
                              sizeof(record));

    active_tape_callback_state = NULL;
}

static void test_sim_tape_async_spfilebyrecf_normalizes_check_leot(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    struct sim_tape_callback_state callback_state;
    uint8_t first_record[] = {0x4A, 0x4B};
    uint32_t files_skipped = 99;
    uint32_t records_skipped = 99;
    t_stat queued_status;
#if defined(SIM_ASYNCH_IO)
    bool saved_asynch_enabled = sim_asynch_enabled;

    sim_asynch_enabled = true;
#endif

    active_tape_callback_state = &callback_state;
    reset_tape_callback_state(&callback_state);

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
#if defined(SIM_ASYNCH_IO)
    assert_non_null(fixture->unit.a_check_completion);
    assert_non_null(fixture->unit.a_is_active);
#endif
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, first_record, sizeof(first_record)),
        MTSE_OK);
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);

    queued_status = sim_tape_spfilebyrecf_a(&fixture->unit, 2, &files_skipped,
                                            &records_skipped, 2,
                                            record_tape_callback);
#if defined(SIM_ASYNCH_IO)
    assert_int_equal(queued_status, MTSE_OK);
#else
    assert_int_equal(queued_status, MTSE_LEOT);
#endif
    wait_for_tape_callback(&callback_state);
    assert_tape_callback(&callback_state, &fixture->unit, MTSE_LEOT);
    assert_int_equal(files_skipped, 1);
    assert_int_equal(records_skipped, 1);

    active_tape_callback_state = NULL;
#if defined(SIM_ASYNCH_IO)
    sim_asynch_enabled = saved_asynch_enabled;
#endif
}

/* Verify the synchronous callback wrappers return the same status as the
   base APIs and report it via the supplied callback. */
/* Verify a standard tape image can be attached, written, rewound, and
   read back including tape marks and end-of-medium status. */
static void test_sim_tape_standard_image_round_trip(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t first_record[] = {0x01, 0x02, 0x03};
    uint8_t second_record[] = {0xAA, 0xBB};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_true((fixture->unit.flags & UNIT_ATT) != 0);

    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, first_record, sizeof(first_record)),
        MTSE_OK);
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, second_record, sizeof(second_record)),
        MTSE_OK);
    assert_true(fixture->unit.tape_eom > 0);

    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);
    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, first_record,
                              sizeof(first_record));

    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_TMK);
    assert_int_equal(record_length, 0);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, second_record,
                              sizeof(second_record));

    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_EOM);
    assert_int_equal(record_length, 0);
}

/* Verify odd standard tape records are padded internally without making
   the pad byte part of the caller-visible record. */
static void
test_sim_tape_standard_odd_record_uses_internal_padding(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t record[] = {0x61, 0x62, 0x63};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(sim_tape_wrrecf(&fixture->unit, record, sizeof(record)),
                     MTSE_OK);
    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);

    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, record,
                              sizeof(record));

    assert_standard_tape_file_padded_record(fixture, record, sizeof(record));
}

/* Verify even standard tape records and odd E11 records are not padded
   in the tape image. */
static void test_sim_tape_unpadded_record_formats_use_exact_size(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    char e11_path[1024];
    uint8_t e11_record[] = {0x41, 0x42, 0x43};
    uint8_t standard_record[] = {0x51, 0x52, 0x53, 0x54};

    assert_int_equal(simh_test_join_path(e11_path, sizeof(e11_path),
                                         fixture->temp_dir, "sample.e11"),
                     0);

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(sim_tape_wrrecf(&fixture->unit, standard_record,
                                     sizeof(standard_record)),
                     MTSE_OK);
    assert_counted_tape_file_payload(fixture, fixture->tape_path,
                                     standard_record,
                                     sizeof(standard_record));

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "E11", NULL),
                     SCPE_OK);
    assert_int_equal(sim_tape_attach(&fixture->unit, e11_path), SCPE_OK);
    assert_int_equal(sim_tape_wrrecf(&fixture->unit, e11_record,
                                     sizeof(e11_record)),
                     MTSE_OK);
    assert_counted_tape_file_payload(fixture, e11_path, e11_record,
                                     sizeof(e11_record));
}

/* Verify P7B writes mark record bounds without changing the caller buffer. */
static void test_sim_tape_p7b_record_write_marks_record_bounds(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t record[] = {0x01, 0x02, 0x03};
    uint8_t expected[] = {P7B_SOR | 0x01, 0x02, 0x03, P7B_SOR | 0x01};

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "P7B", NULL),
                     SCPE_OK);
    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);

    assert_int_equal(sim_tape_wrrecf(&fixture->unit, record, sizeof(record)),
                     MTSE_OK);
    assert_int_equal(record[0], 0x01);
    assert_tape_file_bytes(fixture, expected, sizeof(expected));
}

/* Verify P7B tape marks are written as EOF records and read as marks. */
static void test_sim_tape_p7b_tape_mark_writes_eof_record(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t expected[] = {P7B_SOR | P7B_EOF, P7B_SOR | P7B_EOF};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "P7B", NULL),
                     SCPE_OK);
    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);

    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(fixture->unit.pos, 1);
    assert_int_equal(fixture->unit.tape_eom, 1);
    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_TMK);
    assert_int_equal(record_length, 0);
    assert_tape_file_bytes(fixture, expected, sizeof(expected));
}

/* Verify P7B tape marks honor write protection without moving the tape. */
static void test_sim_tape_p7b_tape_mark_rejects_write_protect(void **state)
{
    struct sim_tape_fixture *fixture = *state;

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "P7B", NULL),
                     SCPE_OK);
    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);

    fixture->unit.flags |= MTUF_WRP;
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_WRP);
    assert_int_equal(fixture->unit.pos, 0);
    assert_int_equal(fixture->unit.tape_eom, 0);
    assert_false(MT_TST_PNU(&fixture->unit));
    fixture->unit.flags &= ~MTUF_WRP;
}

/* Verify forward and reverse spacing move across records and file
   boundaries as expected on a standard tape image. */
static void test_sim_tape_spacing_and_reverse_reads_work(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t first_record[] = {0x10, 0x11, 0x12, 0x13};
    uint8_t second_record[] = {0x20, 0x21};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;
    uint32_t skipped;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, first_record, sizeof(first_record)),
        MTSE_OK);
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, second_record, sizeof(second_record)),
        MTSE_OK);
    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);

    assert_int_equal(sim_tape_sprecf(&fixture->unit, &record_length), MTSE_OK);
    assert_int_equal(record_length, sizeof(first_record));

    assert_int_equal(sim_tape_spfilef(&fixture->unit, 1, &skipped), MTSE_OK);
    assert_int_equal(skipped, 1);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, second_record,
                              sizeof(second_record));

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecr(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, second_record,
                              sizeof(second_record));

    assert_int_equal(sim_tape_rdrecr(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_TMK);
    assert_int_equal(record_length, 0);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecr(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, first_record,
                              sizeof(first_record));
}

/* Verify debug output renders tape positions and record lengths correctly. */
static void test_sim_tape_debug_output_formats_positions(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    char *output;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);

    output = capture_tape_debug_output(fixture);
    assert_non_null(strstr(output, "wr_lnt: lnt: 0, pos: 12"));
    assert_non_null(strstr(output, "rd_lntf: st: 0, lnt: 4, pos: 12"));
    assert_non_null(strstr(output, "rd_lntr: st: 0, lnt: 4, pos: 0"));
    free(output);
}

/* Verify operational error paths on attached tapes: short read buffers,
   write protection, and detach state cleanup. */
static void test_sim_tape_operational_error_paths(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t record[] = {0x33, 0x44, 0x55, 0x66};
    uint8_t short_buffer[2] = {0};
    t_mtrlnt record_length;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(sim_tape_wrrecf(&fixture->unit, record, sizeof(record)),
                     MTSE_OK);
    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);

    assert_int_equal(sim_tape_rdrecf(&fixture->unit, short_buffer,
                                     &record_length, sizeof(short_buffer)),
                     MTSE_INVRL);
    assert_true(MT_TST_PNU(&fixture->unit));

    MT_CLR_PNU(&fixture->unit);
    fixture->unit.flags |= MTUF_WRP;
    assert_int_equal(sim_tape_wrrecf(&fixture->unit, record, sizeof(record)),
                     MTSE_WRP);
    fixture->unit.flags &= ~MTUF_WRP;

    assert_int_equal(sim_tape_detach(&fixture->unit), SCPE_OK);
    assert_false((fixture->unit.flags & UNIT_ATT) != 0);
    assert_int_equal(fixture->unit.pos, 0);
    assert_int_equal(fixture->unit.tape_eom, 0);
    assert_null(fixture->unit.fileref);
    assert_null(fixture->unit.io_flush);
}

static void test_sim_tape_detach_restores_attach_format_override(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    char attach_arg[2048];

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "TPC", NULL),
                     SCPE_OK);
    assert_int_equal(MT_GET_FMT(&fixture->unit), MTUF_F_TPC);

    assert_true(snprintf(attach_arg, sizeof(attach_arg), "SIMH %s",
                         fixture->tape_path) < (int)sizeof(attach_arg));
    sim_switches = SWMASK('F');
    assert_int_equal(sim_tape_attach_ex(&fixture->unit, attach_arg, 0, 0),
                     SCPE_OK);
    assert_int_equal(MT_GET_FMT(&fixture->unit), MTUF_F_STD);

    assert_int_equal(sim_tape_detach(&fixture->unit), SCPE_OK);
    assert_int_equal(MT_GET_FMT(&fixture->unit), MTUF_F_STD);
}

/* Verify memory-backed tape formats release attach metadata on detach. */
static void test_sim_tape_memory_format_detach_clears_filename(void **state)
{
    static const char ansi_text[] = "WORLD\n";
    static const char fixed_text[] = "HELLO\n";
    struct sim_tape_fixture *fixture = *state;
    char ansi_path[1024];

    assert_int_equal(simh_test_write_file(fixture->tape_path, fixed_text,
                                          sizeof(fixed_text) - 1),
                     0);
    assert_int_equal(simh_test_join_path(ansi_path, sizeof(ansi_path),
                                         fixture->temp_dir, "sample.txt"),
                     0);
    assert_int_equal(simh_test_write_file(ansi_path, ansi_text,
                                          sizeof(ansi_text) - 1),
                     0);

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "FIXED", NULL),
                     SCPE_OK);
    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_non_null(fixture->unit.filename);
    assert_string_equal(fixture->unit.filename, fixture->tape_path);

    assert_int_equal(sim_tape_detach(&fixture->unit), SCPE_OK);
    assert_null(fixture->unit.filename);
    assert_null(fixture->unit.up8);
    assert_null(fixture->unit.io_flush);

    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "ANSI-VMS", NULL),
                     SCPE_OK);
    assert_int_equal(sim_tape_attach(&fixture->unit, ansi_path), SCPE_OK);
    assert_non_null(fixture->unit.filename);
    assert_string_equal(fixture->unit.filename, ansi_path);

    assert_int_equal(sim_tape_detach(&fixture->unit), SCPE_OK);
    assert_null(fixture->unit.filename);
    assert_null(fixture->unit.up8);
    assert_null(fixture->unit.io_flush);
}

/* Verify ANSI-VMS generated labels contain stable sequence and size fields. */
static void test_sim_tape_ansi_vms_labels_format_metadata(void **state)
{
    static const char text[] = "HELLO\n";
    struct sim_tape_fixture *fixture = *state;
    uint8_t record[256] = {0};
    t_mtrlnt record_length;

    assert_int_equal(simh_test_write_file(fixture->tape_path, text,
                                          sizeof(text) - 1),
                     0);
    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "ANSI-VMS", NULL),
                     SCPE_OK);
    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);

    assert_next_tape_record_field(&fixture->unit, 0, "VOL1");
    record_length = read_next_tape_record(&fixture->unit, record,
                                          sizeof(record));
    assert_tape_record_field(record, record_length, 0, "HDR1");
    assert_tape_record_field(record, record_length,
                             offsetof(ANSI_HDR1, file_sequence), "0001");
    record_length = read_next_tape_record(&fixture->unit, record,
                                          sizeof(record));
    assert_tape_record_field(record, record_length, 0, "HDR2");
    assert_tape_record_field(record, record_length,
                             offsetof(ANSI_HDR2, record_format),
                             "D0204800010");
    assert_next_tape_record_field(&fixture->unit, 0, "HDR3");
    assert_int_equal(sim_tape_sprecf(&fixture->unit, &record_length),
                     MTSE_TMK);
    assert_int_equal(sim_tape_sprecf(&fixture->unit, &record_length),
                     MTSE_OK);
    assert_int_equal(sim_tape_sprecf(&fixture->unit, &record_length),
                     MTSE_TMK);
    record_length = read_next_tape_record(&fixture->unit, record,
                                          sizeof(record));
    assert_tape_record_field(record, record_length, 0, "EOF1");
    assert_tape_record_field(record, record_length,
                             offsetof(ANSI_HDR1, block_count), "000001");
}

/* Verify oversized ANSI-VMS HDR2 size fields fail without attaching. */
static void test_sim_tape_ansi_vms_rejects_oversized_hdr2_fields(void **state)
{
    static const char text[] = "HELLO\n";
    struct sim_tape_fixture *fixture = *state;
    struct sim_tape_attach_capture capture;
    char *output = NULL;
    size_t output_size = 0;

    assert_int_equal(simh_test_write_file(fixture->tape_path, text,
                                          sizeof(text) - 1),
                     0);
    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "ANSI-VMS", NULL),
                     SCPE_OK);

    fixture->unit.recsize = 100000;
    capture.unit = &fixture->unit;
    capture.path = fixture->tape_path;
    capture.status = SCPE_OK;
    assert_int_equal(simh_test_capture_stdout(write_sim_tape_attach_output,
                                              &capture, &output,
                                              &output_size),
                     0);
    assert_int_equal(SCPE_BARE_STATUS(capture.status), SCPE_ARG);
    assert_non_null(strstr(output, "ANSI-VMS block size does not fit"));
    assert_false((fixture->unit.flags & UNIT_ATT) != 0);
    assert_null(fixture->unit.fileref);
    assert_null(fixture->unit.filename);
    free(output);
}

/* Verify large ANSI-VMS binary block labels keep record size distinct. */
static void test_sim_tape_ansi_vms_binary_labels_large_block(void **state)
{
    enum { BLOCK_SIZE = 65536 };
    struct sim_tape_fixture *fixture = *state;
    uint8_t record[256] = {0};
    t_mtrlnt record_length;

    write_repeated_binary_file(fixture->tape_path, BLOCK_SIZE);
    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "ANSI-VMS", NULL),
                     SCPE_OK);

    fixture->unit.recsize = BLOCK_SIZE;
    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_next_tape_record_field(&fixture->unit, 0, "VOL1");
    assert_next_tape_record_field(&fixture->unit, 0, "HDR1");
    record_length = read_next_tape_record(&fixture->unit, record,
                                          sizeof(record));
    assert_tape_record_field(record, record_length, 0, "HDR2");
    assert_tape_record_field(record, record_length,
                             offsetof(ANSI_HDR2, block_length), "65536");
    assert_tape_record_field(record, record_length,
                             offsetof(ANSI_HDR2, record_length), "00512");
    record_length = read_next_tape_record(&fixture->unit, record,
                                          sizeof(record));
    assert_tape_record_field(record, record_length, 0, "HDR3");
    assert_tape_record_field(record, record_length,
                             offsetof(ANSI_HDR3, rms_attributes), "0200");
}

/* Verify ANSI-VMS rejects binary block sizes that cannot hold padding. */
static void test_sim_tape_ansi_vms_rejects_partial_binary_block(void **state)
{
    enum { BLOCK_SIZE = 65535 };
    struct sim_tape_fixture *fixture = *state;
    struct sim_tape_attach_capture capture;
    char *output = NULL;
    size_t output_size = 0;

    write_repeated_binary_file(fixture->tape_path, BLOCK_SIZE);
    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "ANSI-VMS", NULL),
                     SCPE_OK);

    fixture->unit.recsize = BLOCK_SIZE;
    capture.unit = &fixture->unit;
    capture.path = fixture->tape_path;
    capture.status = SCPE_OK;
    assert_int_equal(simh_test_capture_stdout(write_sim_tape_attach_output,
                                              &capture, &output,
                                              &output_size),
                     0);
    assert_int_equal(SCPE_BARE_STATUS(capture.status), SCPE_ARG);
    assert_string_contains(output, "Binary file block size 65535");
    assert_string_contains(output, "ANSI record size 512");
    assert_false((fixture->unit.flags & UNIT_ATT) != 0);
    assert_null(fixture->unit.fileref);
    assert_null(fixture->unit.filename);
    free(output);
}

/* Verify ANSI-VMS HDR4 reports the stored extra file name length. */
static void test_sim_tape_ansi_vms_long_name_reports_stored_extra(void **state)
{
    static const char text[] = "HELLO\n";
    struct sim_tape_fixture *fixture = *state;
    char long_name[96];
    char long_path[1024];
    char expected_extra_name_used[3];
    uint8_t record[256] = {0};
    t_mtrlnt record_length;

    (void)snprintf(expected_extra_name_used, sizeof(expected_extra_name_used),
                   "%02u", (uint_t)sizeof(((ANSI_HDR4 *)0)->extra_name));
    memset(long_name, 'a', sizeof(long_name));
    memcpy(&long_name[sizeof(long_name) - 5], ".txt", 5);
    assert_int_equal(simh_test_join_path(long_path, sizeof(long_path),
                                         fixture->temp_dir, long_name),
                     0);
    assert_int_equal(simh_test_write_file(long_path, text, sizeof(text) - 1),
                     0);
    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "ANSI-VMS", NULL),
                     SCPE_OK);
    assert_int_equal(sim_tape_attach(&fixture->unit, long_path), SCPE_OK);

    assert_next_tape_record_field(&fixture->unit, 0, "VOL1");
    assert_next_tape_record_field(&fixture->unit, 0, "HDR1");
    assert_next_tape_record_field(&fixture->unit, 0, "HDR2");
    assert_next_tape_record_field(&fixture->unit, 0, "HDR3");
    record_length = read_next_tape_record(&fixture->unit, record,
                                          sizeof(record));
    assert_tape_record_field(record, record_length, 0, "HDR4");
    assert_tape_record_field(record, record_length,
                             offsetof(ANSI_HDR4, extra_name_used),
                             expected_extra_name_used);
}

/* Verify DOS11 generated tapes expose a header, data block, and tape mark. */
static void test_sim_tape_dos11_memory_tape_reads_file(void **state)
{
    static const char text[] = "HELLO\n";
    struct sim_tape_fixture *fixture = *state;
    char dos11_path[1024];
    uint8_t record[1024] = {0};
    t_mtrlnt record_length;

    assert_int_equal(simh_test_join_path(dos11_path, sizeof(dos11_path),
                                         fixture->temp_dir, "!!!"),
                     0);
    assert_int_equal(simh_test_write_file(dos11_path, text,
                                          sizeof(text) - 1),
                     0);
    assert_int_equal(sim_tape_set_fmt(&fixture->unit, 0, "DOS11", NULL),
                     SCPE_OK);
    assert_int_equal(sim_tape_attach(&fixture->unit, dos11_path), SCPE_OK);

    record_length = read_next_tape_record(&fixture->unit, record,
                                          sizeof(record));
    assert_int_equal(record_length, 14);
    record_length = read_next_tape_record(&fixture->unit, record,
                                          sizeof(record));
    assert_true(record_length > 0);
    assert_int_equal(sim_tape_sprecf(&fixture->unit, &record_length),
                     MTSE_TMK);
}

/* Verify DOS11 generated names use the full six-digit fallback range. */
static void test_sim_tape_dos11_fallback_name_uses_six_digits(void **state)
{
    char name[9];

    (void)state;

    sim_tape_dos11_fallback_name(name, 0);
    assert_memory_equal(name, "000000   ", sizeof(name));
    sim_tape_dos11_fallback_name(name, 99999);
    assert_memory_equal(name, "099999   ", sizeof(name));
    sim_tape_dos11_fallback_name(name, 100000);
    assert_memory_equal(name, "100000   ", sizeof(name));
    sim_tape_dos11_fallback_name(name, 999999);
    assert_memory_equal(name, "999999   ", sizeof(name));
    sim_tape_dos11_fallback_name(name, 1000000);
    assert_memory_equal(name, "000000   ", sizeof(name));
}

/* Verify ANSI decimal labels reject values that do not fit their fields. */
static void test_sim_tape_ansi_decimal_fields_validate_width(void **state)
{
    char field[6];
    char wide_field[16];

    (void)state;

    memcpy(field, "xxxxxx", sizeof(field));
    assert_true(sim_tape_format_ansi_decimal(field, 4, 0));
    assert_memory_equal(field, "0000", 4);
    assert_true(sim_tape_format_ansi_decimal(field, 4, 1));
    assert_memory_equal(field, "0001", 4);
    assert_true(sim_tape_format_ansi_decimal(field, 4, 9999));
    assert_memory_equal(field, "9999", 4);
    memcpy(field, "xxxxxx", sizeof(field));
    assert_false(sim_tape_format_ansi_decimal(field, 4, 10000));
    assert_memory_equal(field, "xxxxxx", sizeof(field));

    assert_true(sim_tape_format_ansi_decimal(field, 5, 99999));
    assert_memory_equal(field, "99999", 5);
    memcpy(field, "zzzzzz", sizeof(field));
    assert_false(sim_tape_format_ansi_decimal(field, 5, 100000));
    assert_memory_equal(field, "zzzzzz", sizeof(field));

    assert_true(sim_tape_format_ansi_decimal(field, 6, 999999));
    assert_memory_equal(field, "999999", 6);
    memcpy(field, "yyyyyy", sizeof(field));
    assert_false(sim_tape_format_ansi_decimal(field, 6, 1000000));
    assert_memory_equal(field, "yyyyyy", sizeof(field));
    assert_true(sim_tape_format_ansi_decimal(wide_field, 15,
                                             UINT64_C(123456789012345)));
    assert_memory_equal(wide_field, "123456789012345", 15);
    assert_false(sim_tape_format_ansi_decimal(NULL, 4, 1));
    assert_false(sim_tape_format_ansi_decimal(field, 0, 1));
}

/* Verify TESTLIB tape processing attach commands are built without
   truncating long filenames. */
static void test_sim_tape_test_process_args_builds_attach_commands(void **state)
{
    enum {
        FILENAME_LEN = 300
    };
    char filename[FILENAME_LEN + 1];
    const struct {
        const char *filename;
        const char *format;
        t_awslnt recsize;
        const char *expected;
    } cases[] = {
        {"TapeTestFile1.bin", "fixed", 2048,
         "fixed 2048 TapeTestFile1.bin.fixed"},
        {"TapeTestFile1.*", "dos11", 0, "dos11 TapeTestFile1.*"},
        {filename, "ansi-vms", 0, NULL},
    };
    size_t i;

    (void)state;
    memset(filename, 'x', FILENAME_LEN);
    filename[FILENAME_LEN] = '\0';

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char *args;

        args = sim_tape_test_process_args(cases[i].filename, cases[i].format,
                                          cases[i].recsize);
        assert_non_null(args);
        if (cases[i].expected != NULL)
            assert_string_equal(args, cases[i].expected);
        else {
            size_t prefix_len = strlen("ansi-vms ");

            assert_memory_equal(args, "ansi-vms ", prefix_len);
            assert_memory_equal(args + prefix_len, filename, FILENAME_LEN);
            assert_string_equal(args + prefix_len + FILENAME_LEN, ".ansi-vms");
        }
        free(args);
    }
}

/* Verify TESTLIB Architecture Workstation tape attach commands are built
   without truncating long filenames. */
static void test_sim_tape_test_aws_attach_args_builds_command(void **state)
{
    enum {
        FILENAME_LEN = 300
    };
    char filename[FILENAME_LEN + 1];
    const struct {
        const char *filename;
        const char *expected;
    } cases[] = {
        {"TapeTestFile1", "aws TapeTestFile1.aws.tape"},
        {filename, NULL},
    };
    size_t i;

    (void)state;
    memset(filename, 'x', FILENAME_LEN);
    filename[FILENAME_LEN] = '\0';

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char *args;

        args = sim_tape_test_aws_attach_args(cases[i].filename);
        assert_non_null(args);
        if (cases[i].expected != NULL)
            assert_string_equal(args, cases[i].expected);
        else {
            size_t prefix_len = strlen("aws ");

            assert_memory_equal(args, "aws ", prefix_len);
            assert_memory_equal(args + prefix_len, filename, FILENAME_LEN);
            assert_string_equal(args + prefix_len + FILENAME_LEN, ".aws.tape");
        }
        free(args);
    }
}

/* Verify TESTLIB classification attach commands are built without
   truncating long test file names. */
static void test_sim_tape_test_classify_args_builds_command(void **state)
{
    enum {
        TEST_NAME_LEN = 300
    };
    char test_name[TEST_NAME_LEN + 1];
    const struct {
        const char *unit_name;
        const char *attach_args;
        const char *test_name;
        const char *expected;
    } cases[] = {
        {"TAPE0", "-F FIXED", "sample.txt",
         "TAPE0 -v -F FIXED sample.txt"},
        {"TAPE0", "-F FIXED", test_name, NULL},
    };
    size_t i;

    (void)state;
    memset(test_name, 'x', TEST_NAME_LEN);
    test_name[TEST_NAME_LEN] = '\0';

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char *args;

        args = sim_tape_test_classify_args(cases[i].unit_name,
                                           cases[i].attach_args,
                                           cases[i].test_name);
        assert_non_null(args);
        if (cases[i].expected != NULL)
            assert_string_equal(args, cases[i].expected);
        else {
            size_t prefix_len = strlen("TAPE0 -v -F FIXED ");

            assert_memory_equal(args, "TAPE0 -v -F FIXED ", prefix_len);
            assert_string_equal(args + prefix_len, test_name);
        }
        free(args);
    }
}

/* Verify TESTLIB file table helpers open every suffixed file and close all
   recorded streams. */
static void test_sim_tape_test_file_table_opens_and_closes(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    FILE *first = NULL;
    FILE *second = NULL;
    SIM_TAPE_TEST_FILE files[] = {
        {&first, ".one"},
        {&second, ".two"},
    };
    char path[1024];

    assert_int_equal(sim_tape_test_open_files(files, 2, fixture->tape_path),
                     SCPE_OK);
    assert_non_null(first);
    assert_non_null(second);

    assert_int_equal(simh_test_join_path(path, sizeof(path), fixture->temp_dir,
                                         "sample.tap.one"),
                     0);
    assert_int_equal(access(path, F_OK), 0);
    assert_int_equal(simh_test_join_path(path, sizeof(path), fixture->temp_dir,
                                         "sample.tap.two"),
                     0);
    assert_int_equal(access(path, F_OK), 0);

    sim_tape_test_close_files(files, 2);
    assert_null(first);
    assert_null(second);
}

/* Verify TESTLIB file table open failures close any streams opened earlier in
   the table. */
static void test_sim_tape_test_file_table_closes_partial_failure(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    FILE *first = NULL;
    FILE *second = NULL;
    SIM_TAPE_TEST_FILE files[] = {
        {&first, ".one"},
        {&second, "/missing-parent/file"},
    };

    assert_int_equal(sim_tape_test_open_files(files, 2, fixture->tape_path),
                     SCPE_OPENERR);
    assert_null(first);
    assert_null(second);
}

/* Verify generic positioning can count objects and position by
   file/record tuples after an optional rewind. */
static void test_sim_tape_position_tracks_objects_and_files(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t first_record[] = {0x01, 0x03, 0x05};
    uint8_t second_record[] = {0x02, 0x04};
    uint8_t third_record[] = {0x06};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;
    uint32_t records_skipped;
    uint32_t files_skipped;
    uint32_t objects_skipped;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, first_record, sizeof(first_record)),
        MTSE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, second_record, sizeof(second_record)),
        MTSE_OK);
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, third_record, sizeof(third_record)),
        MTSE_OK);

    assert_int_equal(sim_tape_position(
                         &fixture->unit, MTPOS_M_REW | MTPOS_M_OBJ, 3,
                         &records_skipped, 0, &files_skipped, &objects_skipped),
                     MTSE_OK);
    assert_int_equal(records_skipped, 0);
    assert_int_equal(files_skipped, 0);
    assert_int_equal(objects_skipped, 3);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, third_record,
                              sizeof(third_record));

    assert_int_equal(sim_tape_position(&fixture->unit, MTPOS_M_REW, 1,
                                       &records_skipped, 1, &files_skipped,
                                       &objects_skipped),
                     MTSE_OK);
    assert_int_equal(records_skipped, 1);
    assert_int_equal(files_skipped, 1);
    assert_int_equal(objects_skipped, 4);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_EOM);
    assert_int_equal(record_length, 0);
}

/* Verify direct file-spacing reports both file and record counts and
   detects logical end of tape on a double tape mark. */
static void test_sim_tape_spfilebyrecf_reports_counts_and_leot(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t first_record[] = {0x0A, 0x0B};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;
    uint32_t files_skipped;
    uint32_t records_skipped;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, first_record, sizeof(first_record)),
        MTSE_OK);
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);

    assert_int_equal(sim_tape_spfilebyrecf(&fixture->unit, 1, &files_skipped,
                                           &records_skipped, false),
                     MTSE_OK);
    assert_int_equal(files_skipped, 1);
    assert_int_equal(records_skipped, 1);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_TMK);
    assert_int_equal(record_length, 0);

    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);
    assert_int_equal(sim_tape_spfilebyrecf(&fixture->unit, 2, &files_skipped,
                                           &records_skipped, true),
                     MTSE_LEOT);
    assert_int_equal(files_skipped, 1);
    assert_int_equal(records_skipped, 1);
}

/* Verify reverse file-spacing reports both file and record counts and
   leaves the unit positioned at the preceding file boundary. */
static void test_sim_tape_spfilebyrecr_reports_counts(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t first_record[] = {0x31, 0x32};
    uint8_t second_record[] = {0x41, 0x42, 0x43};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;
    uint32_t files_skipped;
    uint32_t records_skipped;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, first_record, sizeof(first_record)),
        MTSE_OK);
    assert_int_equal(sim_tape_wrtmk(&fixture->unit), MTSE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, second_record, sizeof(second_record)),
        MTSE_OK);

    assert_int_equal(sim_tape_spfilebyrecr(&fixture->unit, 1, &files_skipped,
                                           &records_skipped),
                     MTSE_OK);
    assert_int_equal(files_skipped, 1);
    assert_int_equal(records_skipped, 1);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecr(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, first_record,
                              sizeof(first_record));
}

/* Verify write-EOM-and-rewind leaves the unit rewound and makes the
   newly written EOM visible after the existing data. */
static void test_sim_tape_wreomrw_writes_eom_and_rewinds(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t record[] = {0x11, 0x22, 0x33};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(sim_tape_wrrecf(&fixture->unit, record, sizeof(record)),
                     MTSE_OK);
    MT_SET_PNU(&fixture->unit);

    assert_int_equal(sim_tape_wreomrw(&fixture->unit), MTSE_OK);
    assert_true(sim_tape_bot(&fixture->unit));
    assert_false(MT_TST_PNU(&fixture->unit));

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, record,
                              sizeof(record));

    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_EOM);
    assert_int_equal(record_length, 0);
}

/* Verify reset clears position-not-updated state without detaching or
   rewinding an attached tape. */
static void test_sim_tape_reset_clears_pnu_without_detaching(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t record[] = {0x09, 0x08, 0x07};
    t_addr saved_pos;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(sim_tape_wrrecf(&fixture->unit, record, sizeof(record)),
                     MTSE_OK);
    saved_pos = fixture->unit.pos;
    MT_SET_PNU(&fixture->unit);

    assert_int_equal(sim_tape_reset(&fixture->unit), SCPE_OK);
    assert_false(MT_TST_PNU(&fixture->unit));
    assert_true((fixture->unit.flags & UNIT_ATT) != 0);
    assert_int_equal(fixture->unit.pos, saved_pos);
}

/* Verify erase-gap and erase-record operations modify standard tapes in
   the expected direction-sensitive ways. */
static void test_sim_tape_gap_and_erase_operations_modify_records(void **state)
{
    struct sim_tape_fixture *fixture = *state;
    uint8_t first_record[] = {0x21, 0x22, 0x23};
    uint8_t second_record[] = {0x51, 0x52};
    uint8_t read_buffer[16] = {0};
    t_mtrlnt record_length;

    assert_int_equal(sim_tape_attach(&fixture->unit, fixture->tape_path),
                     SCPE_OK);
    assert_int_equal(sim_tape_wrgap(&fixture->unit, 1), MTSE_IOERR);
    assert_int_equal(
        sim_tape_set_dens(&fixture->unit, MT_DENS_1600, NULL, NULL), SCPE_OK);
    assert_int_equal(sim_tape_wrgap(&fixture->unit, 0), MTSE_OK);

    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, first_record, sizeof(first_record)),
        MTSE_OK);
    assert_int_equal(
        sim_tape_wrrecf(&fixture->unit, second_record, sizeof(second_record)),
        MTSE_OK);
    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);

    assert_int_equal(sim_tape_errecf(&fixture->unit, sizeof(first_record)),
                     MTSE_OK);
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_OK);
    assert_tape_record_equals(read_buffer, record_length, second_record,
                              sizeof(second_record));

    assert_int_equal(sim_tape_errecr(&fixture->unit, sizeof(second_record)),
                     MTSE_OK);
    assert_int_equal(sim_tape_rewind(&fixture->unit), MTSE_OK);
    assert_int_equal(sim_tape_rdrecf(&fixture->unit, read_buffer,
                                     &record_length, sizeof(read_buffer)),
                     MTSE_RUNAWAY);
    assert_int_equal(record_length, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_sim_tape_status_predicates_follow_unit_state,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(test_sim_tape_set_and_show_format,
                                        setup_sim_tape_fixture,
                                        teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_set_fmt_supports_ansi_variants,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(test_sim_tape_set_and_show_capacity,
                                        setup_sim_tape_fixture,
                                        teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_density_helpers_validate_and_render,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_attach_help_describes_supported_usage,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(test_sim_tape_self_test_succeeds,
                                        setup_sim_tape_fixture,
                                        teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_self_test_rejects_attached_units,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_callback_wrappers_report_sync_status,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_async_spfilebyrecf_normalizes_check_leot,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(test_sim_tape_standard_image_round_trip,
                                        setup_sim_tape_fixture,
                                        teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_standard_odd_record_uses_internal_padding,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_unpadded_record_formats_use_exact_size,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_p7b_record_write_marks_record_bounds,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_p7b_tape_mark_writes_eof_record,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_p7b_tape_mark_rejects_write_protect,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_spacing_and_reverse_reads_work,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_debug_output_formats_positions,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_spfilebyrecf_reports_counts_and_leot,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_spfilebyrecr_reports_counts, setup_sim_tape_fixture,
            teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_position_tracks_objects_and_files,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_wreomrw_writes_eom_and_rewinds,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_reset_clears_pnu_without_detaching,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_gap_and_erase_operations_modify_records,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(test_sim_tape_operational_error_paths,
                                        setup_sim_tape_fixture,
                                        teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_detach_restores_attach_format_override,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_memory_format_detach_clears_filename,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_ansi_vms_labels_format_metadata,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_ansi_vms_rejects_oversized_hdr2_fields,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_ansi_vms_binary_labels_large_block,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_ansi_vms_rejects_partial_binary_block,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_ansi_vms_long_name_reports_stored_extra,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_dos11_memory_tape_reads_file,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test(
            test_sim_tape_dos11_fallback_name_uses_six_digits),
        cmocka_unit_test(
            test_sim_tape_ansi_decimal_fields_validate_width),
        cmocka_unit_test(
            test_sim_tape_test_process_args_builds_attach_commands),
        cmocka_unit_test(test_sim_tape_test_aws_attach_args_builds_command),
        cmocka_unit_test(test_sim_tape_test_classify_args_builds_command),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_test_file_table_opens_and_closes,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_tape_test_file_table_closes_partial_failure,
            setup_sim_tape_fixture, teardown_sim_tape_fixture),
        cmocka_unit_test(
            test_sim_tape_error_text_covers_named_and_generic_errors),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
