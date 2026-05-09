/* scp_expect.c: SCP expect/send subsystem

   This file contains the SCP support for queued console input injection
   and output-triggered expect rules.  The subsystem owns the SEND/EXPECT
   command handlers, the expect rule matcher, and the internal device used
   to stop simulation when an expect rule fires.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include "sim_defs.h"
#include "scp.h"
#include "scp_pcre2.h"
#include "sim_tmxr.h"

/* Initialize one SEND context with the standard default timing values. */
void sim_send_init_context(SEND *snd, DEVICE *dptr, uint32 dbit)
{
    snd->dptr = dptr;
    snd->dbit = dbit;
    snd->default_delay = SEND_DEFAULT_DELAY;
    snd->default_after = SEND_DEFAULT_DELAY;
    snd->delay = 0;
    snd->after = 0;
    snd->next_time = 0.0;
}

/* Initialize one EXPECT context with the standard default halt value. */
void sim_expect_init_context(EXPECT *exp, DEVICE *dptr, uint32 dbit)
{
    exp->dptr = dptr;
    exp->dbit = dbit;
    exp->default_haltafter = 0;
}

/* Describe the internal expect device in SHOW output. */
static const char *sim_int_expect_description(DEVICE *dptr)
{
    (void)dptr;
    return "Expect facility";
}

/* Stop simulation when an expect rule fires. */
static t_stat expect_svc(UNIT *uptr)
{
    (void)uptr;
    return SCPE_EXPECT | (scp_do_echo_enabled() ? 0 : SCPE_NOMESSAGE);
}

static UNIT sim_expect_unit = {UDATA(&expect_svc, 0, 0)};

DEVICE sim_expect_dev = {"INT-EXPECT",
                         &sim_expect_unit,
                         NULL,
                         NULL,
                         1,
                         0,
                         0,
                         0,
                         0,
                         0,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         DEV_NOSAVE,
                         0,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         sim_int_expect_description};

bool sim_expect_is_unit(const UNIT *uptr)
{
    return uptr == &sim_expect_unit;
}

/* Search one expect context for a matching display-format pattern. */
static const EXPTAB *sim_exp_fnd(const EXPECT *exp, const char *match,
                                 size_t start_rule)
{
    size_t i;

    if (NULL == exp->rules)
        return NULL;

    for (i = start_rule; i < exp->size; i++)
        if (!strcmp(exp->rules[i].match_pattern, match))
            return &exp->rules[i];

    return NULL;
}

/* Release one compiled regex rule and reset its metadata. */
static void sim_exp_free_regex_rule(EXPTAB *ep)
{
    if ((NULL != ep) && (ep->switches & EXP_TYP_REGEX) && (NULL != ep->regex))
        pcre2_code_free(ep->regex);
    if (NULL != ep) {
        ep->regex = NULL;
        ep->re_nsub = 0;
    }
}

/* Decode a slash-delimited regex pattern body into a temporary buffer. */
static void sim_exp_extract_regex_pattern(const char *match, uint8 *match_buf)
{
    memcpy(match_buf, match + 1, strlen(match) - 2);
    match_buf[strlen(match) - 2] = '\0';
}

/* Compile one regex pattern and report its capture-group count. */
static t_stat sim_exp_compile_regex_pattern(EXPECT *exp, const uint8 *match_buf,
                                            int32 switches, EXPTAB *ep)
{
    int error_code;
    PCRE2_SIZE error_offset;
    PCRE2_UCHAR errmsg[256];
    uint32_t re_nsub;

    ep->regex =
        pcre2_compile((PCRE2_SPTR)match_buf, PCRE2_ZERO_TERMINATED,
                      (switches & EXP_TYP_REGEX_I) ? PCRE2_CASELESS : 0,
                      &error_code, &error_offset, NULL);
    if (NULL == ep->regex) {
        if (pcre2_get_error_message(error_code, errmsg,
                                    sizeof(errmsg) / sizeof(*errmsg)) < 0)
            strcpy((char *)errmsg, "unknown");
        sim_messagef(SCPE_ARG, "Regular Expression Error: %s\n",
                     (char *)errmsg);
        return SCPE_ARG | SCPE_NOMESSAGE;
    }
    (void)pcre2_pattern_info(ep->regex, PCRE2_INFO_CAPTURECOUNT, &re_nsub);
    ep->re_nsub = (int)re_nsub;
    sim_debug(exp->dbit, exp->dptr,
              "Expect Regular Expression: \"%s\" has %d sub expressions\n",
              match_buf, ep->re_nsub);
    return SCPE_OK;
}

/* Compile a regex rule directly into one EXPTAB entry. */
static t_stat sim_exp_install_regex_rule(EXPECT *exp, EXPTAB *ep,
                                         const uint8 *match_buf,
                                         int32 switches)
{
    return sim_exp_compile_regex_pattern(exp, match_buf, switches, ep);
}

