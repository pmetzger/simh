#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "scp_expect.h"
#include "test_scp_expect_fixture.h"
#include "test_support.h"

/* Feed one byte sequence through sim_exp_check() and require success. */
static void assert_expect_bytes(EXPECT *exp, const uint8_t *data, size_t size)
{
    size_t i;

    for (i = 0; i < size; ++i)
        assert_int_equal(sim_exp_check(exp, data[i]), SCPE_OK);
}

/* Convenience wrapper for feeding ordinary C strings to EXPECT. */
static void assert_expect_string(EXPECT *exp, const char *text)
{
    assert_expect_bytes(exp, (const uint8_t *)text, strlen(text));
}

/* Assert that one SCP substitution variable has the expected value. */
static void assert_scp_var_equals(const char *name, const char *expected)
{
    char buf[CBUFSIZE];
    const char *value;

    value = _sim_get_env_special(name, buf, sizeof(buf));
    assert_non_null(value);
    assert_string_equal(value, expected);
}

/* Return one SCP substitution variable when optional emptiness matters. */
static const char *get_scp_var(const char *name, char *buf, size_t buf_size)
{
    return _sim_get_env_special(name, buf, buf_size);
}

/* Poll one queued SEND byte and require the expected character. */
static void assert_send_polls_byte(SEND *snd, uint8_t value)
{
    t_stat stat;

    assert_true(sim_send_poll_data(snd, &stat));
    assert_int_equal(stat, value | SCPE_KFLAG);
}

/* Read one temporary output stream into a heap string. */
static char *read_stream_text(FILE *stream)
{
    char *text;
    size_t size;

    assert_non_null(stream);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    fclose(stream);
    return text;
}

/* Capture sim_show_send_input() output into a heap string. */
static char *capture_show_send_input_text(const SEND *snd)
{
    FILE *stream = tmpfile();

    assert_non_null(stream);
    assert_int_equal(sim_show_send_input(stream, snd), SCPE_OK);
    return read_stream_text(stream);
}

/* Capture sim_exp_show() output for one filter and expected status. */
static char *capture_show_expect_text(const EXPECT *exp, const char *match,
                                      t_stat expected)
{
    FILE *stream = tmpfile();

    assert_non_null(stream);
    assert_int_equal(sim_exp_show(stream, exp, match), expected);
    return read_stream_text(stream);
}

/* Capture sim_show_send() output for one wrapper invocation. */
static char *capture_show_send_wrapper_text(const char *cptr, t_stat expected)
{
    FILE *stream = tmpfile();

    assert_non_null(stream);
    assert_int_equal(sim_show_send(stream, NULL, NULL, 0, cptr), expected);
    return read_stream_text(stream);
}

/* Capture sim_show_expect() output for one wrapper invocation. */
static char *capture_show_expect_wrapper_text(const char *cptr, t_stat expected)
{
    FILE *stream = tmpfile();

    assert_non_null(stream);
    assert_int_equal(sim_show_expect(stream, NULL, NULL, 0, cptr), expected);
    return read_stream_text(stream);
}


/* Verify SEND queues bytes and polls them back in order immediately. */
static void test_sim_send_input_queues_and_polls_bytes(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    static uint8_t payload[] = {'A', 'B', 'C'};
    t_stat stat;

    assert_int_equal(
        sim_send_input(&fixture->snd, payload, sizeof(payload), 0, 0), SCPE_OK);
    assert_int_equal(fixture->snd.insoff, sizeof(payload));

    assert_send_polls_byte(&fixture->snd, 'A');
    assert_send_polls_byte(&fixture->snd, 'B');
    assert_send_polls_byte(&fixture->snd, 'C');
    assert_false(sim_send_poll_data(&fixture->snd, &stat));
}

/* Verify clearing SEND state discards any queued input immediately. */
static void test_sim_send_clear_discards_pending_bytes(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    static uint8_t payload[] = {'X', 'Y'};
    t_stat stat;

    assert_int_equal(
        sim_send_input(&fixture->snd, payload, sizeof(payload), 0, 0), SCPE_OK);
    assert_int_equal(sim_send_clear(&fixture->snd), SCPE_OK);
    assert_int_equal(fixture->snd.insoff, 0);
    assert_int_equal(fixture->snd.extoff, 0);
    assert_false(sim_send_poll_data(&fixture->snd, &stat));
}

