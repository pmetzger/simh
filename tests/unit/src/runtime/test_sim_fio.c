#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_fio.h"
#include "test_support.h"

struct sim_fio_fixture {
    char temp_dir[1024];
    char nested_dir[1024];
    char file_path[1024];
    char copy_path[1024];
    char normalized_path[1024];
    char path_only[1024];
    char original_cwd[1024];
};

struct filelist_context {
    char **entries;
    size_t count;
};

struct stdout_capture_context {
    char **filelist;
};

static char *copy_env_value(const char *name)
{
    const char *value = getenv(name);
    char *copy = value ? strdup(value) : NULL;

    if (value != NULL)
        assert_non_null(copy);
    return copy;
}

static void restore_env_value(const char *name, char *value)
{
    if (value != NULL) {
        assert_int_equal(setenv(name, value, 1), 0);
        free(value);
    } else {
        assert_int_equal(unsetenv(name), 0);
    }
}

static void normalize_separators(char *path)
{
    char *p;

    for (p = path; *p != '\0'; ++p)
        if (*p == '\\')
            *p = '/';
}

#if defined(_WIN32)
static void use_windows_separators(char *path)
{
    char *p;

    for (p = path; *p != '\0'; ++p)
        if (*p == '/')
            *p = '\\';
}
#endif

/* Run one callback with TZ set to a known value, then restore it. */
static void with_timezone(const char *timezone, void (*fn)(void *ctx),
                          void *ctx)
{
    char *saved_tz = copy_env_value("TZ");

    assert_int_equal(setenv("TZ", timezone, 1), 0);
    tzset();
    fn(ctx);
    restore_env_value("TZ", saved_tz);
    tzset();
}

static int setup_sim_fio_fixture(void **state)
{
    static const uint8_t file_bytes[] = {0x10, 0x20, 0x30, 0x40, 0x50};
    struct sim_fio_fixture *fixture;

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    assert_int_equal(simh_test_make_temp_dir(fixture->temp_dir,
                                             sizeof(fixture->temp_dir),
                                             "sim-fio"),
                     0);
    assert_int_equal(simh_test_join_path(fixture->nested_dir,
                                         sizeof(fixture->nested_dir),
                                         fixture->temp_dir, "alpha"),
                     0);
    assert_int_equal(sim_mkdir(fixture->nested_dir), 0);
    assert_int_equal(simh_test_join_path(fixture->file_path,
                                         sizeof(fixture->file_path),
                                         fixture->nested_dir, "report.bin"),
                     0);
    assert_int_equal(simh_test_join_path(fixture->copy_path,
                                         sizeof(fixture->copy_path),
                                         fixture->nested_dir, "copy.bin"),
                     0);
    assert_int_equal(simh_test_write_file(fixture->file_path, file_bytes,
                                          sizeof(file_bytes)),
                     0);
    assert_non_null(
        sim_getcwd(fixture->original_cwd, sizeof(fixture->original_cwd)));

    assert_int_equal(snprintf(fixture->normalized_path,
                              sizeof(fixture->normalized_path),
                              "%s/alpha/report.bin", fixture->temp_dir) <
                         (int)sizeof(fixture->normalized_path),
                     1);
    assert_int_equal(snprintf(fixture->path_only, sizeof(fixture->path_only),
                              "%s/alpha/", fixture->temp_dir) <
                         (int)sizeof(fixture->path_only),
                     1);

    *state = fixture;
    return 0;
}

static int teardown_sim_fio_fixture(void **state)
{
    struct sim_fio_fixture *fixture = *state;

    assert_int_equal(simh_test_remove_path(fixture->temp_dir), 0);
    free(fixture);
    *state = NULL;
    return 0;
}

static void filelist_callback(const char *directory, const char *filename,
                              t_offset file_size, const struct stat *filestat,
                              void *context)
{
    struct filelist_context *result = context;
    char full_path[1024];
    char **entries;

    (void)file_size;
    (void)filestat;

    assert_int_equal(snprintf(full_path, sizeof(full_path), "%s%s", directory,
                              filename) < (int)sizeof(full_path),
                     1);

    entries = realloc(result->entries, (result->count + 1) * sizeof(*entries));
    assert_non_null(entries);
    result->entries = entries;
    result->entries[result->count] = strdup(full_path);
    assert_non_null(result->entries[result->count]);
    ++result->count;
}

static void free_filelist_context(struct filelist_context *context)
{
    size_t i;

    for (i = 0; i < context->count; ++i) {
        free(context->entries[i]);
    }
    free(context->entries);
    context->entries = NULL;
    context->count = 0;
}