/* Flatten the expect buffer into a contiguous string for regex matching. */
static char *sim_exp_build_regex_buffer(EXPECT *exp, char **tstr)
{
    char *cbuf = (char *)exp->buf;

    if (NULL != *tstr)
        return *tstr;
    if (strlen((char *)exp->buf) != exp->buf_ins) {
        size_t off;

        *tstr = (char *)malloc(exp->buf_ins + 1);
        (*tstr)[0] = '\0';
        for (off = 0; off < exp->buf_ins; off += 1 + strlen((char *)&exp->buf[off]))
            strcpy(&(*tstr)[strlen(*tstr)], (char *)&exp->buf[off]);
        cbuf = *tstr;
    }
    return cbuf;
}

/* Export regex match groups into the documented SCP variables. */
static void sim_exp_export_regex_groups(EXPECT *exp, const char *cbuf,
                                        const PCRE2_SIZE *ovector, int rc,
                                        size_t *sim_exp_match_sub_count,
                                        int re_nsub)
{
    size_t j;
    char *buf = (char *)malloc(1 + exp->buf_ins);

    for (j = 0; j < (size_t)rc; j++) {
        char env_name[32];
        size_t start_offs;
        size_t end_offs;
        bool have_range;

        start_offs = (size_t)ovector[2 * j];
        end_offs = (size_t)ovector[2 * j + 1];
        have_range = (ovector[2 * j] != PCRE2_UNSET) &&
                     (ovector[2 * j + 1] != PCRE2_UNSET) &&
                     (end_offs >= start_offs);

        sprintf(env_name, "_EXPECT_MATCH_GROUP_%d", (int)j);
        if (have_range) {
            memcpy(buf, &cbuf[start_offs], end_offs - start_offs);
            buf[end_offs - start_offs] = '\0';
            sim_sub_var_set(env_name, buf);
            sim_debug(exp->dbit, exp->dptr, "%s=%s\n", env_name, buf);
        } else {
            sim_debug(exp->dbit, exp->dptr, "unset %s\n", env_name);
            sim_sub_var_unset(env_name);
        }
    }
    for (; j < *sim_exp_match_sub_count; j++) {
        char env_name[32];

        sprintf(env_name, "_EXPECT_MATCH_GROUP_%d", (int)j);
        sim_sub_var_set(env_name, "");
    }
    *sim_exp_match_sub_count = (size_t)re_nsub + 1;
    free(buf);
}

/* Return whether SCP expect debug logging is active for one context. */
static bool sim_exp_debug_enabled(const EXPECT *exp)
{
    return sim_deb && exp->dptr && (exp->dptr->dctrl & exp->dbit);
}

/* Log one regex check using already-flattened buffer contents. */
static void sim_exp_log_regex_check(const EXPECT *exp, const char *cbuf,
                                    const EXPTAB *ep)
{
    char *estr;

    if (!sim_exp_debug_enabled(exp))
        return;
    estr = sim_encode_quoted_string((const uint8 *)cbuf, exp->buf_ins);
    sim_debug(exp->dbit, exp->dptr, "Checking String: %s\n", estr);
    sim_debug(exp->dbit, exp->dptr, "Against RegEx Match Rule: %s\n",
              ep->match_pattern);
    free(estr);
}

/* Log one exact-match buffer comparison. */
static void sim_exp_log_exact_check(const EXPECT *exp, const uint8 *data,
                                    size_t data_size, const uint8 *match,
                                    size_t match_size, size_t start_offs)
{
    char *estr;
    char *mstr;

    if (!sim_exp_debug_enabled(exp))
        return;
    estr = sim_encode_quoted_string(data, data_size);
    mstr = sim_encode_quoted_string(match, match_size);
    sim_debug(exp->dbit, exp->dptr, "Checking String[%" SIZE_T_FMT "d:%"
                                    SIZE_T_FMT "d]: %s\n",
              start_offs, data_size, estr);
    sim_debug(exp->dbit, exp->dptr, "Against Match Data: %s\n", mstr);
    free(estr);
    free(mstr);
}