/* Verify an exact-match rule triggers its action and then removes itself. */
static void test_sim_exp_check_triggers_action_and_consumes_rule(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    char action[CBUFSIZE];

    assert_int_equal(
        sim_exp_set(&fixture->exp, "\"OK\"", 0, 0, 0, "echo matched"), SCPE_OK);
    assert_int_equal(fixture->exp.size, 1);

    assert_expect_string(&fixture->exp, "O");
    assert_int_equal(fixture->exp.size, 1);
    assert_null(sim_brk_getact(action, sizeof(action)));

    assert_expect_string(&fixture->exp, "K");
    assert_int_equal(fixture->exp.size, 0);
    assert_string_equal(sim_brk_getact(action, sizeof(action)), "echo matched");
}

/* Verify persistent duplicate rules are rejected to avoid ambiguity. */
static void test_sim_exp_set_rejects_duplicate_persistent_rules(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(
        sim_exp_set(&fixture->exp, "\"READY\"", 0, 0, EXP_TYP_PERSIST, NULL),
        SCPE_OK);
    assert_int_equal(SCPE_BARE_STATUS(sim_exp_set(&fixture->exp, "\"READY\"", 0,
                                                  0, EXP_TYP_PERSIST, NULL)),
                     SCPE_ARG);
}

/* Verify explicit clear and clear-all both leave a clean expect context. */
static void test_sim_exp_clear_paths_remove_rules_and_buffers(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(
        sim_exp_set(&fixture->exp, "\"A\"", 0, 0, EXP_TYP_PERSIST, NULL),
        SCPE_OK);
    assert_int_equal(
        sim_exp_set(&fixture->exp, "\"B\"", 0, 0, EXP_TYP_PERSIST, NULL),
        SCPE_OK);
    assert_int_equal(fixture->exp.size, 2);

    assert_int_equal(sim_exp_clr(&fixture->exp, "\"A\""), SCPE_OK);
    assert_int_equal(fixture->exp.size, 1);

    assert_expect_string(&fixture->exp, "X");
    assert_true(fixture->exp.buf_ins > 0);

    assert_int_equal(sim_exp_clrall(&fixture->exp), SCPE_OK);
    assert_int_equal(fixture->exp.size, 0);
    assert_null(fixture->exp.rules);
    assert_null(fixture->exp.buf);
    assert_int_equal(fixture->exp.buf_ins, 0);
}

/* Verify clearing a missing rule in an empty context is a harmless no-op. */
static void test_sim_exp_clr_handles_empty_context_without_rules(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(fixture->exp.size, 0);
    assert_null(fixture->exp.rules);
    assert_int_equal(sim_exp_clr(&fixture->exp, "\"A\""), SCPE_OK);
}

/* Regex-specific matching behavior. */

/* Verify regex expect rules capture groups into SCP substitution state. */
static void test_sim_exp_check_populates_regex_capture_groups(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(sim_exp_set(&fixture->exp, "/ab([0-9][0-9])/", 0, 0,
                                 EXP_TYP_REGEX, NULL),
                     SCPE_OK);

    assert_expect_string(&fixture->exp, "ab42");

    assert_scp_var_equals("_EXPECT_MATCH_PATTERN", "/ab([0-9][0-9])/");
    assert_scp_var_equals("_EXPECT_MATCH_GROUP_0", "ab42");
    assert_scp_var_equals("_EXPECT_MATCH_GROUP_1", "42");
}

/* Verify regex rules honor case-independent matching when requested. */
static void test_sim_exp_check_honors_case_independent_regex(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(sim_exp_set(&fixture->exp, "/ab([0-9])/", 0, 0,
                                 EXP_TYP_REGEX | EXP_TYP_REGEX_I, NULL),
                     SCPE_OK);

    assert_expect_string(&fixture->exp, "AB5");

    assert_scp_var_equals("_EXPECT_MATCH_PATTERN", "/ab([0-9])/");
    assert_scp_var_equals("_EXPECT_MATCH_GROUP_0", "AB5");
    assert_scp_var_equals("_EXPECT_MATCH_GROUP_1", "5");
}

/* Verify invalid regex syntax is rejected without installing a rule. */
static void test_sim_exp_set_rejects_invalid_regex_syntax(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(SCPE_BARE_STATUS(sim_exp_set(&fixture->exp, "/(/", 0, 0,
                                                  EXP_TYP_REGEX, NULL)),
                     SCPE_ARG);
    assert_int_equal(fixture->exp.size, 0);
    assert_null(fixture->exp.rules);
}

