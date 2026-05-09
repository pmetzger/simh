#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "test_cmocka.h"

#include "sim_dynstr.h"
#include "sim_dynstr_internal.h"

static int simh_test_dynstr_realloc_calls = 0;
static int simh_test_dynstr_fail_after = -1;
static bool simh_test_dynstr_vsnprintf_fail = false;

/* Fail allocation after the configured number of successful calls. */
static void *simh_test_dynstr_realloc_fail(void *ptr, size_t size)
{
    ++simh_test_dynstr_realloc_calls;
    if (simh_test_dynstr_fail_after == 0)
        return NULL;
    if (simh_test_dynstr_fail_after > 0)
        --simh_test_dynstr_fail_after;
    return realloc(ptr, size);
}

/* Force formatted append down the negative-return path when requested. */
static int PRINTF_FMT(3, 0) simh_test_dynstr_vsnprintf_fail_hook(
    char *buf, size_t size, const char *fmt, va_list args)
{
    if (simh_test_dynstr_vsnprintf_fail)
        return -1;
    return vsnprintf(buf, size, fmt, args);
}

/* Reset hooks and counters so each dynamic-string case is isolated. */
static int setup_sim_dynstr_fixture(void **state)
{
    (void)state;

    simh_test_dynstr_realloc_calls = 0;
    simh_test_dynstr_fail_after = -1;
    simh_test_dynstr_vsnprintf_fail = false;
    sim_dynstr_reset_test_hooks();
    return 0;
}

/* Clear the allocator hook after each case. */
static int teardown_sim_dynstr_fixture(void **state)
{
    (void)state;

    sim_dynstr_reset_test_hooks();
    return 0;
}

/* Verify init yields an empty logical string without storage. */
static void test_sim_dynstr_init_yields_empty_string(void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    assert_null(ds.buf);
    assert_int_equal(ds.len, 0);
    assert_int_equal(ds.cap, 0);
    assert_string_equal(sim_dynstr_cstr(&ds), "");
}

/* Verify append creates storage and preserves a trailing NUL. */
static void test_sim_dynstr_append_builds_string(void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, "alpha"));
    assert_non_null(ds.buf);
    assert_string_equal(sim_dynstr_cstr(&ds), "alpha");
    assert_int_equal(ds.len, 5);
    assert_true(ds.cap >= 6);
    sim_dynstr_free(&ds);
}

/* Verify appendf formats short text through the stack buffer path. */
static void test_sim_dynstr_appendf_builds_short_string(void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_appendf(&ds, "%s=%d", "alpha", 7));
    assert_string_equal(sim_dynstr_cstr(&ds), "alpha=7");
    assert_int_equal(ds.len, 7);
    sim_dynstr_free(&ds);
}

/* Verify append_ch extends an existing string by one character. */
static void test_sim_dynstr_append_ch_extends_string(void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, "ab"));
    assert_true(sim_dynstr_append_ch(&ds, 'c'));
    assert_string_equal(sim_dynstr_cstr(&ds), "abc");
    assert_int_equal(ds.len, 3);
    sim_dynstr_free(&ds);
}

/* Verify growth preserves earlier text while expanding capacity. */
static void test_sim_dynstr_grows_and_preserves_contents(void **state)
{
    sim_dynstr_t ds;
    char long_text[40];

    (void)state;

    memset(long_text, 'x', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, long_text));
    assert_true(sim_dynstr_append(&ds, long_text));
    assert_string_equal(sim_dynstr_cstr(&ds),
                        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    assert_true(ds.cap > 16);
    sim_dynstr_free(&ds);
}

/* Verify appendf handles output larger than the stack buffer. */
static void test_sim_dynstr_appendf_handles_long_formatted_output(void **state)
{
    sim_dynstr_t ds;
    char long_text[CBUFSIZE * 2];

    (void)state;

    memset(long_text, 'y', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_appendf(&ds, "<%s>", long_text));
    assert_int_equal(ds.len, strlen(long_text) + 2);
    assert_int_equal(sim_dynstr_cstr(&ds)[0], '<');
    assert_int_equal(sim_dynstr_cstr(&ds)[ds.len - 1], '>');
    sim_dynstr_free(&ds);
}