/* Run one regex rule against the current expect buffer. */
static bool sim_exp_check_regex_rule(EXPECT *exp, EXPTAB *ep, char **tstr,
                                       size_t *sim_exp_match_sub_count)
{
    char *cbuf;
    pcre2_match_data *match_data;
    int rc;
    PCRE2_SIZE *ovector;

    cbuf = sim_exp_build_regex_buffer(exp, tstr);
    sim_exp_log_regex_check(exp, cbuf, ep);
    match_data = pcre2_match_data_create_from_pattern(ep->regex, NULL);
    if (NULL == match_data)
        return false;
    rc = pcre2_match(ep->regex, (PCRE2_SPTR)cbuf, (PCRE2_SIZE)exp->buf_ins,
                     0, PCRE2_NOTBOL, match_data, NULL);
    if (rc >= 0) {
        ovector = pcre2_get_ovector_pointer(match_data);
        sim_exp_export_regex_groups(exp, cbuf, ovector, rc,
                                    sim_exp_match_sub_count, ep->re_nsub);
        pcre2_match_data_free(match_data);
        return true;
    }
    pcre2_match_data_free(match_data);
    return false;
}

/* Remove one expect rule from a context and compact the table. */
static t_stat sim_exp_clr_tab(EXPECT *exp, EXPTAB *ep)
{
    size_t i;

    if (NULL == ep)
        return SCPE_OK;

    free(ep->match);
    free(ep->match_pattern);
    free(ep->act);
    sim_exp_free_regex_rule(ep);
    exp->size -= 1;
    for (i = ep - exp->rules; i < exp->size; i++)
        exp->rules[i] = exp->rules[i + 1];
    if (exp->size == 0) {
        free(exp->rules);
        exp->rules = NULL;
    }
    return SCPE_OK;
}

/* Display one expect rule in command-replay form. */
static t_stat sim_exp_show_tab(FILE *st, const EXPECT *exp, const EXPTAB *ep)
{
    uint32 default_haltafter = exp->default_haltafter;

    if (!ep)
        return SCPE_OK;
    fprintf(st, "    EXPECT");
    if (ep->switches & EXP_TYP_PERSIST)
        fprintf(st, " -p");
    if (ep->switches & EXP_TYP_CLEARALL)
        fprintf(st, " -c");
    if (ep->switches & EXP_TYP_REGEX)
        fprintf(st, " -r");
    if (ep->switches & EXP_TYP_REGEX_I)
        fprintf(st, " -i");
    if (ep->after != default_haltafter)
        fprintf(st, " HALTAFTER=%d", (int)ep->after);
    fprintf(st, " %s", ep->match_pattern);
    if (ep->cnt > 0)
        fprintf(st, " [%d]", ep->cnt);
    if (ep->act)
        fprintf(st, " %s", ep->act);
    fprintf(st, "\n");
    return SCPE_OK;
}

static void sim_exp_show_context_state(FILE *st, const EXPECT *exp,
                                       uint32 default_haltafter);

/* Return whether one command token looks like a TMXR dev:line target. */
static bool sim_exp_has_line_target(const char *token)
{
    return sim_isalpha(token[0]) && (strchr(token, ':') != NULL);
}

/* Resolve an optional SEND dev:line prefix and advance past it. */
static t_stat sim_exp_resolve_send_target(const char **cptr, SEND **snd,
                                          const char **next)
{
    char gbuf[CBUFSIZE];
    const char *tptr;
    t_stat r;

    tptr = get_glyph(*cptr, gbuf, ',');
    if (!sim_exp_has_line_target(gbuf)) {
        *snd = sim_cons_get_send();
        *next = tptr;
        return SCPE_OK;
    }

    r = tmxr_locate_line_send(gbuf, snd);
    if (r != SCPE_OK)
        return r;
    *cptr = tptr;
    *next = get_glyph(*cptr, gbuf, ',');
    return SCPE_OK;
}

/* Resolve an optional EXPECT dev:line prefix and advance past it. */
static t_stat sim_exp_resolve_expect_target(const char **cptr, EXPECT **exp,
                                            bool show_error)
{
    char gbuf[CBUFSIZE];
    const char *tptr;
    t_stat r;

    tptr = get_glyph(*cptr, gbuf, ',');
    if (!sim_exp_has_line_target(gbuf)) {
        *exp = sim_cons_get_expect();
        return SCPE_OK;
    }

    r = tmxr_locate_line_expect(gbuf, exp);
    if ((r != SCPE_OK) && show_error)
        return sim_messagef(r, "No such active line: %s\n", gbuf);
    if (r != SCPE_OK)
        return r;
    *cptr = tptr;
    return SCPE_OK;
}