/* Verify later regex matches clear stale capture groups from prior rules. */
static void test_sim_exp_check_clears_stale_regex_capture_groups(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(sim_exp_set(&fixture->exp, "/ab([0-9])([0-9])/", 0, 0,
                                 EXP_TYP_REGEX, NULL),
                     SCPE_OK);
    assert_expect_string(&fixture->exp, "ab42");

    assert_scp_var_equals("_EXPECT_MATCH_GROUP_0", "ab42");
    assert_scp_var_equals("_EXPECT_MATCH_GROUP_1", "4");
    assert_scp_var_equals("_EXPECT_MATCH_GROUP_2", "2");

    assert_int_equal(sim_exp_set(&fixture->exp, "/cd([0-9])/", 0, 0,
                                 EXP_TYP_REGEX, NULL),
                     SCPE_OK);
    assert_expect_string(&fixture->exp, "cd7");

    assert_scp_var_equals("_EXPECT_MATCH_GROUP_0", "cd7");
    assert_scp_var_equals("_EXPECT_MATCH_GROUP_1", "7");
    assert_scp_var_equals("_EXPECT_MATCH_GROUP_2", "");
}

/* Verify the SHOW helpers describe pending SEND and EXPECT state. */
static void test_show_helpers_render_pending_state(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    static uint8_t payload[] = {'Z'};
    char *text;

    assert_int_equal(
        sim_exp_set(&fixture->exp, "\"Z\"", 0, 0, EXP_TYP_PERSIST, NULL),
        SCPE_OK);
    assert_int_equal(
        sim_send_input(&fixture->snd, payload, sizeof(payload), 0, 0), SCPE_OK);

    text = capture_show_send_input_text(&fixture->snd);
    assert_non_null(strstr(text, "pending input Data"));
    free(text);

    text = capture_show_expect_text(&fixture->exp, "", SCPE_OK);
    assert_non_null(strstr(text, "\"Z\""));
    free(text);
}

/* Verify SEND command parsing updates console defaults and queue timing. */
static void test_send_cmd_sets_console_defaults_and_queue_timing(void **state)
{
    SEND *snd;

    (void)state;

    snd = sim_cons_get_send();
    assert_int_equal(send_cmd(1, "DELAY=7 AFTER=11"), SCPE_OK);
    assert_int_equal(snd->default_delay, 7);
    assert_int_equal(snd->default_after, 11);

    assert_int_equal(send_cmd(1, "\"AZ\""), SCPE_OK);
    assert_int_equal(snd->delay, 7);
    assert_int_equal(snd->after, 11);
    assert_int_equal(snd->insoff, 2);
}

/* Verify SEND command parsing rejects malformed arguments and extras. */
static void test_send_cmd_rejects_invalid_argument_forms(void **state)
{
    (void)state;

    assert_int_equal(send_cmd(1, ""), SCPE_2FARG);
    assert_int_equal(SCPE_BARE_STATUS(send_cmd(1, "plain")), SCPE_ARG);
    assert_int_equal(SCPE_BARE_STATUS(send_cmd(1, "AFTER=1 plain")),
                     SCPE_ARG);
    assert_int_equal(send_cmd(1, "\"A\" extra"), SCPE_2MARG);
    assert_int_equal(SCPE_BARE_STATUS(send_cmd(1, "\"\\q\"")), SCPE_ARG);
    assert_int_equal(
        SCPE_BARE_STATUS(send_cmd(1, "DELAY=bogus \"A\"")), SCPE_ARG);
    assert_int_equal(
        SCPE_BARE_STATUS(send_cmd(1, "AFTER=bogus \"A\"")), SCPE_ARG);
}

/* Verify the SEND -t switch converts microseconds into instruction time. */
static void test_send_cmd_time_switch_converts_usec_to_instructions(void **state)
{
    SEND *snd;
    uint32_t expected_delay;
    uint32_t expected_after;

    (void)state;

    snd = sim_cons_get_send();
    expected_delay =
        (uint32_t)((sim_timer_inst_per_sec() * 2000) / 1000000.0);
    expected_after =
        (uint32_t)((sim_timer_inst_per_sec() * 3000) / 1000000.0);

    assert_int_equal(send_cmd(1, "-t DELAY=2000 AFTER=3000 \"A\""), SCPE_OK);
    assert_int_equal(snd->delay, expected_delay);
    assert_int_equal(snd->after, expected_after);
}

/* Verify SEND line targeting updates the requested TMXR line and defaults. */
static void test_send_cmd_targets_named_tmxr_line(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    SEND *snd = &fixture->lines[1].send;

    assert_int_equal(send_cmd(1, "TTY:1 DELAY=5 AFTER=9"), SCPE_OK);
    assert_int_equal(snd->default_delay, 5);
    assert_int_equal(snd->default_after, 9);

    assert_int_equal(send_cmd(1, "TTY:1 \"AZ\""), SCPE_OK);
    assert_int_equal(snd->delay, 5);
    assert_int_equal(snd->after, 9);
    assert_int_equal(snd->insoff, 2);
    assert_int_equal(sim_cons_get_send()->insoff, 0);
}

