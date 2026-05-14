#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "dynstr.h"
#include "dynstr_internal.h"

static int test_dynstr_vsnprintf_fail_on_call = 0;
static int test_dynstr_vsnprintf_calls = 0;

#define DYNSTR_TEST_STACK_SIZE 4096

/* Exercise the va_list append API from an ordinary variadic wrapper. */
static bool PRINTF_FMT(2, 3)
    test_dynstr_call_vappendf(dynstr_t *ds, const char *fmt, ...)
{
    va_list args;
    bool ok;

    va_start(args, fmt);
    ok = dynstr_vappendf(ds, fmt, args);
    va_end(args);
    return ok;
}

/* Force formatted append down the negative-return path when requested. */
static int PRINTF_FMT(3, 0)
    test_dynstr_vsnprintf_fail_hook(char *buf, size_t size, const char *fmt,
                                    va_list args)
{
    ++test_dynstr_vsnprintf_calls;
    if ((test_dynstr_vsnprintf_fail_on_call != 0) &&
        (test_dynstr_vsnprintf_calls == test_dynstr_vsnprintf_fail_on_call))
        return -1;
    return vsnprintf(buf, size, fmt, args);
}

/* Reset hooks and counters so each dynamic-string case is isolated. */
static int setup_dynstr_fixture(void **state)
{
    (void)state;

    test_dynstr_vsnprintf_fail_on_call = 0;
    test_dynstr_vsnprintf_calls = 0;
    dynstr_reset_test_hooks();
    return 0;
}

/* Clear the formatter hook after each case. */
static int teardown_dynstr_fixture(void **state)
{
    (void)state;

    dynstr_reset_test_hooks();
    return 0;
}

/* Verify init yields an empty logical string without storage. */
static void test_dynstr_init_yields_empty_string(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    assert_null(ds.buf);
    assert_int_equal(ds.len, 0);
    assert_int_equal(ds.cap, 0);
    assert_string_equal(dynstr_cstr(&ds), "");
}

/* Verify append creates storage and preserves a trailing NUL. */
static void test_dynstr_append_builds_string(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    dynstr_append(&ds, "alpha");
    assert_non_null(ds.buf);
    assert_string_equal(dynstr_cstr(&ds), "alpha");
    assert_int_equal(ds.len, 5);
    assert_true(ds.cap >= 6);
    dynstr_free(&ds);
}

/* Verify appendf formats short text through the stack buffer path. */
static void test_dynstr_appendf_builds_short_string(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    assert_true(dynstr_appendf(&ds, "%s=%d", "alpha", 7));
    assert_string_equal(dynstr_cstr(&ds), "alpha=7");
    assert_int_equal(ds.len, 7);
    dynstr_free(&ds);
}

/* Verify append_ch extends an existing string by one character. */
static void test_dynstr_append_ch_extends_string(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    dynstr_append(&ds, "ab");
    dynstr_append_ch(&ds, 'c');
    assert_string_equal(dynstr_cstr(&ds), "abc");
    assert_int_equal(ds.len, 3);
    dynstr_free(&ds);
}

