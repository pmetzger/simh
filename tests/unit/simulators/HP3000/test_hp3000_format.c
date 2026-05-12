#include <stdio.h>
#include <string.h>

#include "test_cmocka.h"

#include "hp3000_defs.h"

static BITSET_NAME lsb_names[] = {"bit0", NULL, "bit2"};
static const BITSET_FORMAT lsb_format = {
    FMT_INIT(lsb_names, 0, lsb_first, no_alt, no_bar)};

static BITSET_NAME msb_names[] = {"high", "middle", "low"};
static const BITSET_FORMAT msb_format = {
    FMT_INIT(msb_names, 2, msb_first, no_alt, no_bar)};

static BITSET_NAME alternate_names[] = {"\1load\0store", "\1ready\0busy"};
static const BITSET_FORMAT alternate_format = {
    FMT_INIT(alternate_names, 0, lsb_first, has_alt, no_bar)};

static BITSET_NAME trailing_names[] = {"flag"};
static const BITSET_FORMAT trailing_format = {
    FMT_INIT(trailing_names, 0, lsb_first, no_alt, append_bar)};

static BITSET_NAME high_bit_names[] = {"sign"};
static const BITSET_FORMAT high_bit_format = {
    FMT_INIT(high_bit_names, 31, lsb_first, no_alt, no_bar)};

static void test_fmt_char_formats_representative_values(void **state)
{
    char buffer[FMT_CHAR_SIZE];

    (void)state;

    assert_string_equal(fmt_char_buf(buffer, sizeof(buffer), 0000), "NUL");
    assert_string_equal(fmt_char_buf(buffer, sizeof(buffer), 0177), "DEL");
    assert_string_equal(fmt_char_buf(buffer, sizeof(buffer), 'A'), "'A'");
    assert_string_equal(fmt_char_buf(buffer, sizeof(buffer), 0200), "\\200");
    assert_string_equal(fmt_char_buf(buffer, sizeof(buffer), 0400), "\\000");
}

static void test_fmt_bitset_formats_representative_values(void **state)
{
    char buffer[FMT_BITSET_SIZE];

    (void)state;

    assert_string_equal(
        fmt_bitset_buf(buffer, sizeof(buffer), 0005, lsb_format),
        "bit0 | bit2");
    assert_string_equal(
        fmt_bitset_buf(buffer, sizeof(buffer), 0024, msb_format), "high | low");
    assert_string_equal(
        fmt_bitset_buf(buffer, sizeof(buffer), 0001, alternate_format),
        "load | busy");
    assert_string_equal(
        fmt_bitset_buf(buffer, sizeof(buffer), 0000, lsb_format), "(none)");
    assert_string_equal(
        fmt_bitset_buf(buffer, sizeof(buffer), 0000, trailing_format), "");
    assert_string_equal(
        fmt_bitset_buf(buffer, sizeof(buffer), 0001, trailing_format),
        "flag | ");
    assert_string_equal(
        fmt_bitset_buf(buffer, sizeof(buffer), 0x80000000u, high_bit_format),
        "sign");
}

static void test_format_macros_provide_independent_call_buffers(void **state)
{
    char combined[64];

    (void)state;

    snprintf(combined, sizeof(combined), "%s/%s/%s", fmt_char('A'),
             fmt_char('B'), fmt_bitset(0005, lsb_format));

    assert_string_equal(combined, "'A'/'B'/bit0 | bit2");
}

static void test_fmt_bitset_reports_buffer_overflow(void **state)
{
    BITSET_NAME long_names[] = {"abcdef"};
    const BITSET_FORMAT long_format = {
        FMT_INIT(long_names, 0, lsb_first, no_alt, no_bar)};
    char buffer[6];

    (void)state;

    assert_string_equal(
        fmt_bitset_buf(buffer, sizeof(buffer), 0001, long_format),
        "(buffer overflow)");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_fmt_char_formats_representative_values),
        cmocka_unit_test(test_fmt_bitset_formats_representative_values),
        cmocka_unit_test(test_format_macros_provide_independent_call_buffers),
        cmocka_unit_test(test_fmt_bitset_reports_buffer_overflow),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