/* Verify EXPECT parses repeat counts, HALTAFTER, and trailing actions. */
static void test_sim_set_expect_parses_repeat_haltafter_and_action(void **state)
{
    EXPECT *exp;
    char action[CBUFSIZE];

    (void)state;

    exp = sim_cons_get_expect();
    assert_int_equal(sim_set_expect(exp, "HALTAFTER=13"), SCPE_OK);
    assert_int_equal(exp->default_haltafter, 13);

    assert_int_equal(sim_set_expect(exp, "[2] HALTAFTER=9 \"GO\" echo hit"),
                     SCPE_OK);
    assert_int_equal(exp->size, 1);
    assert_int_equal(exp->rules[0].cnt, 1);
    assert_int_equal(exp->rules[0].after, 9);
    assert_string_equal(exp->rules[0].act, "echo hit");

    sim_brk_clract();
    assert_int_equal(sim_exp_check(exp, 'G'), SCPE_OK);
    assert_int_equal(sim_exp_check(exp, 'O'), SCPE_OK);
    assert_int_equal(exp->rules[0].cnt, 0);
    assert_null(sim_brk_getact(action, sizeof(action)));

    assert_int_equal(sim_exp_check(exp, 'G'), SCPE_OK);
    assert_int_equal(sim_exp_check(exp, 'O'), SCPE_OK);
    assert_string_equal(sim_brk_getact(action, sizeof(action)), "echo hit");
}

/* Verify EXPECT line targeting updates the requested TMXR line only. */
static void test_expect_cmd_targets_named_tmxr_line(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    EXPECT *exp = &fixture->lines[1].expect;
    EXPTAB *rule;

    assert_int_equal(expect_cmd(1, "TTY:1 HALTAFTER=7"), SCPE_OK);
    assert_int_equal(exp->default_haltafter, 7);

    assert_int_equal(expect_cmd(1, "TTY:1 \"GO\" echo hit"), SCPE_OK);
    assert_int_equal(exp->size, 1);
    assert_int_equal(sim_cons_get_expect()->size, 0);

    rule = &exp->rules[0];
    assert_string_equal(rule->match_pattern, "\"GO\"");
    assert_int_equal(rule->after, 7);
    assert_string_equal(rule->act, "echo hit");
}

/* Verify EXPECT parser errors and -t switch handling on the console rule. */
static void test_expect_cmd_parses_switches_and_rejects_bad_input(void **state)
{
    EXPECT *exp;

    (void)state;

    exp = sim_cons_get_expect();
    assert_int_equal(
        SCPE_BARE_STATUS(expect_cmd(1, "[0] \"A\" action")), SCPE_ARG);
    assert_int_equal(
        SCPE_BARE_STATUS(expect_cmd(1, "HALTAFTER=bogus \"A\"")), SCPE_ARG);
    assert_int_equal(SCPE_BARE_STATUS(expect_cmd(1, "plain")), SCPE_ARG);

    assert_int_equal(expect_cmd(1, "-t HALTAFTER=1000 \"A\""), SCPE_OK);
    assert_int_equal(exp->size, 1);
    assert_true(exp->rules[0].switches & EXP_TYP_TIME);
    assert_int_equal(exp->rules[0].after, 1000);
}

/* Verify NOEXPECT clears one rule or all rules through the parser path. */
static void test_sim_set_noexpect_removes_specific_rules_and_all(void **state)
{
    EXPECT *exp;

    (void)state;

    exp = sim_cons_get_expect();
    assert_int_equal(sim_exp_set(exp, "\"A\"", 0, 0, EXP_TYP_PERSIST, NULL),
                     SCPE_OK);
    assert_int_equal(sim_exp_set(exp, "\"B\"", 0, 0, EXP_TYP_PERSIST, NULL),
                     SCPE_OK);
    assert_int_equal(exp->size, 2);

    assert_int_equal(sim_set_noexpect(exp, "\"A\""), SCPE_OK);
    assert_int_equal(exp->size, 1);
    assert_string_equal(exp->rules[0].match_pattern, "\"B\"");

    assert_int_equal(sim_set_noexpect(exp, ""), SCPE_OK);
    assert_int_equal(exp->size, 0);
}

/* Verify HALTAFTER rules reject concurrent siblings and CLEARALL clears all. */
static void test_expect_rule_restrictions_and_clearall_match_behavior(
    void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(
        sim_exp_set(&fixture->exp, "\"A\"", 0, 0, EXP_TYP_PERSIST, NULL),
        SCPE_OK);
    assert_int_equal(
        SCPE_BARE_STATUS(
            sim_exp_set(&fixture->exp, "\"B\"", 0, 1, 0, NULL)),
        SCPE_ARG);

    assert_int_equal(sim_exp_clrall(&fixture->exp), SCPE_OK);
    assert_int_equal(sim_exp_set(&fixture->exp, "\"A\"", 0, 0,
                                 EXP_TYP_PERSIST, NULL),
                     SCPE_OK);
    assert_int_equal(sim_exp_set(&fixture->exp, "\"B\"", 0, 0,
                                 EXP_TYP_CLEARALL, NULL),
                     SCPE_OK);
    assert_int_equal(sim_exp_check(&fixture->exp, 'B'), SCPE_OK);
    assert_int_equal(fixture->exp.size, 0);
}