/* Verify append failure preserves the prior contents and capacity. */
static void test_sim_dynstr_append_failure_preserves_state(void **state)
{
    sim_dynstr_t ds;
    size_t old_cap;

    (void)state;

    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, "alpha"));
    old_cap = ds.cap;
    simh_test_dynstr_fail_after = 0;
    sim_dynstr_set_test_realloc_hook(simh_test_dynstr_realloc_fail);

    assert_false(sim_dynstr_append(&ds, "this string forces growth"));
    assert_int_equal(simh_test_dynstr_realloc_calls, 1);
    assert_string_equal(sim_dynstr_cstr(&ds), "alpha");
    assert_int_equal(ds.len, 5);
    assert_int_equal(ds.cap, old_cap);
    sim_dynstr_free(&ds);
}

/* Verify append_ch failure also preserves the earlier text. */
static void test_sim_dynstr_append_ch_failure_preserves_state(void **state)
{
    sim_dynstr_t ds;
    char full_text[32];

    (void)state;

    memset(full_text, 'a', sizeof(full_text) - 1);
    full_text[sizeof(full_text) - 1] = '\0';
    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, full_text));
    simh_test_dynstr_fail_after = 0;
    sim_dynstr_set_test_realloc_hook(simh_test_dynstr_realloc_fail);

    assert_false(sim_dynstr_append_ch(&ds, 'q'));
    assert_int_equal(simh_test_dynstr_realloc_calls, 1);
    assert_string_equal(sim_dynstr_cstr(&ds), full_text);
    assert_int_equal(ds.len, 31);
    sim_dynstr_free(&ds);
}

/* Verify appendf failure also preserves the earlier text. */
static void test_sim_dynstr_appendf_failure_preserves_state(void **state)
{
    sim_dynstr_t ds;
    char long_text[CBUFSIZE * 2];
    size_t old_cap;

    (void)state;

    memset(long_text, 'z', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, "alpha"));
    old_cap = ds.cap;
    simh_test_dynstr_fail_after = 0;
    sim_dynstr_set_test_realloc_hook(simh_test_dynstr_realloc_fail);

    assert_false(sim_dynstr_appendf(&ds, "<%s>", long_text));
    assert_int_equal(simh_test_dynstr_realloc_calls, 1);
    assert_string_equal(sim_dynstr_cstr(&ds), "alpha");
    assert_int_equal(ds.len, 5);
    assert_int_equal(ds.cap, old_cap);
    sim_dynstr_free(&ds);
}

/* Verify appendf also handles formatter failure without changing state. */
static void test_sim_dynstr_appendf_vsnprintf_failure_preserves_state(
    void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, "alpha"));
    simh_test_dynstr_vsnprintf_fail = true;
    sim_dynstr_set_test_vsnprintf_hook(simh_test_dynstr_vsnprintf_fail_hook);

    assert_false(sim_dynstr_appendf(&ds, "%s", "beta"));
    assert_string_equal(sim_dynstr_cstr(&ds), "alpha");
    assert_int_equal(ds.len, 5);
    sim_dynstr_free(&ds);
}

/* Verify leading-character trimming updates the visible contents. */
static void test_sim_dynstr_ltrim_chars_removes_requested_prefix(void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, ", ,alpha"));
    sim_dynstr_ltrim_chars(&ds, ", ");
    assert_string_equal(sim_dynstr_cstr(&ds), "alpha");
    assert_int_equal(ds.len, 5);
    sim_dynstr_free(&ds);
}

/* Verify ltrim is a no-op for strings that do not need trimming. */
static void test_sim_dynstr_ltrim_chars_leaves_other_strings_unchanged(
    void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    sim_dynstr_ltrim_chars(&ds, ", ");
    assert_null(ds.buf);

    assert_true(sim_dynstr_append(&ds, "beta"));
    sim_dynstr_ltrim_chars(&ds, ", ");
    assert_string_equal(sim_dynstr_cstr(&ds), "beta");
    assert_int_equal(ds.len, 4);
    sim_dynstr_free(&ds);
}

/* Verify take transfers ownership of the allocated buffer. */
static void test_sim_dynstr_take_transfers_buffer_ownership(void **state)
{
    sim_dynstr_t ds;
    char *taken;

    (void)state;

    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, "alpha"));
    taken = sim_dynstr_take(&ds);
    assert_non_null(taken);
    assert_string_equal(taken, "alpha");
    assert_null(ds.buf);
    assert_int_equal(ds.len, 0);
    assert_int_equal(ds.cap, 0);
    free(taken);
}