t_stat send_cmd(int32 flag, const char *cptr)
{
    const char *tptr;
    char gbuf[CBUFSIZE];
    uint8 dbuf[CBUFSIZE];
    uint32 dsize = 0;
    uint32 delay;
    bool delay_set = false;
    uint32 after;
    bool after_set = false;
    t_stat r;
    SEND *snd;

    GET_SWITCHES(cptr);
    r = sim_exp_resolve_send_target(&cptr, &snd, &tptr);
    if (r != SCPE_OK)
        return r;
    if (!flag)
        return sim_send_clear(snd);
    delay = snd->default_delay;
    after = snd->default_after;
    tptr = get_glyph(cptr, gbuf, ',');
    while (*cptr) {
        if ((!strncmp(gbuf, "DELAY=", 6)) && (gbuf[6])) {
            delay = (uint32)get_uint(&gbuf[6], 10, 2000000000, &r);
            if (r != SCPE_OK)
                return sim_messagef(SCPE_ARG, "Invalid Delay Value: %s\n",
                                    &gbuf[6]);
            cptr = tptr;
            tptr = get_glyph(cptr, gbuf, ',');
            delay_set = true;
            if (!after_set)
                after = delay;
            continue;
        }
        if ((!strncmp(gbuf, "AFTER=", 6)) && (gbuf[6])) {
            after = (uint32)get_uint(&gbuf[6], 10, 2000000000, &r);
            if (r != SCPE_OK)
                return sim_messagef(SCPE_ARG, "Invalid After Value: %s\n",
                                    &gbuf[6]);
            cptr = tptr;
            tptr = get_glyph(cptr, gbuf, ',');
            after_set = true;
            continue;
        }
        if ((*cptr == '"') || (*cptr == '\''))
            break;
        return SCPE_ARG;
    }
    if (!*cptr) {
        if ((!delay_set) && (!after_set))
            return SCPE_2FARG;
        snd->default_delay = delay;
        snd->default_after = after;
        return SCPE_OK;
    }
    if ((*cptr != '"') && (*cptr != '\''))
        return sim_messagef(SCPE_ARG, "String must be quote delimited\n");
    cptr = get_glyph_quoted(cptr, gbuf, 0);
    if (*cptr != '\0')
        return SCPE_2MARG;

    if (SCPE_OK != sim_decode_quoted_string(gbuf, dbuf, &dsize))
        return sim_messagef(SCPE_ARG, "Invalid String\n");
    return sim_send_input(snd, dbuf, dsize, after, delay);
}

t_stat sim_show_send(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                     const char *cptr)
{
    t_stat r;
    SEND *snd;
    const char *tptr;

    (void)dptr;
    (void)uptr;
    (void)flag;

    r = sim_exp_resolve_send_target(&cptr, &snd, &tptr);
    if (r != SCPE_OK)
        return r;
    if (*cptr)
        return SCPE_2MARG;
    return sim_show_send_input(st, snd);
}

t_stat expect_cmd(int32 flag, const char *cptr)
{
    t_stat r;
    EXPECT *exp;

    GET_SWITCHES(cptr);
    r = sim_exp_resolve_expect_target(&cptr, &exp, true);
    if (r != SCPE_OK)
        return r;
    if (flag)
        return sim_set_expect(exp, cptr);
    return sim_set_noexpect(exp, cptr);
}

t_stat sim_show_expect(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                       const char *cptr)
{
    char gbuf[CBUFSIZE];
    t_stat r;
    EXPECT *exp;
    const char *tptr;

    (void)dptr;
    (void)uptr;
    (void)flag;

    r = sim_exp_resolve_expect_target(&cptr, &exp, false);
    if (r != SCPE_OK)
        return r;
    if (*cptr && (*cptr != '"') && (*cptr != '\''))
        return SCPE_ARG;
    tptr = get_glyph_quoted(cptr, gbuf, 0);
    if (*tptr != '\0')
        return SCPE_2MARG;
    if (*cptr && (cptr[strlen(cptr) - 1] != '"') &&
        (cptr[strlen(cptr) - 1] != '\''))
        return SCPE_ARG;
    return sim_exp_show(st, exp, gbuf);
}

t_stat sim_set_expect(EXPECT *exp, const char *cptr)
{
    char gbuf[CBUFSIZE];
    const char *tptr;
    const char *c1ptr;
    uint32 after;
    bool after_set = false;
    int32 cnt = 0;
    t_stat r;

    if ((cptr == NULL) || (*cptr == 0))
        return SCPE_2FARG;
    after = exp->default_haltafter;
    if (*cptr == '[') {
        cnt = (int32)strtotv(cptr + 1, &c1ptr, 10);
        if ((cptr == c1ptr) || (*c1ptr != ']') || (cnt <= 0))
            return sim_messagef(SCPE_ARG,
                                "Invalid Repeat count specification\n");
        cnt -= 1;
        cptr = c1ptr + 1;
        while (sim_isspace(*cptr))
            ++cptr;
    }
    tptr = get_glyph(cptr, gbuf, ',');
    if ((!strncmp(gbuf, "HALTAFTER=", 10)) && (gbuf[10])) {
        after = (uint32)get_uint(&gbuf[10], 10, 2000000000, &r);
        if (r != SCPE_OK)
            return sim_messagef(SCPE_ARG, "Invalid Halt After Value: %s\n",
                                &gbuf[10]);
        cptr = tptr;
        after_set = true;
    }
    if ((*cptr != '\0') && (*cptr != '"') && (*cptr != '\''))
        return sim_messagef(SCPE_ARG, "String must be quote delimited\n");
    cptr = get_glyph_quoted(cptr, gbuf, 0);

    if ((gbuf[0] == '\0') && (*cptr == '\0') && after_set) {
        exp->default_haltafter = after;
        return SCPE_OK;
    }

    return sim_exp_set(exp, gbuf, cnt, after, sim_switches, cptr);
}

