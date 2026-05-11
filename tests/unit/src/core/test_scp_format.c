#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"

/* Verify quoted strings preserve printable text and escape controls. */
static void test_sim_encode_quoted_string_escapes_control_bytes(void **state)
{
    const uint8_t input[] = {'A', '\n', 1, '\\'};
    char *encoded;

    (void)state;

    encoded = sim_encode_quoted_string(input, sizeof(input));

    assert_non_null(encoded);
    assert_string_equal(encoded, "\"A\\n\\001\\\\\"");

    free(encoded);
}

/* Verify value formatting keeps comma and sign placement stable. */
static void test_sprint_val_formats_comma_signed_values(void **state)
{
    char buffer[MAX_WIDTH + 1];

    (void)state;

    assert_int_equal(
        sprint_val(buffer, (t_value)(t_svalue)-1234567, 10, 10, PV_RCOMMASIGN),
        SCPE_OK);
    assert_string_equal(buffer, "-1,234,567");
}

/* Verify elapsed-time strings preserve existing singular/plural behavior. */
static void test_sim_fmt_secs_formats_subsecond_and_whole_seconds(void **state)
{
    (void)state;

    assert_string_equal(sim_fmt_secs(0.0), "");
    assert_string_equal(sim_fmt_secs(0.000001), "1 usec");
    assert_string_equal(sim_fmt_secs(0.001), "1 msec");
    assert_string_equal(sim_fmt_secs(1.0), "1 second");
    assert_string_equal(sim_fmt_secs(2.0), "2 seconds");
}

/* Verify numeric grouping remains unchanged. */
static void test_sim_fmt_numeric_inserts_decimal_commas(void **state)
{
    (void)state;

    assert_string_equal(sim_fmt_numeric(0.0), "0");
    assert_string_equal(sim_fmt_numeric(1234567.0), "1,234,567");
    assert_string_equal(sim_fmt_numeric(-1234567.0), "-1,234,567");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sim_encode_quoted_string_escapes_control_bytes),
        cmocka_unit_test(test_sprint_val_formats_comma_signed_values),
        cmocka_unit_test(test_sim_fmt_secs_formats_subsecond_and_whole_seconds),
        cmocka_unit_test(test_sim_fmt_numeric_inserts_decimal_commas),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