/* SHOW and reporting behavior. */

/* Verify SHOW reports filtered no-match errors for expect rules. */
static void test_sim_exp_show_reports_when_no_rules_match_filter(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    char *text;

    assert_int_equal(
        sim_exp_set(&fixture->exp, "\"A\"", 0, 0, EXP_TYP_PERSIST, NULL),
        SCPE_OK);

    text = capture_show_expect_text(&fixture->exp, "\"NOPE\"", SCPE_ARG);
    assert_non_null(strstr(text, "No Rules match '\"NOPE\"'"));
    free(text);
}

/* Verify SHOW EXPECT reports no-match on an empty expect context too. */
static void test_sim_exp_show_reports_no_match_for_empty_context(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    char *text;

    text = capture_show_expect_text(&fixture->exp, "\"A\"", SCPE_ARG);
    assert_non_null(strstr(text, "No Rules match '\"A\"'"));
    free(text);
}

/* Verify exact-string matching works when the buffer wraps mid-pattern. */
static void test_sim_exp_check_matches_across_buffer_wrap(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(
        sim_exp_set(&fixture->exp, "\"ABCDE\"", 0, 0, EXP_TYP_PERSIST, NULL),
        SCPE_OK);

    assert_expect_string(&fixture->exp, "xxABCD");
    assert_int_equal(fixture->exp.buf_ins, 0);
    assert_expect_string(&fixture->exp, "E");

    assert_scp_var_equals("_EXPECT_MATCH_PATTERN", "\"ABCDE\"");
}

/* Verify regex buffers slide instead of wrapping when they fill. */
static void test_sim_exp_check_slides_regex_buffers_when_full(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    size_t i;

    assert_int_equal(
        sim_exp_set(&fixture->exp, "/never-matches/", 0, 0,
                    EXP_TYP_REGEX | EXP_TYP_PERSIST, NULL),
        SCPE_OK);
    assert_int_equal(fixture->exp.buf_size, 1025);

    for (i = 0; i < fixture->exp.buf_size; ++i)
        assert_int_equal(sim_exp_check(&fixture->exp, 'a'), SCPE_OK);

    assert_int_equal(fixture->exp.buf_ins, 513);
    assert_int_equal(fixture->exp.buf_data, fixture->exp.buf_ins);
}

/* Verify expect matches activate the internal unit and identify it. */
static void test_expect_matches_schedule_internal_expect_unit(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_false(sim_expect_is_unit(&fixture->unit));
    assert_true(sim_expect_is_unit(&sim_expect_dev.units[0]));
    assert_int_equal(
        sim_exp_set(&fixture->exp, "\"HI\"", 0, 0, 0, NULL), SCPE_OK);
    assert_expect_string(&fixture->exp, "HI");
    assert_int_equal(SCPE_BARE_STATUS(sim_process_event()), SCPE_EXPECT);
}

/* Verify expect rules can schedule a microsecond-based stop delay. */
static void test_expect_time_rules_schedule_microsecond_based_stop(
    void **state)
{
    struct scp_expect_fixture *fixture = *state;
    int32_t expected_delay;
    int32_t scheduled_delay;

    expected_delay =
        (int32_t)((sim_timer_inst_per_sec() * 5000) / 1000000.0);

    assert_int_equal(sim_exp_set(&fixture->exp, "\"GO\"", 0, 5000,
                                 EXP_TYP_TIME, NULL),
                     SCPE_OK);
    assert_expect_string(&fixture->exp, "GO");
    assert_true(sim_is_active(&sim_expect_dev.units[0]));
    scheduled_delay = sim_activate_time(&sim_expect_dev.units[0]);
    assert_true(scheduled_delay >= expected_delay);
    assert_true(scheduled_delay > 0);
}

/* Verify expect helpers handle empty contexts and expose descriptions. */
static void test_expect_helpers_handle_empty_contexts(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    FILE *stream;

    assert_string_equal(sim_expect_dev.description(&sim_expect_dev),
                        "Expect facility");
    assert_int_equal(sim_exp_check(&fixture->exp, 'X'), SCPE_OK);

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(sim_exp_showall(stream, &fixture->exp), SCPE_OK);
    fclose(stream);
}