/* Verify growth preserves earlier text while expanding capacity. */
static void test_dynstr_grows_and_preserves_contents(void **state)
{
    dynstr_t ds;
    char long_text[40];

    (void)state;

    memset(long_text, 'x', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    dynstr_init(&ds);
    dynstr_append(&ds, long_text);
    dynstr_append(&ds, long_text);
    assert_string_equal(dynstr_cstr(&ds),
                        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    assert_true(ds.cap > 16);
    dynstr_free(&ds);
}

/* Verify appendf handles output larger than the stack buffer. */
static void test_dynstr_appendf_handles_long_formatted_output(void **state)
{
    dynstr_t ds;
    char long_text[DYNSTR_TEST_STACK_SIZE * 2];

    (void)state;

    memset(long_text, 'y', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    dynstr_init(&ds);
    assert_true(dynstr_appendf(&ds, "<%s>", long_text));
    assert_int_equal(ds.len, strlen(long_text) + 2);
    assert_int_equal(dynstr_cstr(&ds)[0], '<');
    assert_int_equal(dynstr_cstr(&ds)[ds.len - 1], '>');
    dynstr_free(&ds);
}

/* Verify va_list formatted append has the same visible behavior as appendf. */
static void test_dynstr_vappendf_builds_string(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    assert_true(test_dynstr_call_vappendf(&ds, "%s=%d", "alpha", 7));
    assert_string_equal(dynstr_cstr(&ds), "alpha=7");
    assert_int_equal(ds.len, 7);
    dynstr_free(&ds);
}

/* Verify va_list formatted append handles output larger than the stack path. */
static void test_dynstr_vappendf_handles_long_formatted_output(void **state)
{
    dynstr_t ds;
    char long_text[DYNSTR_TEST_STACK_SIZE * 2];

    (void)state;

    memset(long_text, 'v', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    dynstr_init(&ds);
    assert_true(test_dynstr_call_vappendf(&ds, "<%s>", long_text));
    assert_int_equal(ds.len, strlen(long_text) + 2);
    assert_int_equal(dynstr_cstr(&ds)[0], '<');
    assert_int_equal(dynstr_cstr(&ds)[ds.len - 1], '>');
    dynstr_free(&ds);
}

/* Verify appendf handles initial formatter failure without changing state. */
static void
test_dynstr_appendf_initial_vsnprintf_failure_preserves_state(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    dynstr_append(&ds, "alpha");
    test_dynstr_vsnprintf_fail_on_call = 1;
    dynstr_set_test_vsnprintf_hook(test_dynstr_vsnprintf_fail_hook);

    assert_false(dynstr_appendf(&ds, "%s", "beta"));
    assert_string_equal(dynstr_cstr(&ds), "alpha");
    assert_int_equal(ds.len, 5);
    dynstr_free(&ds);
}

/* Verify appendf handles render formatter failure without changing state. */
static void
test_dynstr_appendf_render_vsnprintf_failure_preserves_state(void **state)
{
    dynstr_t ds;
    char long_text[DYNSTR_TEST_STACK_SIZE * 2];

    (void)state;

    memset(long_text, 'z', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    dynstr_init(&ds);
    dynstr_append(&ds, "alpha");
    test_dynstr_vsnprintf_fail_on_call = 2;
    dynstr_set_test_vsnprintf_hook(test_dynstr_vsnprintf_fail_hook);

    assert_false(dynstr_appendf(&ds, "%s", long_text));
    assert_string_equal(dynstr_cstr(&ds), "alpha");
    assert_int_equal(ds.len, 5);
    dynstr_free(&ds);
}

/* Verify leading-character trimming updates the visible contents. */
static void test_dynstr_ltrim_chars_removes_requested_prefix(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    dynstr_append(&ds, ", ,alpha");
    dynstr_ltrim_chars(&ds, ", ");
    assert_string_equal(dynstr_cstr(&ds), "alpha");
    assert_int_equal(ds.len, 5);
    dynstr_free(&ds);
}

/* Verify ltrim is a no-op for strings that do not need trimming. */
static void test_dynstr_ltrim_chars_leaves_other_strings_unchanged(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    dynstr_ltrim_chars(&ds, ", ");
    assert_null(ds.buf);

    dynstr_append(&ds, "beta");
    dynstr_ltrim_chars(&ds, ", ");
    assert_string_equal(dynstr_cstr(&ds), "beta");
    assert_int_equal(ds.len, 4);
    dynstr_free(&ds);
}

/* Verify take transfers ownership of the allocated buffer. */
static void test_dynstr_take_transfers_buffer_ownership(void **state)
{
    dynstr_t ds;
    char *taken;

    (void)state;

    dynstr_init(&ds);
    dynstr_append(&ds, "alpha");
    taken = dynstr_take(&ds);
    assert_non_null(taken);
    assert_string_equal(taken, "alpha");
    assert_null(ds.buf);
    assert_int_equal(ds.len, 0);
    assert_int_equal(ds.cap, 0);
    free(taken);
}

/* Verify take returns NULL when no storage has ever been allocated. */
static void
test_dynstr_take_returns_null_for_empty_unallocated_string(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    assert_null(dynstr_take(&ds));
    assert_null(ds.buf);
    assert_int_equal(ds.len, 0);
    assert_int_equal(ds.cap, 0);
}

/* Verify free releases storage and resets the struct to the empty state. */
static void test_dynstr_free_resets_state(void **state)
{
    dynstr_t ds;

    (void)state;

    dynstr_init(&ds);
    dynstr_append(&ds, "alpha");
    dynstr_free(&ds);
    assert_null(ds.buf);
    assert_int_equal(ds.len, 0);
    assert_int_equal(ds.cap, 0);
    assert_string_equal(dynstr_cstr(&ds), "");
}

/* Verify concat returns a newly allocated C string with both inputs. */
static void test_dynstr_concat_cstrs_joins_two_strings(void **state)
{
    char *joined;

    (void)state;

    joined = dynstr_concat_cstrs("alpha/", "beta");
    assert_non_null(joined);
    assert_string_equal(joined, "alpha/beta");
    free(joined);
}

/* Verify concat allocates enough room for large string pairs. */
static void test_dynstr_concat_cstrs_keeps_long_strings(void **state)
{
    enum { EXTRA = 32 };
    char directory[DYNSTR_TEST_STACK_SIZE + EXTRA];
    char filename[64];
    char *joined = NULL;
    size_t directory_len = sizeof(directory) - 1;
    size_t filename_len = sizeof(filename) - 1;

    (void)state;

    memset(directory, 'd', directory_len);
    directory[directory_len] = '\0';
    memset(filename, 'f', filename_len);
    filename[filename_len] = '\0';

    joined = dynstr_concat_cstrs(directory, filename);
    assert_non_null(joined);
    assert_int_equal(strlen(joined), directory_len + filename_len);
    assert_memory_equal(joined, directory, directory_len);
    assert_memory_equal(joined + directory_len, filename, filename_len);
    free(joined);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_dynstr_init_yields_empty_string,
                                        setup_dynstr_fixture,
                                        teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(test_dynstr_append_builds_string,
                                        setup_dynstr_fixture,
                                        teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(test_dynstr_appendf_builds_short_string,
                                        setup_dynstr_fixture,
                                        teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(test_dynstr_append_ch_extends_string,
                                        setup_dynstr_fixture,
                                        teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_grows_and_preserves_contents, setup_dynstr_fixture,
            teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_appendf_handles_long_formatted_output,
            setup_dynstr_fixture, teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(test_dynstr_vappendf_builds_string,
                                        setup_dynstr_fixture,
                                        teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_vappendf_handles_long_formatted_output,
            setup_dynstr_fixture, teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_appendf_initial_vsnprintf_failure_preserves_state,
            setup_dynstr_fixture, teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_appendf_render_vsnprintf_failure_preserves_state,
            setup_dynstr_fixture, teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_ltrim_chars_removes_requested_prefix,
            setup_dynstr_fixture, teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_ltrim_chars_leaves_other_strings_unchanged,
            setup_dynstr_fixture, teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_take_transfers_buffer_ownership, setup_dynstr_fixture,
            teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_take_returns_null_for_empty_unallocated_string,
            setup_dynstr_fixture, teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(test_dynstr_free_resets_state,
                                        setup_dynstr_fixture,
                                        teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_concat_cstrs_joins_two_strings, setup_dynstr_fixture,
            teardown_dynstr_fixture),
        cmocka_unit_test_setup_teardown(
            test_dynstr_concat_cstrs_keeps_long_strings, setup_dynstr_fixture,
            teardown_dynstr_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