static int filelist_context_contains(const struct filelist_context *context,
                                     const char *path)
{
    size_t i;

    for (i = 0; i < context->count; ++i) {
        if (strcmp(context->entries[i], path) == 0)
            return 1;
    }
    return 0;
}

static int filelist_contains(char **filelist, const char *path)
{
    size_t i;

    if (filelist == NULL) {
        return 0;
    }

    for (i = 0; filelist[i] != NULL; ++i) {
        if (strcmp(filelist[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void write_printed_filelist(void *context)
{
    struct stdout_capture_context *capture = context;

    sim_print_filelist(capture->filelist);
}

/* TODO: Add shared-memory coverage once the test environment has a
   stable policy for shm_open()-based tests. */

/* Verify trailing whitespace is removed in place and the input pointer
   is preserved. */
static void test_sim_trim_endspc_strips_trailing_whitespace(void **state)
{
    char text[] = "alpha beta \t \n";
    char *result;

    (void)state;

    result = sim_trim_endspc(text);
    assert_ptr_equal(result, text);
    assert_string_equal(text, "alpha beta");
}

/* Verify the portable strlcpy/strlcat API reports truncation like
   the BSD originals. */
static void test_strlcpy_and_strlcat_follow_bsd_semantics(void **state)
{
    char copy_buf[5];
    char cat_buf[7] = "abc";

    (void)state;

    assert_int_equal(strlcpy(copy_buf, "sample", sizeof(copy_buf)), 6);
    assert_string_equal(copy_buf, "samp");

    assert_int_equal(strlcat(cat_buf, "defghi", sizeof(cat_buf)), 9);
    assert_string_equal(cat_buf, "abcdef");
}

/* Verify the case-folding helpers preserve ordering and optionally
   collapse whitespace runs. */
static void
test_sim_string_compare_helpers_handle_case_and_whitespace(void **state)
{
    (void)state;

    assert_int_equal(sim_strncasecmp("Alpha", "aLpHaZ", 5), 0);
    assert_true(sim_strncasecmp("Alpha", "AlpHz", 5) < 0);
    assert_true(sim_strcasecmp("beta", "ALPHA") > 0);
    assert_int_equal(sim_strcasecmp("Gamma", "gAmMa"), 0);

    assert_int_equal(sim_strwhitecasecmp("Alpha\t beta", "alpha beta", true),
                     0);
    assert_true(sim_strwhitecasecmp("Alpha Beta", "alpha beta", false) < 0);
}

/* Verify filepath extraction normalizes paths and returns full, path,
   name, and extension segments. */
static void
test_sim_filepath_parts_splits_normalized_path_components(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char request_path[1024];
    char *full_path;
    char *path_only;
    char *name_only;
    char *ext_only;
    char *name_ext;

    assert_int_equal(snprintf(request_path, sizeof(request_path),
                              "%s//alpha/./beta/../%s", fixture->temp_dir,
                              "report.bin") < (int)sizeof(request_path),
                     1);

    full_path = sim_filepath_parts(request_path, "f");
    path_only = sim_filepath_parts(request_path, "p");
    name_only = sim_filepath_parts(request_path, "n");
    ext_only = sim_filepath_parts(request_path, "x");
    name_ext = sim_filepath_parts(request_path, "nx");

    assert_non_null(full_path);
    assert_non_null(path_only);
    assert_non_null(name_only);
    assert_non_null(ext_only);
    assert_non_null(name_ext);

    assert_string_equal(full_path, fixture->normalized_path);
    assert_string_equal(path_only, fixture->path_only);
    assert_string_equal(name_only, "report");
    assert_string_equal(ext_only, ".bin");
    assert_string_equal(name_ext, "report.bin");

    free(full_path);
    free(path_only);
    free(name_only);
    free(ext_only);
    free(name_ext);
}

/* Verify filesize extraction reports the stat-backed size for an
   existing file. */
static void test_sim_filepath_parts_reports_existing_file_size(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char request_path[1024];
    char *size_text;

    assert_int_equal(snprintf(request_path, sizeof(request_path),
                              "%s/alpha/report.bin",
                              fixture->temp_dir) < (int)sizeof(request_path),
                     1);

    size_text = sim_filepath_parts(request_path, "z");
    assert_non_null(size_text);
    assert_string_equal(size_text, "5 ");
    free(size_text);
}

/* Verify filepath extraction handles empty-part requests and timestamp
   formatting after quoted home-directory expansion. */
static void
test_sim_filepath_parts_handles_home_expansion_and_time_fields(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char *saved_home;
    char quoted_home_file[] = "\"~/alpha/report.bin\"";
    char *stripped_path;
    char *time_text;

    saved_home = copy_env_value("HOME");
    assert_int_equal(setenv("HOME", fixture->temp_dir, 1), 0);

    stripped_path = sim_filepath_parts(quoted_home_file, "");
    time_text = sim_filepath_parts(quoted_home_file, "t");
    assert_non_null(stripped_path);
    assert_non_null(time_text);
    assert_string_equal(stripped_path, fixture->normalized_path);
    assert_non_null(strstr(time_text, "/"));
    assert_non_null(strstr(time_text, "M "));

    free(time_text);
    free(stripped_path);

    restore_env_value("HOME", saved_home);
}

#if !defined(_WIN32)
static void expect_fixed_timestamp_text(void *ctx)
{
    struct sim_fio_fixture *fixture = ctx;
    time_t access_time = (time_t)1700000000;
    time_t write_time = (time_t)1700000123;
    char *time_text;

    assert_int_equal(
        sim_set_file_times(fixture->file_path, access_time, write_time),
        SCPE_OK);

    time_text = sim_filepath_parts(fixture->file_path, "t");
    assert_non_null(time_text);
    assert_string_equal(time_text, "11/14/2023 10:15 PM ");

    free(time_text);
}

static void expect_ordered_part_text(void *ctx)
{
    struct sim_fio_fixture *fixture = ctx;
    time_t access_time = (time_t)1700000000;
    time_t write_time = (time_t)1700000123;
    char *part_text;

    assert_int_equal(
        sim_set_file_times(fixture->file_path, access_time, write_time),
        SCPE_OK);

    part_text = sim_filepath_parts(fixture->file_path, "ztnx");
    assert_non_null(part_text);
    assert_string_equal(part_text, "5 11/14/2023 10:15 PM report.bin");

    free(part_text);
}

/* Verify timestamp extraction preserves the fixed legacy formatting. */
static void test_sim_filepath_parts_formats_fixed_timestamp(void **state)
{
    struct sim_fio_fixture *fixture = *state;

    with_timezone("UTC0", expect_fixed_timestamp_text, fixture);
}

/* Verify combined parts are appended in caller-requested order. */
static void test_sim_filepath_parts_preserves_part_order(void **state)
{
    struct sim_fio_fixture *fixture = *state;

    with_timezone("UTC0", expect_ordered_part_text, fixture);
}
#endif

/* Verify relative paths are resolved against the current directory. */
static void test_sim_filepath_parts_resolves_relative_paths(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char *full_path;
    char *name_ext;

    assert_int_equal(sim_chdir(fixture->temp_dir), 0);

    full_path = sim_filepath_parts("alpha/./report.bin", "f");
    name_ext = sim_filepath_parts("alpha/./report.bin", "nx");

    assert_non_null(full_path);
    assert_non_null(name_ext);
    assert_string_equal(full_path, fixture->normalized_path);
    assert_string_equal(name_ext, "report.bin");

    free(name_ext);
    free(full_path);

    assert_int_equal(chdir(fixture->original_cwd), 0);
}

/* Verify missing files still produce defined placeholder time and size
   text instead of crashing. */
static void test_sim_filepath_parts_handles_missing_file_metadata(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char missing_path[1024];
    char *time_text;
    char *size_text;

    assert_int_equal(simh_test_join_path(missing_path, sizeof(missing_path),
                                         fixture->temp_dir, "missing.bin"),
                     0);

    time_text = sim_filepath_parts(missing_path, "t");
    size_text = sim_filepath_parts(missing_path, "z");

    assert_non_null(time_text);
    assert_non_null(size_text);
    assert_non_null(strstr(time_text, "/"));
    assert_string_equal(size_text, "0 ");

    free(time_text);
    free(size_text);
}

/* Verify byte-swap helpers reverse multi-byte objects when the host is
   treated as big-endian. */
static void test_sim_byte_swap_helpers_reverse_multibyte_elements(void **state)
{
    uint16_t buf16[] = {0x1234, 0xABCD};
    uint8_t src32[] = {0x10, 0x20, 0x30, 0x40, 0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t dst32[sizeof(src32)];
    bool saved_sim_end;

    (void)state;

    saved_sim_end = sim_end;
    sim_end = false;

    sim_byte_swap_data(buf16, sizeof(buf16[0]), 2);
    assert_int_equal(buf16[0], 0x3412);
    assert_int_equal(buf16[1], 0xCDAB);

    memset(dst32, 0, sizeof(dst32));
    sim_buf_copy_swapped(dst32, src32, 4, 2);
    assert_memory_equal(
        dst32, ((uint8_t[]){0x40, 0x30, 0x20, 0x10, 0xDD, 0xCC, 0xBB, 0xAA}),
        sizeof(dst32));

    sim_end = saved_sim_end;
}

/* Verify endian-aware fread/fwrite preserve logical values and that
   sim_fsize_ex keeps the caller's file position intact. */
static void
test_sim_fread_and_fwrite_round_trip_in_big_endian_mode(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    uint16_t words_out[] = {0x1122, 0x3344, 0x5566};
    uint16_t words_in[3] = {0, 0, 0};
    uint8_t raw_bytes[sizeof(words_out)];
    FILE *file;
    bool saved_sim_end;

    saved_sim_end = sim_end;
    sim_end = false;

    file = sim_fopen(fixture->copy_path, "wb+");
    assert_non_null(file);

    assert_int_equal(sim_fwrite(words_out, sizeof(words_out[0]), 3, file), 3);
    assert_int_equal(sim_fsize_ex(file), (t_offset)sizeof(words_out));
    assert_int_equal(sim_ftell(file), (t_offset)sizeof(words_out));

    assert_int_equal(sim_fseeko(file, 0, SEEK_SET), 0);
    assert_int_equal(fread(raw_bytes, 1, sizeof(raw_bytes), file),
                     sizeof(raw_bytes));
    assert_memory_equal(raw_bytes,
                        ((uint8_t[]){0x11, 0x22, 0x33, 0x44, 0x55, 0x66}),
                        sizeof(raw_bytes));

    assert_int_equal(sim_fseeko(file, sizeof(words_out[0]), SEEK_SET), 0);
    assert_int_equal(sim_fsize_ex(file), (t_offset)sizeof(words_out));
    assert_int_equal(sim_ftell(file), (t_offset)sizeof(words_out[0]));

    assert_int_equal(sim_fseeko(file, 0, SEEK_SET), 0);
    assert_int_equal(sim_fread(words_in, sizeof(words_in[0]), 3, file), 3);
    assert_memory_equal(words_in, words_out, sizeof(words_out));

    fclose(file);
    sim_end = saved_sim_end;
}

/* Verify sim_fwrite advances correctly across multiple internal flip
   buffers instead of skipping to the wrong source chunk. */
static void test_sim_fwrite_handles_multiple_flip_buffers(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    const size_t count = (FLIP_SIZE / sizeof(uint16_t)) + 2;
    uint16_t *words_out;
    uint16_t *words_in;
    FILE *file;
    bool saved_sim_end;
    size_t i;

    words_out = calloc(count + 2, sizeof(*words_out));
    words_in = calloc(count, sizeof(*words_in));
    assert_non_null(words_out);
    assert_non_null(words_in);

    for (i = 0; i < count; ++i) {
        words_out[i] = (uint16_t)i;
    }
    words_out[count] = 0xDEAD;
    words_out[count + 1] = 0xBEEF;

    saved_sim_end = sim_end;
    sim_end = false;

    file = sim_fopen(fixture->copy_path, "wb+");
    assert_non_null(file);

    assert_int_equal(sim_fwrite(words_out, sizeof(words_out[0]), count, file),
                     count);
    assert_int_equal(sim_fseeko(file, 0, SEEK_SET), 0);
    assert_int_equal(sim_fread(words_in, sizeof(words_in[0]), count, file),
                     count);

    for (i = 0; i < count; ++i) {
        assert_int_equal(words_in[i], words_out[i]);
    }

    fclose(file);
    sim_end = saved_sim_end;
    free(words_in);
    free(words_out);
}

/* Verify file-size and seekability helpers behave on real files and
   named-path queries. */
static void test_sim_file_size_helpers_report_sizes_consistently(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    FILE *file;

    file = sim_fopen(fixture->file_path, "rb");
    assert_non_null(file);

    assert_true(sim_can_seek(file));
    assert_int_equal(sim_fsize(file), 5);
    assert_int_equal(sim_fsize_ex(file), 5);
    assert_int_equal(sim_fsize_name(fixture->file_path), 5);
    assert_int_equal(sim_fsize_name_ex(fixture->file_path), 5);
    assert_int_equal(sim_fsize_name_ex(fixture->copy_path), 0);

    fclose(file);
}

/* Verify sim_finit recalculates the exported host-endian and large-file
   capability flags from the current build. */
static void test_sim_finit_sets_endian_and_large_file_flags(void **state)
{
    int32_t result;

    (void)state;

    result = sim_finit();
    assert_int_equal(result, sim_end);
    assert_int_equal(sim_toffset_64, sizeof(t_offset) > sizeof(int32_t));
    assert_int_equal(sim_taddr_64,
                     sim_toffset_64 && (sizeof(t_addr) > sizeof(int32_t)));
}

/* Verify the t_addr wrapper seek helper delegates to the extended seek
   path and updates the current position correctly. */
static void test_sim_fseek_moves_to_requested_offsets(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    FILE *file;

    file = sim_fopen(fixture->file_path, "rb");
    assert_non_null(file);

    assert_int_equal(sim_fseek(file, 2, SEEK_SET), 0);
    assert_int_equal(sim_ftell(file), 2);
    assert_int_equal(sim_fseek(file, 1, SEEK_CUR), 0);
    assert_int_equal(sim_ftell(file), 3);
    assert_int_equal(sim_fseek(file, 0, SEEK_END), 0);
    assert_int_equal(sim_ftell(file), 5);

    fclose(file);
}

/* Verify sim_fopen and sim_set_fsize can create and resize a file
   addressed through home-directory expansion. */
static void test_sim_fopen_and_sim_set_fsize_handle_home_expansion(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char *saved_home;
    char relative_file[] = "~/resize.bin";
    char quoted_file[] = "\"~/resize.bin\"";
    FILE *file;
    struct stat statb;

    saved_home = copy_env_value("HOME");
    assert_int_equal(setenv("HOME", fixture->temp_dir, 1), 0);

    file = sim_fopen(relative_file, "wb+");
    assert_non_null(file);
    assert_int_equal(fwrite("abcdef", 1, 6, file), 6);
    assert_int_equal(fflush(file), 0);
    assert_int_equal(sim_set_fsize(file, 3), 0);
    fclose(file);

    assert_int_equal(sim_stat(quoted_file, &statb), 0);
    assert_int_equal((int)statb.st_size, 3);

    restore_env_value("HOME", saved_home);
}

/* Verify sim_fopen strips file-argument quotes without treating
   backslashes as C string escapes. */
static void test_sim_fopen_normalizes_quoted_paths(void **state)
{
    static const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    struct sim_fio_fixture *fixture = *state;
    char path[1024];
    char quoted_path[sizeof(path) + 2];
    FILE *file;

#if defined(_WIN32)
    assert_int_equal(simh_test_join_path(path, sizeof(path),
                                         fixture->temp_dir, "tabtest.bin"),
                     0);
    use_windows_separators(path);
#else
    assert_int_equal(simh_test_join_path(path, sizeof(path),
                                         fixture->temp_dir,
                                         "literal\\tpath.bin"),
                     0);
#endif
    assert_int_equal(simh_test_write_file(path, data, sizeof(data)), 0);
    assert_int_equal(snprintf(quoted_path, sizeof(quoted_path), "\"%s\"",
                              path) < (int)sizeof(quoted_path),
                     1);

    file = sim_fopen(quoted_path, "rb");
    assert_non_null(file);
    assert_int_equal(sim_fsize(file), sizeof(data));
    fclose(file);
}

#if !defined(_WIN32)
/* Verify sim_set_file_times updates the file timestamps to caller-
   supplied values. */
static void test_sim_set_file_times_updates_stat_times(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    struct stat statb;
    time_t access_time = (time_t)1700000000;
    time_t write_time = (time_t)1700000123;

    assert_int_equal(
        sim_set_file_times(fixture->file_path, access_time, write_time),
        SCPE_OK);
    assert_int_equal(sim_stat(fixture->file_path, &statb), 0);
    assert_int_equal(statb.st_atime, access_time);
    assert_int_equal(statb.st_mtime, write_time);
}
#endif

/* Verify missing files produce a stable strerror-backed description on
   POSIX and a system-provided description on Windows. */
static void test_sim_get_os_error_text_reports_missing_file(void **state)
{
    const char *error_text;

    (void)state;

    error_text = sim_get_os_error_text(ENOENT);
    assert_non_null(error_text);
#if defined(_WIN32)
    assert_true(error_text[0] != '\0');
#else
    assert_string_equal(error_text, strerror(ENOENT));
#endif
}

/* Verify copy and timestamp helpers reject missing inputs rather than
   silently succeeding. */
static void test_sim_file_helpers_report_missing_path_failures(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char missing_path[1024];
    char bad_dest[1024];
    t_stat status;

    assert_int_equal(simh_test_join_path(missing_path, sizeof(missing_path),
                                         fixture->temp_dir, "missing.bin"),
                     0);
    assert_int_equal(simh_test_join_path(bad_dest, sizeof(bad_dest),
                                         fixture->temp_dir, "absent/out.bin"),
                     0);

    status = sim_copyfile(missing_path, fixture->copy_path, true);
    assert_true((status & SCPE_NOMESSAGE) != 0);
    assert_int_equal(status & ~(SCPE_NOMESSAGE | SCPE_KFLAG | SCPE_BREAK),
                     SCPE_ARG);

    status = sim_copyfile(fixture->file_path, bad_dest, true);
    assert_true((status & SCPE_NOMESSAGE) != 0);
    assert_int_equal(status & ~(SCPE_NOMESSAGE | SCPE_KFLAG | SCPE_BREAK),
                     SCPE_ARG);
    status = sim_set_file_times(missing_path, 1, 2);
    assert_int_equal(SCPE_BARE_STATUS(status), SCPE_IOERR);
}

/* Verify failing open and non-seekable argument validation return
   obvious failure indicators. */
static void test_sim_fopen_and_fifo_helpers_reject_bad_inputs(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char bad_path[1024];

    assert_int_equal(simh_test_join_path(bad_path, sizeof(bad_path),
                                         fixture->temp_dir, "missing/out.bin"),
                     0);

    assert_null(sim_fopen(bad_path, "wb"));
    assert_int_equal(sim_set_fifo_nonblock(NULL), -1);
}

#if !defined(_WIN32)
/* Verify sim_set_fifo_nonblock distinguishes FIFOs from regular files
   and sets O_NONBLOCK on a FIFO stream. */
static void test_sim_set_fifo_nonblock_marks_fifo_streams(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char fifo_path[1024];
    FILE *file;
    int fd;
    int flags;

    assert_int_equal(simh_test_join_path(fifo_path, sizeof(fifo_path),
                                         fixture->temp_dir, "data.fifo"),
                     0);
    assert_int_equal(mkfifo(fifo_path, 0600), 0);

    fd = open(fifo_path, O_RDWR);
    assert_true(fd >= 0);
    file = fdopen(fd, "r+");
    assert_non_null(file);
    assert_int_equal(sim_set_fifo_nonblock(file), 0);
    flags = fcntl(fileno(file), F_GETFL, 0);
    assert_true(flags >= 0);
    assert_true((flags & O_NONBLOCK) != 0);
    fclose(file);

    file = sim_fopen(fixture->file_path, "rb");
    assert_non_null(file);
    assert_int_equal(sim_set_fifo_nonblock(file), -1);
    fclose(file);
}
#endif

/* Verify directory scanning, file-list collection, and file-copying all
   operate on the same discovered file set. */
static void
test_sim_dir_scan_get_filelist_and_copyfile_work_together(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    struct filelist_context context = {0};
    char pattern[1024];
    char second_file[1024];
    char **filelist;
    struct stat statb;

    assert_int_equal(simh_test_join_path(second_file, sizeof(second_file),
                                         fixture->nested_dir, "notes.txt"),
                     0);
    assert_int_equal(simh_test_write_file(second_file, "abc", 3), 0);

    assert_int_equal(sim_copyfile(fixture->file_path, fixture->copy_path, true),
                     SCPE_OK);
    assert_int_equal(
        simh_test_files_equal(fixture->file_path, fixture->copy_path), 1);

    assert_int_equal(snprintf(pattern, sizeof(pattern), "%s/*",
                              fixture->nested_dir) < (int)sizeof(pattern),
                     1);
    assert_int_equal(sim_dir_scan(pattern, filelist_callback, &context),
                     SCPE_OK);
    assert_int_equal(context.count, 3);
    assert_true(filelist_context_contains(&context, fixture->file_path));
    assert_true(filelist_context_contains(&context, fixture->copy_path));
    assert_true(filelist_context_contains(&context, second_file));

    filelist = sim_get_filelist(pattern);
    assert_non_null(filelist);
    assert_true(filelist_contains(filelist, fixture->file_path));
    assert_true(filelist_contains(filelist, fixture->copy_path));
    assert_true(filelist_contains(filelist, second_file));

    assert_int_equal(sim_stat(fixture->copy_path, &statb), 0);
    assert_int_equal((int)statb.st_size, 5);

    sim_free_filelist(&filelist);
    assert_null(filelist);
    free_filelist_context(&context);
}

/* Verify copyfile honors the overwrite flag when the destination file
   already exists. */
static void test_sim_copyfile_honors_overwrite_flag(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    static const uint8_t replacement_bytes[] = {0x99, 0x88, 0x77};
    void *copied_bytes;
    size_t copied_size;
    t_stat status;

    assert_int_equal(simh_test_write_file(fixture->copy_path, replacement_bytes,
                                          sizeof(replacement_bytes)),
                     0);

    status = sim_copyfile(fixture->file_path, fixture->copy_path, false);
    assert_int_equal(status & ~(SCPE_NOMESSAGE | SCPE_KFLAG | SCPE_BREAK),
                     SCPE_ARG);
    assert_int_equal(
        simh_test_read_file(fixture->copy_path, &copied_bytes, &copied_size),
        0);
    assert_int_equal(copied_size, sizeof(replacement_bytes));
    assert_memory_equal(copied_bytes, replacement_bytes, copied_size);
    free(copied_bytes);

    assert_int_equal(sim_copyfile(fixture->file_path, fixture->copy_path, true),
                     SCPE_OK);
    assert_int_equal(
        simh_test_files_equal(fixture->file_path, fixture->copy_path), 1);
}

/* Verify sim_print_filelist writes each entry on its own line through
   the normal stdout-based SIMH output path. */
static void test_sim_print_filelist_formats_one_entry_per_line(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    struct stdout_capture_context capture;
    char **filelist;
    char *text;
    size_t size;

    filelist = calloc(3, sizeof(*filelist));
    assert_non_null(filelist);
    filelist[0] = strdup(fixture->file_path);
    filelist[1] = strdup(fixture->copy_path);
    assert_non_null(filelist[0]);
    assert_non_null(filelist[1]);
    capture.filelist = filelist;

    assert_int_equal(simh_test_capture_stdout(write_printed_filelist, &capture,
                                              &text, &size),
                     0);
    assert_true(size > 0);
    assert_non_null(strstr(text, fixture->file_path));
    assert_non_null(strstr(text, fixture->copy_path));
    assert_non_null(strstr(text, "\n"));

    free(text);
    free(filelist[0]);
    free(filelist[1]);
    free(filelist);
}

/* Verify sim_print_filelist quietly ignores a NULL list. */
static void test_sim_print_filelist_ignores_null_list(void **state)
{
    char *text;
    size_t size;

    (void)state;

    assert_int_equal(simh_test_capture_stdout(
                         write_printed_filelist,
                         &(struct stdout_capture_context){NULL}, &text, &size),
                     0);
    assert_int_equal(size, 0);
    free(text);
}

/* Verify sim_dir_scan reports an argument-style failure when a pattern
   does not match any files. */
static void test_sim_dir_scan_reports_missing_matches(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char pattern[1024];

    assert_int_equal(snprintf(pattern, sizeof(pattern), "%s/*.missing",
                              fixture->nested_dir) < (int)sizeof(pattern),
                     1);
    assert_int_equal(sim_dir_scan(pattern, filelist_callback, NULL), SCPE_ARG);
}

/* Verify the directory wrappers and home-directory expansion can be used
   to create, enter, inspect, and remove a temporary subdirectory. */
static void test_sim_directory_wrappers_handle_home_expansion(void **state)
{
    struct sim_fio_fixture *fixture = *state;
    char *saved_home;
    char relative_home_dir[] = "~/simh-fio-home";
    char quoted_home_dir[] = "\"~/simh-fio-home\"";
    char expanded_home_dir[1024];
    char cwd_buf[1024];
    struct stat statb;
    struct stat cwd_statb;

    saved_home = copy_env_value("HOME");
    assert_int_equal(setenv("HOME", fixture->temp_dir, 1), 0);
    assert_int_equal(snprintf(expanded_home_dir, sizeof(expanded_home_dir),
                              "%s/simh-fio-home", fixture->temp_dir) <
                         (int)sizeof(expanded_home_dir),
                     1);

    assert_int_equal(sim_mkdir(relative_home_dir), 0);
    assert_int_equal(sim_stat(quoted_home_dir, &statb), 0);
    assert_true(S_ISDIR(statb.st_mode));

    assert_int_equal(sim_chdir(relative_home_dir), 0);
    assert_non_null(sim_getcwd(cwd_buf, sizeof(cwd_buf)));
    normalize_separators(cwd_buf);
    assert_non_null(strstr(cwd_buf, "/simh-fio-home"));
    assert_int_equal(sim_stat(".", &cwd_statb), 0);
    assert_int_equal(cwd_statb.st_dev, statb.st_dev);
    assert_int_equal(cwd_statb.st_ino, statb.st_ino);

    assert_int_equal(chdir(fixture->original_cwd), 0);
    assert_int_equal(sim_rmdir(relative_home_dir), 0);
    assert_int_not_equal(sim_stat(relative_home_dir, &statb), 0);

    restore_env_value("HOME", saved_home);
}

/* Verify the ASCII-safe character wrappers clamp non-ASCII input and
   preserve the expected behavior for simple ASCII characters. */
static void test_sim_character_class_helpers_are_ascii_safe(void **state)
{
    (void)state;

    assert_true(sim_isspace(' '));
    assert_false(sim_isspace(-1));
    assert_true(sim_islower('q'));
    assert_false(sim_islower('Q'));
    assert_true(sim_isupper('Q'));
    assert_false(sim_isupper('q'));
    assert_int_equal(sim_toupper('q'), 'Q');
    assert_int_equal(sim_tolower('Q'), 'q');
    assert_true(sim_isalpha('Z'));
    assert_false(sim_isalpha(0xC0));
    assert_true(sim_isprint('!'));
    assert_false(sim_isprint('\n'));
    assert_true(sim_isdigit('7'));
    assert_false(sim_isdigit('x'));
    assert_true(sim_isgraph('!'));
    assert_false(sim_isgraph(' '));
    assert_true(sim_isalnum('A'));
    assert_true(sim_isalnum('4'));
    assert_false(sim_isalnum(0x80));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sim_trim_endspc_strips_trailing_whitespace),
    cmocka_unit_test(test_strlcpy_and_strlcat_follow_bsd_semantics),
        cmocka_unit_test(
            test_sim_string_compare_helpers_handle_case_and_whitespace),
        cmocka_unit_test_setup_teardown(
            test_sim_filepath_parts_splits_normalized_path_components,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_filepath_parts_reports_existing_file_size,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_filepath_parts_handles_home_expansion_and_time_fields,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
#if !defined(_WIN32)
        cmocka_unit_test_setup_teardown(
            test_sim_filepath_parts_formats_fixed_timestamp,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_filepath_parts_preserves_part_order,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
#endif
        cmocka_unit_test_setup_teardown(
            test_sim_filepath_parts_resolves_relative_paths,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_filepath_parts_handles_missing_file_metadata,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test(test_sim_byte_swap_helpers_reverse_multibyte_elements),
        cmocka_unit_test_setup_teardown(
            test_sim_fread_and_fwrite_round_trip_in_big_endian_mode,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_fwrite_handles_multiple_flip_buffers,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_file_size_helpers_report_sizes_consistently,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test(test_sim_finit_sets_endian_and_large_file_flags),
        cmocka_unit_test_setup_teardown(
            test_sim_fseek_moves_to_requested_offsets, setup_sim_fio_fixture,
            teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_fopen_and_sim_set_fsize_handle_home_expansion,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_fopen_normalizes_quoted_paths,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
#if !defined(_WIN32)
        cmocka_unit_test_setup_teardown(
            test_sim_set_file_times_updates_stat_times, setup_sim_fio_fixture,
            teardown_sim_fio_fixture),
#endif
        cmocka_unit_test(test_sim_get_os_error_text_reports_missing_file),
        cmocka_unit_test_setup_teardown(
            test_sim_file_helpers_report_missing_path_failures,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_fopen_and_fifo_helpers_reject_bad_inputs,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
#if !defined(_WIN32)
        cmocka_unit_test_setup_teardown(
            test_sim_set_fifo_nonblock_marks_fifo_streams,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
#endif
        cmocka_unit_test_setup_teardown(
            test_sim_dir_scan_get_filelist_and_copyfile_work_together,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(test_sim_copyfile_honors_overwrite_flag,
                                        setup_sim_fio_fixture,
                                        teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_print_filelist_formats_one_entry_per_line,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test(test_sim_print_filelist_ignores_null_list),
        cmocka_unit_test_setup_teardown(
            test_sim_dir_scan_reports_missing_matches, setup_sim_fio_fixture,
            teardown_sim_fio_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_directory_wrappers_handle_home_expansion,
            setup_sim_fio_fixture, teardown_sim_fio_fixture),
        cmocka_unit_test(test_sim_character_class_helpers_are_ascii_safe),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