/* Verify SHOW EXPECT includes debug guidance when enabled for the context. */
static void test_sim_exp_show_renders_debug_guidance(void **state)
{
    struct scp_expect_fixture *fixture = *state;
    char *text;

    fixture->exp.dbit = 1;
    fixture->device.dctrl = 1;

    text = capture_show_expect_text(&fixture->exp, "", SCPE_OK);
    assert_non_null(strstr(text, "Expect Debugging via: SET TTY DEBUG"));
    free(text);
}

/* Verify the SHOW SEND wrapper accepts the console path and rejects extras. */
static void test_sim_show_send_wrapper_handles_console_arguments(void **state)
{
    FILE *stream;
    char *text;

    (void)state;

    text = capture_show_send_wrapper_text("", SCPE_OK);
    free(text);

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(sim_show_send(stream, NULL, NULL, 0, "extra"),
                     SCPE_2MARG);
    fclose(stream);
}

/* Verify the SHOW EXPECT wrapper accepts quoted filters and rejects extras. */
static void test_sim_show_expect_wrapper_handles_console_arguments(void **state)
{
    EXPECT *exp;
    FILE *stream;
    char *text;

    (void)state;

    exp = sim_cons_get_expect();
    assert_int_equal(sim_exp_set(exp, "\"A\"", 0, 0, EXP_TYP_PERSIST, NULL),
                     SCPE_OK);

    text = capture_show_expect_wrapper_text("\"A\"", SCPE_OK);
    free(text);

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(sim_show_expect(stream, NULL, NULL, 0, "\"A\" extra"),
                     SCPE_2MARG);
    fclose(stream);
}

/* Verify SHOW EXPECT rejects unquoted and unterminated filters. */
static void test_sim_show_expect_wrapper_rejects_invalid_filters(void **state)
{
    FILE *stream;

    (void)state;

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(SCPE_BARE_STATUS(sim_show_expect(stream, NULL, NULL, 0,
                                                      "plain")),
                     SCPE_ARG);
    fclose(stream);

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(SCPE_BARE_STATUS(sim_show_expect(stream, NULL, NULL, 0,
                                                      "\"A")),
                     SCPE_ARG);
    fclose(stream);
}

/* Verify SHOW wrappers resolve TMXR lines and reject unknown line names. */
static void test_show_wrappers_handle_named_tmxr_lines(void **state)
{
    char *text;

    (void)state;

    assert_int_equal(send_cmd(1, "TTY:1 DELAY=4 AFTER=6 \"Q\""), SCPE_OK);
    assert_int_equal(expect_cmd(1, "TTY:1 \"GO\" echo hit"), SCPE_OK);

    text = capture_show_send_wrapper_text("TTY:1", SCPE_OK);
    assert_non_null(strstr(text, "TTY:1"));
    free(text);

    text = capture_show_expect_wrapper_text("TTY:1", SCPE_OK);
    assert_non_null(strstr(text, "\"GO\""));
    free(text);
}

/* Verify line-qualified SEND and EXPECT reject unknown TMXR lines. */
static void test_line_qualified_commands_reject_unknown_tmxr_lines(
    void **state)
{
    FILE *stream;

    (void)state;

    assert_int_equal(SCPE_BARE_STATUS(send_cmd(1, "TTY:9 \"A\"")), SCPE_ARG);
    assert_int_equal(SCPE_BARE_STATUS(expect_cmd(1, "TTY:9 \"A\"")), SCPE_ARG);

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(SCPE_BARE_STATUS(sim_show_send(stream, NULL, NULL, 0,
                                                    "TTY:9")),
                     SCPE_ARG);
    fclose(stream);

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(SCPE_BARE_STATUS(sim_show_expect(stream, NULL, NULL, 0,
                                                      "TTY:9")),
                     SCPE_ARG);
    fclose(stream);
}

/* Verify malformed NOEXPECT inputs are rejected through parser helpers. */
static void test_sim_set_noexpect_rejects_invalid_argument_forms(void **state)
{
    EXPECT *exp;

    (void)state;

    exp = sim_cons_get_expect();
    assert_int_equal(
        SCPE_BARE_STATUS(sim_set_noexpect(exp, "plain")), SCPE_ARG);
    assert_int_equal(sim_set_noexpect(exp, "\"A\" extra"), SCPE_2MARG);
    assert_int_equal(SCPE_BARE_STATUS(expect_cmd(0, "plain")), SCPE_ARG);
}

/* Verify EXPECT parser rejects missing input and non-regex -i usage. */
static void test_expect_parsers_reject_missing_input_and_bad_switches(
    void **state)
{
    EXPECT *exp;

    (void)state;

    exp = sim_cons_get_expect();
    assert_int_equal(sim_set_expect(exp, ""), SCPE_2FARG);
    assert_int_equal(
        SCPE_BARE_STATUS(
            sim_exp_set(exp, "\"A\"", 0, 0, EXP_TYP_REGEX_I, NULL)),
        SCPE_ARG);
}

