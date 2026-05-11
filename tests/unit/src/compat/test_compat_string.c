#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

/*
 * macOS may expose strlcpy/strlcat as fortified function-like macros.
 * Undefine them only in this test so we exercise the compat objects.
 */
#undef strlcpy
#undef strlcat
#undef strdup
#undef strndup

#include "sim_string_compat.h"

/* Verify strlcpy reports source length and NUL-terminates truncated copies. */
static void test_strlcpy_reports_truncation(void **state)
{
    char buffer[5];

    (void)state;

    memset(buffer, 'x', sizeof(buffer));
    assert_int_equal(strlcpy(buffer, "abcdef", sizeof(buffer)), 6);
    assert_string_equal(buffer, "abcd");
    assert_int_equal(buffer[4], '\0');
}

/* Verify strlcat reports the length it tried to build. */
static void test_strlcat_reports_truncation(void **state)
{
    char buffer[7] = "abc";

    (void)state;

    assert_int_equal(strlcat(buffer, "defghi", sizeof(buffer)), 9);
    assert_string_equal(buffer, "abcdef");
}

/* Verify zero-sized strlcpy only measures the source. */
static void test_strlcpy_zero_size_only_measures(void **state)
{
    char buffer[1] = {'x'};

    (void)state;

    assert_int_equal(strlcpy(buffer, "abc", 0), 3);
    assert_int_equal(buffer[0], 'x');
}

/* Verify strdup returns a writable copy of the complete string. */
static void test_strdup_copies_complete_string(void **state)
{
    char *copy;

    (void)state;

    copy = strdup("alpha");
    assert_non_null(copy);
    assert_string_equal(copy, "alpha");

    copy[0] = 'A';
    assert_string_equal(copy, "Alpha");

    free(copy);
}

/* Verify strndup copies at most the requested number of bytes. */
static void test_strndup_truncates_to_limit(void **state)
{
    char *copy;

    (void)state;

    copy = strndup("alphabet", 5);
    assert_non_null(copy);
    assert_string_equal(copy, "alpha");

    free(copy);
}

/* Verify strndup stops at the source NUL before the limit. */
static void test_strndup_stops_at_nul(void **state)
{
    char *copy;

    (void)state;

    copy = strndup("abc", 8);
    assert_non_null(copy);
    assert_string_equal(copy, "abc");

    free(copy);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_strlcpy_reports_truncation),
        cmocka_unit_test(test_strlcat_reports_truncation),
        cmocka_unit_test(test_strlcpy_zero_size_only_measures),
        cmocka_unit_test(test_strdup_copies_complete_string),
        cmocka_unit_test(test_strndup_truncates_to_limit),
        cmocka_unit_test(test_strndup_stops_at_nul),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