t_stat sim_set_noexpect(EXPECT *exp, const char *cptr)
{
    char gbuf[CBUFSIZE];

    if (!cptr || !*cptr)
        return sim_exp_clrall(exp);
    if ((*cptr != '"') && (*cptr != '\''))
        return sim_messagef(SCPE_ARG, "String must be quote delimited\n");
    cptr = get_glyph_quoted(cptr, gbuf, 0);
    if (*cptr != '\0')
        return SCPE_2MARG;
    return sim_exp_clr(exp, gbuf);
}

t_stat sim_exp_clr(EXPECT *exp, const char *match)
{
    EXPTAB *ep = (EXPTAB *)sim_exp_fnd(exp, match, 0);

    while (NULL != ep) {
        sim_exp_clr_tab(exp, ep);
        ep = (EXPTAB *)sim_exp_fnd(exp, match, ep - exp->rules);
    }
    return SCPE_OK;
}

t_stat sim_exp_clrall(EXPECT *exp)
{
    size_t i;

    for (i = 0; i < exp->size; i++) {
        free(exp->rules[i].match);
        free(exp->rules[i].match_pattern);
        free(exp->rules[i].act);
        sim_exp_free_regex_rule(&exp->rules[i]);
    }
    free(exp->rules);
    exp->rules = NULL;
    exp->size = 0;
    free(exp->buf);
    exp->buf = NULL;
    exp->buf_size = 0;
    exp->buf_data = exp->buf_ins = 0;
    return SCPE_OK;
}

t_stat sim_exp_set(EXPECT *exp, const char *match, int32 cnt, uint32 after,
                   int32 switches, const char *act)
{
    EXPTAB *ep;
    uint8 *match_buf;
    uint32 match_size;
    size_t i;

    match_buf = (uint8 *)calloc(strlen(match) + 1, 1);
    if (!match_buf)
        return SCPE_MEM;
    if (switches & EXP_TYP_REGEX) {
        EXPTAB temp_ep;

        memset(&temp_ep, 0, sizeof(temp_ep));
        temp_ep.switches = switches;
        sim_exp_extract_regex_pattern(match, match_buf);
        if (SCPE_OK != sim_exp_compile_regex_pattern(exp, match_buf, switches,
                                                     &temp_ep)) {
            free(match_buf);
            return SCPE_ARG | SCPE_NOMESSAGE;
        }
        sim_exp_free_regex_rule(&temp_ep);
    } else {
        if (switches & EXP_TYP_REGEX_I) {
            free(match_buf);
            return sim_messagef(
                SCPE_ARG, "Case independent matching is only valid for RegEx "
                          "expect rules.\n");
        }
        sim_data_trace(exp->dptr, exp->dptr->units, (const uint8 *)match, "",
                       strlen(match) + 1, "Expect Match String", exp->dbit);
        if (SCPE_OK !=
            sim_decode_quoted_string(match, match_buf, &match_size)) {
            free(match_buf);
            return sim_messagef(SCPE_ARG, "Invalid quoted string\n");
        }
    }
    free(match_buf);
    for (i = 0; i < exp->size; i++) {
        if ((0 == strcmp(match, exp->rules[i].match_pattern)) &&
            (exp->rules[i].switches & EXP_TYP_PERSIST))
            return sim_messagef(SCPE_ARG,
                                "Persistent Expect rule with identical match "
                                "string already exists\n");
    }
    if (after && exp->size)
        return sim_messagef(
            SCPE_ARG, "Multiple concurrent EXPECT rules aren't valid when "
                      "a HALTAFTER parameter is non-zero\n");
    exp->rules =
        (EXPTAB *)realloc(exp->rules, sizeof(*exp->rules) * (exp->size + 1));
    ep = &exp->rules[exp->size];
    exp->size += 1;
    memset(ep, 0, sizeof(*ep));
    ep->after = after;
    ep->match_pattern = (char *)malloc(strlen(match) + 1);
    if (ep->match_pattern)
        strcpy(ep->match_pattern, match);
    ep->cnt = cnt;
    ep->switches = switches;
    match_buf = (uint8 *)calloc(strlen(match) + 1, 1);
    if ((match_buf == NULL) || (ep->match_pattern == NULL)) {
        sim_exp_clr_tab(exp, ep);
        free(match_buf);
        return SCPE_MEM;
    }
    if (switches & EXP_TYP_REGEX) {
        sim_exp_extract_regex_pattern(match, match_buf);
        if (SCPE_OK != sim_exp_install_regex_rule(exp, ep, match_buf, switches)) {
            free(match_buf);
            sim_exp_clr_tab(exp, ep);
            return SCPE_ARG | SCPE_NOMESSAGE;
        }
        free(match_buf);
        match_buf = NULL;
    } else {
        sim_data_trace(exp->dptr, exp->dptr->units, (const uint8 *)match, "",
                       strlen(match) + 1, "Expect Match String", exp->dbit);
        (void)sim_decode_quoted_string(match, match_buf, &match_size);
        ep->match = match_buf;
        ep->size = match_size;
    }
    if (ep->act) {
        free(ep->act);
        ep->act = NULL;
    }
    if (act)
        while (sim_isspace(*act))
            ++act;
    if ((act != NULL) && (*act != 0)) {
        char *newp;

        act = sim_unsub_args(act);
        newp = (char *)calloc(strlen(act) + 1, sizeof(*act));
        if (newp == NULL)
            return SCPE_MEM;
        strcpy(newp, act);
        ep->act = newp;
    }
    for (i = 0; i < exp->size; i++) {
        size_t pattern_bytes = 10 * strlen(ep->match_pattern);
        size_t compare_size =
            (exp->rules[i].switches & EXP_TYP_REGEX)
                ? ((pattern_bytes >= 1024) ? pattern_bytes : 1024)
                : exp->rules[i].size;
        if (compare_size >= exp->buf_size) {
            exp->buf = (uint8 *)realloc(exp->buf, compare_size + 2);
            exp->buf_size = compare_size + 1;
        }
    }
    return SCPE_OK;
}

