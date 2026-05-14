#include <stdio.h>
#include <string.h>

#include "test_cmocka.h"

#include "string_util.h"

/* Verify formatted append preserves existing text and appends new text. */
static void test_strlappendf_appends_formatted_text(void **state)
{
    char buf[32] = "alpha";

    (void)state;

    assert_int_equal(strlappendf(buf, sizeof(buf), "-%s-%d", "beta", 7),
                     strlen("alpha-beta-7"));
    assert_string_equal(buf, "alpha-beta-7");
}

/* Verify truncation leaves a NUL-terminated prefix and reports full length. */
static void test_strlappendf_reports_truncation(void **state)
{
    char buf[10] = "alpha";

    (void)state;

    assert_int_equal(strlappendf(buf, sizeof(buf), "-%s", "betagamma"),
                     strlen("alpha-betagamma"));
    assert_string_equal(buf, "alpha-bet");
}

/* Verify no append room reports truncation and leaves content unchanged. */
static void test_strlappendf_reports_no_append_room(void **state)
{
    char buf[6] = "alpha";

    (void)state;

    assert_int_equal(strlappendf(buf, sizeof(buf), "-%s", "beta"),
                     strlen("alpha-beta"));
    assert_string_equal(buf, "alpha");
}

/* Verify appending an empty formatted fragment is a no-op. */
static void test_strlappendf_accepts_empty_append(void **state)
{
    char buf[16] = "alpha";

    (void)state;

    assert_int_equal(strlappendf(buf, sizeof(buf), "%s", ""), 5);
    assert_string_equal(buf, "alpha");
}

/* Verify zero-sized output only measures the formatted append. */
static void test_strlappendf_zero_size_only_measures(void **state)
{
    (void)state;

    assert_int_equal(strlappendf(NULL, 0, "%s-%d", "alpha", 7),
                     strlen("alpha-7"));
}

/* Verify a buffer without a NUL inside its bound is not modified. */
static void test_strlappendf_handles_unterminated_buffer(void **state)
{
    char buf[5] = {'a', 'l', 'p', 'h', 'a'};

    (void)state;

    assert_int_equal(strlappendf(buf, sizeof(buf), "-%s", "beta"),
                     strlen("alpha-beta"));
    assert_memory_equal(buf, "alpha", sizeof(buf));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_strlappendf_appends_formatted_text),
        cmocka_unit_test(test_strlappendf_reports_truncation),
        cmocka_unit_test(test_strlappendf_reports_no_append_room),
        cmocka_unit_test(test_strlappendf_accepts_empty_append),
        cmocka_unit_test(test_strlappendf_zero_size_only_measures),
        cmocka_unit_test(test_strlappendf_handles_unterminated_buffer),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
