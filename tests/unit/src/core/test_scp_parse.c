#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "scp_parse.h"

/* Verify plain glyph parsing uppercases tokens and skips leading space. */
static void test_get_glyph_uppercases_and_skips_spaces(void **state)
{
    char token[CBUFSIZE];
    const char *rest;

    (void)state;

    rest = get_glyph("alpha  beta", token, 0);

    assert_string_equal(token, "ALPHA");
    assert_string_equal(rest, "beta");
}

/* Verify the no-conversion glyph helper preserves original letter case. */
static void test_get_glyph_nc_preserves_case(void **state)
{
    char token[CBUFSIZE];
    const char *rest;

    (void)state;

    rest = get_glyph_nc("MiXeD,next", token, ',');

    assert_string_equal(token, "MiXeD");
    assert_string_equal(rest, "next");
}

/* Verify quoted glyph parsing keeps a quoted token together. */
static void test_get_glyph_quoted_keeps_quoted_token_intact(void **state)
{
    char token[CBUFSIZE];
    const char *rest;

    (void)state;

    rest = get_glyph_quoted("\"hello world\" tail", token, 0);

    assert_string_equal(token, "\"hello world\"");
    assert_string_equal(rest, "tail");
}

/* Verify quoted glyph parsing preserves escaped quote content. */
static void test_get_glyph_quoted_preserves_escaped_quotes(void **state)
{
    char token[CBUFSIZE];
    const char *rest;

    (void)state;

    rest = get_glyph_quoted("\"say \\\"hi\\\"\", next", token, ',');

    assert_string_equal(token, "\"say \\\"hi\\\"\"");
    assert_string_equal(rest, "next");
}

/* Verify command glyph parsing breaks out a leading shell bang. */
static void test_get_glyph_cmd_handles_bang_prefix(void **state)
{
    char token[CBUFSIZE];
    const char *rest;

    (void)state;

    rest = get_glyph_cmd("!echo test", token);

    assert_string_equal(token, "!");
    assert_string_equal(rest, "echo test");
}

/* Verify a shell bang followed by space is parsed as a normal token. */
static void test_get_glyph_cmd_treats_spaced_bang_as_normal_token(void **state)
{
    char token[CBUFSIZE];
    const char *rest;

    (void)state;

    rest = get_glyph_cmd("! echo", token);

    assert_string_equal(token, "!");
    assert_string_equal(rest, "echo");
}

/* Verify switch parsing handles both bitmask and numeric switch forms. */
static void test_get_switches_parses_bitmask_and_number(void **state)
{
    int32_t sw;
    int32_t number;

    (void)state;

    assert_int_equal(get_switches("-qr", &sw, &number), SW_BITMASK);
    assert_int_equal(sw, SWMASK('Q') | SWMASK('R'));

    assert_int_equal(get_switches("-16", &sw, &number), SW_NUMBER);
    assert_int_equal(number, 16);
}

/* Verify switch parsing rejects mixed or otherwise invalid syntax. */
static void test_get_switches_rejects_invalid_switches(void **state)
{
    int32_t sw;
    int32_t number;

    (void)state;

    assert_int_equal(get_switches("-q1", &sw, &number), SW_ERROR);
    assert_int_equal(get_switches("-?", &sw, &number), SW_ERROR);
}

/* Verify leading simulator switches are consumed and stored globally. */
static void test_get_sim_sw_consumes_switch_tokens(void **state)
{
    const char *rest;

    (void)state;

    sim_switches = 0;
    sim_switch_number = 0;

    rest = get_sim_sw("-qr -16 command tail");

    assert_non_null(rest);
    assert_int_equal(sim_switches, SWMASK('Q') | SWMASK('R'));
    assert_int_equal(sim_switch_number, 16);
    assert_string_equal(rest, "command tail");
}

/* Verify invalid simulator switches abort parsing without side effects. */
static void test_get_sim_sw_returns_null_on_invalid_switches(void **state)
{
    (void)state;

    sim_switches = 0;
    sim_switch_number = 0;

    assert_null(get_sim_sw("-q1 command"));
    assert_int_equal(sim_switches, 0);
    assert_int_equal(sim_switch_number, 0);
}

/* Verify multiple switch groups accumulate until the command body begins. */
static void test_get_sim_sw_accumulates_multiple_switch_groups(void **state)
{
    const char *rest;

    (void)state;

    sim_switches = 0;
    sim_switch_number = 0;

    rest = get_sim_sw("-qr -16 -ab command tail");

    assert_non_null(rest);
    assert_int_equal(sim_switches,
                     SWMASK('Q') | SWMASK('R') | SWMASK('A') | SWMASK('B'));
    assert_int_equal(sim_switch_number, 16);
    assert_string_equal(rest, "command tail");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_get_glyph_uppercases_and_skips_spaces),
        cmocka_unit_test(test_get_glyph_nc_preserves_case),
        cmocka_unit_test(test_get_glyph_quoted_keeps_quoted_token_intact),
        cmocka_unit_test(test_get_glyph_quoted_preserves_escaped_quotes),
        cmocka_unit_test(test_get_glyph_cmd_handles_bang_prefix),
        cmocka_unit_test(test_get_glyph_cmd_treats_spaced_bang_as_normal_token),
        cmocka_unit_test(test_get_switches_parses_bitmask_and_number),
        cmocka_unit_test(test_get_switches_rejects_invalid_switches),
        cmocka_unit_test(test_get_sim_sw_consumes_switch_tokens),
        cmocka_unit_test(test_get_sim_sw_returns_null_on_invalid_switches),
        cmocka_unit_test(test_get_sim_sw_accumulates_multiple_switch_groups),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