/* Verify take returns NULL when no storage has ever been allocated. */
static void test_sim_dynstr_take_returns_null_for_empty_unallocated_string(
    void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    assert_null(sim_dynstr_take(&ds));
    assert_null(ds.buf);
    assert_int_equal(ds.len, 0);
    assert_int_equal(ds.cap, 0);
}

/* Verify free releases storage and resets the struct to the empty state. */
static void test_sim_dynstr_free_resets_state(void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    assert_true(sim_dynstr_append(&ds, "alpha"));
    sim_dynstr_free(&ds);
    assert_null(ds.buf);
    assert_int_equal(ds.len, 0);
    assert_int_equal(ds.cap, 0);
    assert_string_equal(sim_dynstr_cstr(&ds), "");
}

/* Verify resetting hooks restores the default allocator path. */
static void test_sim_dynstr_reset_restores_default_allocator(void **state)
{
    sim_dynstr_t ds;

    (void)state;

    sim_dynstr_init(&ds);
    simh_test_dynstr_fail_after = 0;
    sim_dynstr_set_test_realloc_hook(simh_test_dynstr_realloc_fail);
    sim_dynstr_reset_test_hooks();

    assert_true(sim_dynstr_append(&ds, "alpha"));
    assert_string_equal(sim_dynstr_cstr(&ds), "alpha");
    sim_dynstr_free(&ds);
}

/* Verify concat returns a newly allocated C string with both inputs. */
static void test_sim_dynstr_concat_cstrs_joins_two_strings(void **state)
{
    char *joined;

    (void)state;

    joined = sim_dynstr_concat_cstrs("alpha/", "beta");
    assert_non_null(joined);
    assert_string_equal(joined, "alpha/beta");
    free(joined);
}

/* Verify concat allocates enough room for paths larger than PATH_MAX. */
static void test_sim_dynstr_concat_cstrs_keeps_long_strings(void **state)
{
    enum {
        EXTRA = 32
    };
    char directory[PATH_MAX + EXTRA];
    char filename[64];
    char *joined = NULL;
    size_t directory_len = sizeof(directory) - 1;
    size_t filename_len = sizeof(filename) - 1;

    (void)state;

    memset(directory, 'd', directory_len);
    directory[directory_len] = '\0';
    memset(filename, 'f', filename_len);
    filename[filename_len] = '\0';

    joined = sim_dynstr_concat_cstrs(directory, filename);
    assert_non_null(joined);
    assert_int_equal(strlen(joined), directory_len + filename_len);
    assert_memory_equal(joined, directory, directory_len);
    assert_memory_equal(joined + directory_len, filename, filename_len);
    free(joined);
}

/* Verify concat reports allocation failure without returning partial text. */
static void test_sim_dynstr_concat_cstrs_reports_alloc_failure(void **state)
{
    (void)state;

    simh_test_dynstr_fail_after = 0;
    sim_dynstr_set_test_realloc_hook(simh_test_dynstr_realloc_fail);

    assert_null(sim_dynstr_concat_cstrs("alpha/", "beta"));
    assert_int_equal(simh_test_dynstr_realloc_calls, 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_init_yields_empty_string, setup_sim_dynstr_fixture,
            teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(test_sim_dynstr_append_builds_string,
                                        setup_sim_dynstr_fixture,
                                        teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_appendf_builds_short_string,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_append_ch_extends_string, setup_sim_dynstr_fixture,
            teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_grows_and_preserves_contents,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_appendf_handles_long_formatted_output,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_append_failure_preserves_state,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_append_ch_failure_preserves_state,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_appendf_failure_preserves_state,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_appendf_vsnprintf_failure_preserves_state,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_ltrim_chars_removes_requested_prefix,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_ltrim_chars_leaves_other_strings_unchanged,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_take_transfers_buffer_ownership,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_take_returns_null_for_empty_unallocated_string,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(test_sim_dynstr_free_resets_state,
                                        setup_sim_dynstr_fixture,
                                        teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_reset_restores_default_allocator,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_concat_cstrs_joins_two_strings,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_concat_cstrs_keeps_long_strings,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_dynstr_concat_cstrs_reports_alloc_failure,
            setup_sim_dynstr_fixture, teardown_sim_dynstr_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