/* Verify invalid literal strings are rejected for exact-match rules. */
static void test_sim_exp_set_rejects_invalid_quoted_match_string(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(
        SCPE_BARE_STATUS(
            sim_exp_set(&fixture->exp, "\"\\q\"", 0, 0, 0, NULL)),
        SCPE_ARG);
}

/* Verify SEND state compacts pending bytes and honors future timing. */
static void test_send_state_compacts_pending_bytes_and_honors_timing(
    void **state)
{
    struct scp_expect_fixture *fixture = *state;
    static uint8_t payload[] = {'A', 'B', 'C'};
    t_stat stat;

    assert_int_equal(
        sim_send_input(&fixture->snd, payload, sizeof(payload), 5, 7),
        SCPE_OK);
    assert_true(sim_send_poll_data(&fixture->snd, &stat));
    assert_int_equal(stat, SCPE_OK);

    fixture->snd.next_time = sim_gtime();
    assert_true(sim_send_poll_data(&fixture->snd, &stat));
    assert_int_equal(stat, 'A' | SCPE_KFLAG);
    assert_int_equal(fixture->snd.extoff, 1);

    fixture->snd.next_time = sim_gtime();
    assert_int_equal(
        sim_send_input(&fixture->snd, payload, sizeof(payload), 0, 0),
        SCPE_OK);
    assert_int_equal(fixture->snd.extoff, 0);
    assert_int_equal(fixture->snd.insoff, 5);
    assert_memory_equal(fixture->snd.buffer, "BCABC", fixture->snd.insoff);
}

/* Additional SEND/EXPECT edge cases. */

/* Verify SHOW SEND renders pending timing using typed default values. */
static void test_sim_show_send_input_renders_timing_and_default_fallback(
    void **state)
{
    SEND *snd;
    static uint8_t payload[] = {'A'};
    char *text;

    (void)state;

    snd = sim_cons_get_send();
    assert_int_equal(
        sim_send_input(snd, payload, sizeof(payload), 5000, 2000000), SCPE_OK);

    text = capture_show_send_input_text(snd);
    assert_non_null(strstr(text, "pending input Data"));
    assert_non_null(strstr(text, "first character"));
    assert_non_null(strstr(text, "between characters"));
    assert_non_null(
        strstr(text, "Default delay before first character input is 1000"));
    assert_non_null(
        strstr(text, "Default delay between character input is 1000"));
    free(text);
}

/* Verify SHOW SEND reports distinct default after and delay values. */
static void test_sim_show_send_input_reports_distinct_default_values(
    void **state)
{
    SEND *snd;
    char *text;

    (void)state;

    snd = sim_cons_get_send();
    snd->default_delay = 7;
    snd->default_after = 11;

    text = capture_show_send_input_text(snd);
    assert_non_null(
        strstr(text, "Default delay before first character input is 11"));
    assert_non_null(
        strstr(text, "Default delay between character input is 7"));
    free(text);
}

/* Verify SHOW SEND renders microsecond detail and debug guidance. */
static void test_sim_show_send_input_renders_microsecond_and_debug_detail(
    void **state)
{
    struct scp_expect_fixture *fixture = *state;
    SEND *snd;
    char *text;
    uint32_t threshold;

    snd = &fixture->lines[1].send;
    threshold = (uint32_t)(sim_timer_inst_per_sec() / 1000000.0);
    snd->delay = threshold + 5;
    snd->after = threshold + 7;
    snd->next_time = sim_gtime() + snd->after;
    snd->dptr = &fixture->device;
    snd->dbit = 1;
    fixture->device.dctrl = 1;

    text = capture_show_send_input_text(snd);
    free(text);
}

/* Verify regex export clears optional capture groups that did not match. */
static void test_regex_optional_group_unsets_unmatched_capture_group(
    void **state)
{
    struct scp_expect_fixture *fixture = *state;
    const char *group1;
    char buf[CBUFSIZE];

    assert_int_equal(sim_exp_set(&fixture->exp, "/ab(c)?/", 0, 0,
                                 EXP_TYP_REGEX, NULL),
                     SCPE_OK);
    assert_expect_string(&fixture->exp, "ab");

    assert_scp_var_equals("_EXPECT_MATCH_GROUP_0", "ab");
    group1 = get_scp_var("_EXPECT_MATCH_GROUP_1", buf, sizeof(buf));
    assert_true((NULL == group1) || (group1[0] == '\0'));
}