t_stat sim_exp_show(FILE *st, const EXPECT *exp, const char *match)
{
    const EXPTAB *ep = (const EXPTAB *)sim_exp_fnd(exp, match, 0);
    uint32 default_haltafter = exp->default_haltafter;

    sim_exp_show_context_state(st, exp, default_haltafter);
    if (!*match)
        return sim_exp_showall(st, exp);
    if (!ep) {
        fprintf(st, "  No Rules match '%s'\n", match);
        return SCPE_ARG;
    }
    do {
        sim_exp_show_tab(st, exp, ep);
        ep = (const EXPTAB *)sim_exp_fnd(exp, match, 1 + (ep - exp->rules));
    } while (ep);
    return SCPE_OK;
}

t_stat sim_exp_showall(FILE *st, const EXPECT *exp)
{
    size_t i;

    for (i = 0; i < exp->size; i++)
        sim_exp_show_tab(st, exp, &exp->rules[i]);
    return SCPE_OK;
}

/* Render SHOW EXPECT state from already-gathered values. */
static void sim_exp_show_context_state(FILE *st, const EXPECT *exp,
                                       uint32 default_haltafter)
{
    if (exp->buf_size) {
        char *bstr = sim_encode_quoted_string(exp->buf, exp->buf_ins);

        fprintf(st, "  Match Buffer Size: %" SIZE_T_FMT "d\n", exp->buf_size);
        fprintf(st, "  Buffer Insert Offset: %" SIZE_T_FMT "d\n", exp->buf_ins);
        fprintf(st, "  Buffer Contents: %s\n", bstr);
        if (default_haltafter)
            fprintf(st, "  Default HaltAfter: %u %s\n",
                    (unsigned)default_haltafter, sim_vm_interval_units);
        free(bstr);
    }
    if (exp->dptr && (exp->dbit & exp->dptr->dctrl))
        fprintf(st, "  Expect Debugging via: SET %s DEBUG%s%s\n",
                sim_dname(exp->dptr), exp->dptr->debflags ? "=" : "",
                exp->dptr->debflags ? _get_dbg_verb(exp->dbit, exp->dptr, NULL)
                                    : "");
    fprintf(st, "  Match Rules:\n");
}

/* Render pending SEND input data from already-gathered state. */
static void sim_show_send_pending_data(FILE *st, const SEND *snd)
{
    if (snd->extoff < snd->insoff) {
        fprintf(st, "  %" SIZE_T_FMT "d bytes of pending input Data:\n    ",
                snd->insoff - snd->extoff);
        fprint_buffer_string(st, snd->buffer + snd->extoff,
                             snd->insoff - snd->extoff);
        fprintf(st, "\n");
    } else
        fprintf(st, "  No Pending Input Data\n");
}

/* Render SEND timing and default state from explicit values. */
static void sim_show_send_timing(FILE *st, const SEND *snd, uint32 delay,
                                 uint32 after)
{
    if ((snd->next_time - sim_gtime()) > 0) {
        if (((snd->next_time - sim_gtime()) >
             (sim_timer_inst_per_sec() / 1000000.0)) &&
            ((sim_timer_inst_per_sec() / 1000000.0) > 0.0))
            fprintf(st,
                    "  Minimum of %d %s (%d microseconds) before sending "
                    "first character\n",
                    (int)(snd->next_time - sim_gtime()), sim_vm_interval_units,
                    (int)((snd->next_time - sim_gtime()) /
                          (sim_timer_inst_per_sec() / 1000000.0)));
        else
            fprintf(st, "  Minimum of %d %s before sending first character\n",
                    (int)(snd->next_time - sim_gtime()), sim_vm_interval_units);
    }
    if ((snd->delay > (sim_timer_inst_per_sec() / 1000000.0)) &&
        ((sim_timer_inst_per_sec() / 1000000.0) > 0.0))
        fprintf(st, "  Minimum of %d %s (%d microseconds) between characters\n",
                (int)snd->delay, sim_vm_interval_units,
                (int)(snd->delay / (sim_timer_inst_per_sec() / 1000000.0)));
    else
        fprintf(st, "  Minimum of %d %s between characters\n", (int)snd->delay,
                sim_vm_interval_units);
    if (after)
        fprintf(st, "  Default delay before first character input is %u %s\n",
                after, sim_vm_interval_units);
    if (delay)
        fprintf(st, "  Default delay between character input is %u %s\n", delay,
                sim_vm_interval_units);
    if (snd->dptr && (snd->dbit & snd->dptr->dctrl))
        fprintf(st, "  Send Debugging via: SET %s DEBUG%s%s\n",
                sim_dname(snd->dptr), snd->dptr->debflags ? "=" : "",
                snd->dptr->debflags ? _get_dbg_verb(snd->dbit, snd->dptr, NULL)
                                    : "");
}

/* Check one exact-match rule against the current expect buffer. */
static bool sim_exp_check_exact_rule(EXPECT *exp, EXPTAB *ep)
{
    if (exp->buf_data < ep->size)
        return false;
    if (exp->buf_ins < ep->size) {
        if (exp->buf_ins > 0) {
            sim_exp_log_exact_check(exp, exp->buf, exp->buf_ins,
                                    &ep->match[ep->size - exp->buf_ins],
                                    exp->buf_ins, 0);
            if (memcmp(exp->buf, &ep->match[ep->size - exp->buf_ins],
                       exp->buf_ins))
                return false;
        }
        sim_exp_log_exact_check(
            exp, &exp->buf[exp->buf_size - (ep->size - exp->buf_ins)],
            ep->size - exp->buf_ins, ep->match, ep->size - exp->buf_ins,
            exp->buf_size - ep->size - exp->buf_ins);
        return memcmp(&exp->buf[exp->buf_size - (ep->size - exp->buf_ins)],
                      ep->match, ep->size - exp->buf_ins) == 0;
    }
    sim_exp_log_exact_check(exp, &exp->buf[exp->buf_ins - ep->size], ep->size,
                            ep->match, ep->size, exp->buf_ins - ep->size);
    return memcmp(&exp->buf[exp->buf_ins - ep->size], ep->match, ep->size) ==
           0;
}