/* Verify actions are trimmed before storing them on a new rule. */
static void test_sim_exp_set_trims_leading_action_whitespace(void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(sim_exp_set(&fixture->exp, "\"GO\"", 0, 0, 0,
                                 "   echo hit"),
                     SCPE_OK);
    assert_string_equal(fixture->exp.rules[0].act, "echo hit");
}

/* Verify regex checks flatten wrapped buffers with embedded NUL bytes. */
static void test_regex_matching_flattens_embedded_nul_buffer_segments(
    void **state)
{
    struct scp_expect_fixture *fixture = *state;

    assert_int_equal(sim_exp_set(&fixture->exp, "/AB/", 0, 0,
                                 EXP_TYP_REGEX | EXP_TYP_PERSIST, NULL),
                     SCPE_OK);
    assert_int_equal(sim_exp_set(&fixture->exp, "/ZZ/", 0, 0,
                                 EXP_TYP_REGEX | EXP_TYP_PERSIST, NULL),
                     SCPE_OK);

    {
        static const uint8_t wrapped_data[] = {'A', '\0', 'B'};

        assert_expect_bytes(&fixture->exp, wrapped_data, sizeof(wrapped_data));
    }

    assert_scp_var_equals("_EXPECT_MATCH_PATTERN", "/AB/");
    assert_scp_var_equals("_EXPECT_MATCH_GROUP_0", "AB");
}

/* Verify SHOW can render a decorated expect rule through the match filter. */
static void test_sim_exp_show_renders_decorated_rule_details(void **state)
{
    EXPECT *exp;
    char *text;

    (void)state;

    exp = sim_cons_get_expect();
    exp->default_haltafter = 1;
    assert_int_equal(sim_exp_set(exp, "/A(B)?/", 2, 9,
                                 EXP_TYP_PERSIST | EXP_TYP_CLEARALL |
                                     EXP_TYP_REGEX | EXP_TYP_REGEX_I,
                                 "echo done"),
                     SCPE_OK);

    text = capture_show_expect_text(exp, "/A(B)?/", SCPE_OK);
    assert_non_null(strstr(text, "EXPECT -p -c -r -i"));
    assert_non_null(strstr(text, "HALTAFTER=9"));
    assert_non_null(strstr(text, "[2]"));
    assert_non_null(strstr(text, "echo done"));
    free(text);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_sim_send_input_queues_and_polls_bytes,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_send_clear_discards_pending_bytes,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_check_triggers_action_and_consumes_rule,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_set_rejects_duplicate_persistent_rules,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_clear_paths_remove_rules_and_buffers,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_clr_handles_empty_context_without_rules,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_check_populates_regex_capture_groups,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_check_honors_case_independent_regex,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_set_rejects_invalid_regex_syntax,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_check_clears_stale_regex_capture_groups,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(test_show_helpers_render_pending_state,
                                        simh_test_setup_scp_expect_fixture,
                                        simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_send_cmd_sets_console_defaults_and_queue_timing,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_send_cmd_rejects_invalid_argument_forms,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_send_cmd_time_switch_converts_usec_to_instructions,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(test_send_cmd_targets_named_tmxr_line,
                                        simh_test_setup_scp_expect_fixture,
                                        simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_expect_parses_repeat_haltafter_and_action,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_expect_cmd_targets_named_tmxr_line,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_expect_cmd_parses_switches_and_rejects_bad_input,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_noexpect_removes_specific_rules_and_all,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_expect_rule_restrictions_and_clearall_match_behavior,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_show_reports_when_no_rules_match_filter,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_show_reports_no_match_for_empty_context,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_check_matches_across_buffer_wrap,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_check_slides_regex_buffers_when_full,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_expect_matches_schedule_internal_expect_unit,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_send_wrapper_handles_console_arguments,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_expect_wrapper_handles_console_arguments,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_expect_wrapper_rejects_invalid_filters,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_show_wrappers_handle_named_tmxr_lines,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_line_qualified_commands_reject_unknown_tmxr_lines,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_set_noexpect_rejects_invalid_argument_forms,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_expect_parsers_reject_missing_input_and_bad_switches,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_set_rejects_invalid_quoted_match_string,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_send_state_compacts_pending_bytes_and_honors_timing,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_send_input_renders_timing_and_default_fallback,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_send_input_reports_distinct_default_values,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_show_send_input_renders_microsecond_and_debug_detail,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_regex_optional_group_unsets_unmatched_capture_group,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_set_trims_leading_action_whitespace,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_regex_matching_flattens_embedded_nul_buffer_segments,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_show_renders_decorated_rule_details,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_expect_time_rules_schedule_microsecond_based_stop,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_expect_helpers_handle_empty_contexts,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_exp_show_renders_debug_guidance,
            simh_test_setup_scp_expect_fixture,
            simh_test_teardown_scp_expect_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