t_stat sim_exp_check(EXPECT *exp, uint8 data)
{
    size_t i;
    EXPTAB *ep = NULL;
    int regex_checks = 0;
    char *tstr = NULL;

    if ((!exp) || (!exp->rules))
        return SCPE_OK;

    exp->buf[exp->buf_ins++] = data;
    exp->buf[exp->buf_ins] = '\0';
    if (exp->buf_data < exp->buf_size)
        ++exp->buf_data;

    for (i = 0; i < exp->size; i++) {
        ep = &exp->rules[i];
        if (ep->switches & EXP_TYP_REGEX) {
            static size_t sim_exp_match_sub_count = 0;

            ++regex_checks;
            if (sim_exp_check_regex_rule(exp, ep, &tstr,
                                         &sim_exp_match_sub_count))
                break;
        } else {
            if (sim_exp_check_exact_rule(exp, ep))
                break;
        }
    }
    if (exp->buf_ins == exp->buf_size) {
        if (regex_checks) {
            memmove(exp->buf, &exp->buf[exp->buf_size / 2],
                    exp->buf_size - (exp->buf_size / 2));
            exp->buf_ins -= exp->buf_size / 2;
            exp->buf_data = exp->buf_ins;
            sim_debug(exp->dbit, exp->dptr,
                      "Buffer Full - sliding the last %" SIZE_T_FMT
                      "d bytes to start of buffer new insert at: %" SIZE_T_FMT
                      "d\n",
                      exp->buf_size / 2, exp->buf_ins);
        } else {
            exp->buf_ins = 0;
            sim_debug(exp->dbit, exp->dptr, "Buffer wrapping\n");
        }
    }
    if (i != exp->size) {
        sim_debug(exp->dbit, exp->dptr, "Matched expect pattern: %s\n",
                  ep->match_pattern);
        sim_sub_var_set("_EXPECT_MATCH_PATTERN", ep->match_pattern);
        if (ep->cnt > 0) {
            ep->cnt -= 1;
            sim_debug(exp->dbit, exp->dptr,
                      "Waiting for %d more match%s before stopping\n", ep->cnt,
                      (ep->cnt == 1) ? "" : "es");
        } else {
            uint32 after = ep->after;
            int32 switches = ep->switches;

            if (ep->act && *ep->act)
                sim_debug(exp->dbit, exp->dptr, "Initiating actions: %s\n",
                          ep->act);
            else
                sim_debug(exp->dbit, exp->dptr,
                          "No actions specified, stopping...\n");
            sim_brk_setact(ep->act);
            if (ep->switches & EXP_TYP_CLEARALL)
                sim_exp_clrall(exp);
            else if (!(ep->switches & EXP_TYP_PERSIST))
                sim_exp_clr_tab(exp, ep);
            sim_activate(
                &sim_expect_unit,
                (switches & EXP_TYP_TIME)
                    ? (int32)((sim_timer_inst_per_sec() * after) / 1000000.0)
                    : after);
        }
        exp->buf_data = exp->buf_ins = 0;
    }
    free(tstr);
    return SCPE_OK;
}

t_stat sim_send_input(SEND *snd, uint8 *data, size_t size, uint32 after,
                      uint32 delay)
{
    if (snd->extoff != 0) {
        if (snd->insoff > snd->extoff)
            memmove(snd->buffer, snd->buffer + snd->extoff,
                    snd->insoff - snd->extoff);
        snd->insoff -= snd->extoff;
        snd->extoff = 0;
    }
    if (snd->insoff + size > snd->bufsize) {
        snd->bufsize = snd->insoff + size;
        snd->buffer = (uint8 *)realloc(snd->buffer, snd->bufsize);
    }
    memcpy(snd->buffer + snd->insoff, data, size);
    snd->insoff += size;
    snd->delay = (sim_switches & SWMASK('T'))
                     ? (uint32)((sim_timer_inst_per_sec() * delay) / 1000000.0)
                     : delay;
    snd->after = (sim_switches & SWMASK('T'))
                     ? (uint32)((sim_timer_inst_per_sec() * after) / 1000000.0)
                     : after;
    if (sim_switches & SWMASK('T'))
        sim_debug(snd->dbit, snd->dptr,
                  "%d bytes queued for input. Delay %d usecs = %d insts, "
                  "After %d usecs = %d insts\n",
                  (int)size, (int)delay, (int)snd->delay, (int)after,
                  (int)snd->after);
    else
        sim_debug(snd->dbit, snd->dptr,
                  "%d bytes queued for input. Delay=%d, After=%d\n", (int)size,
                  (int)delay, (int)after);
    snd->next_time = sim_gtime() + snd->after;
    return SCPE_OK;
}

t_stat sim_send_clear(SEND *snd)
{
    snd->insoff = 0;
    snd->extoff = 0;
    return SCPE_OK;
}

t_stat sim_show_send_input(FILE *st, const SEND *snd)
{
    uint32 delay = snd->default_delay;
    uint32 after = snd->default_after;

    fprintf(st, "%s\n", tmxr_send_line_name(snd));
    sim_show_send_pending_data(st, snd);
    sim_show_send_timing(st, snd, delay, after);
    return SCPE_OK;
}

bool sim_send_poll_data(SEND *snd, t_stat *stat)
{
    if ((NULL != snd) && (snd->extoff < snd->insoff)) {
        if (sim_gtime() < snd->next_time) {
            *stat = SCPE_OK;
            sim_debug(snd->dbit, snd->dptr, "Too soon to inject next byte\n");
        } else {
            char dstr[8] = "";

            *stat = snd->buffer[snd->extoff++] | SCPE_KFLAG;
            snd->next_time = sim_gtime() + snd->delay;
            if (sim_isgraph(*stat & 0xFF) || ((*stat & 0xFF) == ' '))
                sprintf(dstr, " '%c'", *stat & 0xFF);
            sim_debug(snd->dbit, snd->dptr, "Byte value: 0x%02X%s injected\n",
                      *stat & 0xFF, dstr);
        }
        return true;
    }
    return false;
}
