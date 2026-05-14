/* scp_cmdvars.c: SCP command-variable substitution helpers */
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "scp.h"

#include "sim_console.h"
#include "dynstr.h"
#include "sim_host_path.h"
#include "sim_types.h"

#if !defined(_WIN32)
#include <sys/utsname.h>
#endif

struct deleted_env_var {
    char *name;
    char *value;
};

struct scp_sub_var {
    char *name;
    char *value;
};

#define SIM_CMDVARS_PARTS_SIZE 32

static struct deleted_env_var *sim_external_env = NULL;
static int sim_external_env_count = 0;
static struct scp_sub_var *sim_sub_vars = NULL;
static int sim_sub_var_count = 0;
static char *sim_sub_instr = NULL;
static char *sim_sub_instr_buf = NULL;
static size_t sim_sub_instr_size = 0;
static size_t *sim_sub_instr_off = NULL;
static struct timespec cmd_time;
static char sim_host_ostype[64];

extern bool sim_runlimit_enabled;
extern int32_t sim_runlimit_value;
extern const char *sim_runlimit_units;
extern bool sim_file_compare_diff_valid;
extern size_t sim_file_compare_diff_offset;

static bool sim_cmdvars_probe_uname(char *buf, size_t size);
static bool sim_cmdvars_localtime(time_t now, struct tm *tmnow);
#if !defined(_WIN32)
/* Tests can replace the underlying uname(3) call to drive failure paths. */
/* Call uname(3) through the default host implementation. */
static int sim_cmdvars_default_uname_call(struct utsname *utsname_info)
{
    return uname(utsname_info);
}

static sim_cmdvars_uname_hook_fn sim_cmdvars_uname_hook =
    sim_cmdvars_default_uname_call;
#endif

/* Tests can replace the underlying localtime call to drive failure paths. */
/* Break down one time_t value with the platform local-time routine. */
static bool sim_cmdvars_default_localtime_call(time_t now, struct tm *tmnow)
{
#if defined(_WIN32)
    return localtime_s(tmnow, &now) == 0;
#else
    return localtime_r(&now, tmnow) != NULL;
#endif
}

static sim_cmdvars_localtime_hook_fn sim_cmdvars_localtime_hook =
    sim_cmdvars_default_localtime_call;

/* Assemble the %* expansion into one growable temporary string. */
static void sim_cmdvars_expand_star_args(dynstr_t *ds, char *do_arg[])
{
    size_t i;

    for (i = 1; i <= 9; ++i) {
        if (do_arg[i] == NULL)
            break;
        if (i != 1)
            dynstr_append(ds, " ");
        if (strchr(do_arg[i], ' ')) {
            char quote = '"';

            if (strchr(do_arg[i], quote))
                quote = '\'';
            dynstr_append_ch(ds, quote);
            dynstr_append(ds, do_arg[i]);
            dynstr_append_ch(ds, quote);
        } else
            dynstr_append(ds, do_arg[i]);
    }
}

/* Break down one wall-clock time value using the platform local-time API. */
static bool sim_cmdvars_localtime(time_t now, struct tm *tmnow)
{
    return sim_cmdvars_localtime_hook(now, tmnow);
}

/* Decode a whole-line quoted command before later substitution work. */
static void sim_cmdvars_decode_initial_quoted_line(char *ip, size_t instr_size,
                                                   char *instr, char *scratch)
{
    if ((*ip == '"') || (*ip == '\'')) {
        const char *cptr = ip;
        char *tp = scratch;

        cptr = get_glyph_quoted(cptr, tp, 0);
        if (*cptr == '\0') {
            uint32_t dsize;

            if (SCPE_OK == sim_decode_quoted_string(tp, (uint8_t *)tp, &dsize)) {
                tp[dsize] = '\0';
                while (sim_isspace(*tp))
                    memmove(tp, tp + 1, strlen(tp));
                strlcpy(ip, tp, instr_size - (ip - instr));
                strlcpy(sim_sub_instr + (ip - instr), tp,
                        instr_size - (ip - instr));
            }
        }
    }
}

/* Copy one expanded value into the command output buffer. */
static void sim_cmdvars_copy_expansion(const char *ap, char **op, char *oend,
                                       const char *instr, const char *ip,
                                       size_t *outstr_off)
{
    while (*ap && (*op < oend)) {
        sim_sub_instr_off[(*outstr_off)++] = ip - instr;
        *(*op)++ = *ap++;
    }
}

/* Apply any %~ filepath-part fixup, then copy one expansion to the output. */
static void sim_cmdvars_expand_and_copy(const char *ap, bool expand_it,
                                        char *parts, char **op, char *oend,
                                        const char *instr, const char *ip,
                                        size_t *outstr_off)
{
    char *expanded = NULL;

    if (ap == NULL)
        return;
    if (expand_it) {
        expanded = sim_filepath_parts(ap, parts);
        ap = expanded;
    }
    sim_cmdvars_copy_expansion(ap, op, oend, instr, ip, outstr_off);
    free(expanded);
}

/* Parse one %-form after the leading percent character. */
static const char *sim_cmdvars_parse_percent(const char **ipp, char *gbuf,
                                             char *rbuf, size_t rbuf_size,
                                             char *do_arg[], bool *expand_it,
                                             char *parts,
                                             dynstr_t *star_args,
                                             bool *emit_percent)
{
    const char *ip = *ipp;
    const char *ap = NULL;
    bool malformed_parts = false;
    uint32_t i;

    *emit_percent = false;
    *expand_it = false;
    parts[0] = '\0';
    if (*ip == '~') {
        *expand_it = true;
        ++ip;
        for (i = 0;
             (i < (SIM_CMDVARS_PARTS_SIZE - 1)) && (strchr("fpnxtz", *ip));
             i++, ip++) {
            parts[i] = *ip;
            parts[i + 1] = '\0';
        }
        if ((i == (SIM_CMDVARS_PARTS_SIZE - 1)) && strchr("fpnxtz", *ip)) {
            malformed_parts = true;
            while (strchr("fpnxtz", *ip))
                ++ip;
        }
    }
    if (malformed_parts) {
        if (((*ip >= '0') && (*ip <= '9')) || (*ip == '*'))
            ++ip;
        else if (*ip != '\0') {
            get_glyph_nc(ip, gbuf, '%');
            ip += strlen(gbuf);
            if (*ip == '%')
                ++ip;
        }
        *ipp = ip;
        return NULL;
    }
    if ((*ip >= '0') && (*ip <= ('9'))) {
        ap = do_arg[*ip - '0'];
        for (i = 0; i < (uint32_t)(*ip - '0'); ++i)
            if (do_arg[i] == NULL) {
                ap = NULL;
                break;
            }
        ++ip;
    } else if (*ip == '*') {
        sim_cmdvars_expand_star_args(star_args, do_arg);
        ap = dynstr_cstr(star_args);
        ++ip;
    } else if (*ip == '\0') {
        *emit_percent = true;
    } else {
        get_glyph_nc(ip, gbuf, '%');
        ap = _sim_get_env_special(gbuf, rbuf, rbuf_size);
        ip += strlen(gbuf);
        if (*ip == '%')
            ++ip;
    }
    *ipp = ip;
    return ap;
}

/* Expand the initial command token if it names one command variable. */
static bool sim_cmdvars_expand_initial_token(char **ipp, char **op,
                                               char *oend, const char *instr,
                                               char *gbuf, char *rbuf,
                                               size_t rbuf_size,
                                               size_t *outstr_off)
{
    const char *ap = NULL;
    char *ip = *ipp;

    get_glyph(ip, gbuf, 0);
    ap = _sim_get_env_special(gbuf, rbuf, rbuf_size);
    if (ap == NULL)
        return false;
    sim_cmdvars_copy_expansion(ap, op, oend, instr, ip, outstr_off);
    ip += strlen(gbuf);
    *ipp = ip;
    return true;
}

/* Return the value of one SCP-owned substitution variable, if present. */
static const char *sim_sub_var_get(const char *name)
{
    int i;

    for (i = 0; i < sim_sub_var_count; ++i)
        if (0 == strcmp(name, sim_sub_vars[i].name))
            return sim_sub_vars[i].value;
    return NULL;
}

/* Remove one saved external environment variable by index. */
static void sim_external_env_remove_index(int index)
{
    int j;

    free(sim_external_env[index].name);
    free(sim_external_env[index].value);
    for (j = index; j + 1 < sim_external_env_count; ++j)
        sim_external_env[j] = sim_external_env[j + 1];
    --sim_external_env_count;
    if (sim_external_env_count == 0) {
        free(sim_external_env);
        sim_external_env = NULL;
    } else {
        sim_external_env = (struct deleted_env_var *)realloc(
            sim_external_env,
            sim_external_env_count * sizeof(*sim_external_env));
    }
}

/* Remove one saved external environment variable by name. */
static void sim_external_env_remove_name(const char *name)
{
    int i;

    for (i = 0; i < sim_external_env_count; ++i)
        if (0 == strcmp(name, sim_external_env[i].name)) {
            sim_external_env_remove_index(i);
            return;
        }
}

/* Return the executable basename used for %SIM_BIN_NAME%. */
static const char *sim_bin_name_value(char *rbuf, size_t rbuf_size)
{
    const char *base;

    if ((NULL == sim_prog_name) || ('\0' == sim_prog_name[0]))
        return NULL;
    base = strrchr(sim_prog_name, '/');
    if (base == NULL)
        base = strrchr(sim_prog_name, '\\');
    if (base == NULL)
        base = sim_prog_name;
    else
        ++base;
    strlcpy(rbuf, base, rbuf_size);
    return rbuf;
}

/* Probe the host OS type with uname(3). */
static bool sim_cmdvars_probe_uname(char *buf, size_t size)
{
#if defined(_WIN32)
    strlcpy(buf, "Windows", size);
    return true;
#else
    struct utsname utsname_info;

    if (sim_cmdvars_uname_hook(&utsname_info) != 0)
        return false;
    strlcpy(buf, utsname_info.sysname, size);
    return buf[0] != '\0';
#endif
}

/* Return the cached or discovered host OS type. */
static const char *sim_ostype_value(char *rbuf, size_t rbuf_size)
{
    if ((sim_host_ostype[0] == '\0') &&
        !sim_cmdvars_probe_uname(sim_host_ostype, sizeof(sim_host_ostype)))
        return NULL;
    strlcpy(rbuf, sim_host_ostype, rbuf_size);
    return rbuf;
}

/* Look up one real host environment variable by exact or uppercase name. */
static const char *sim_get_host_env_uplowcase(const char *gbuf, char *rbuf,
                                              size_t rbuf_size)
{
    const char *ap = NULL;
    size_t i;

    ap = getenv(gbuf);
    if (ap)
        return ap;
    strlcpy(rbuf, gbuf, rbuf_size);
    for (i = 0; rbuf[i]; i++)
        rbuf[i] = (char)toupper((uchar_t)rbuf[i]);
    return getenv(rbuf);
}

/* Perform :~ and := substring/substitution operations on one value. */
/* TODO: This fixed-buffer interface is the wrong long-term shape.
   Rewrite command-variable substitution to use dynamic allocation so
   callers get complete results or explicit errors instead of silent
   truncation.  Prefer existing string helpers, or add focused ones,
   rather than open-coding string assembly in pointer arithmetic. */
static void sim_subststr_substr(const char *ops, char *rbuf, size_t rbuf_size)
{
    int rbuf_len = (int)strlen(rbuf);
    char *tstr = strdup(rbuf);

    if (*ops == '~') {
        int offset = 0, length = rbuf_len;
        int o, l;

        switch (sscanf(ops + 1, "%d,%d", &o, &l)) {
        case 2:
            if (l < 0)
                length = rbuf_len - MIN(-l, rbuf_len);
            else
                length = l;
            FALLTHROUGH;
        case 1:
            if (o < 0)
                offset = rbuf_len - MIN(-o, rbuf_len);
            else
                offset = MIN(o, rbuf_len);
            break;
        case 0:
            offset = 0;
            length = rbuf_len;
            break;
        }
        if (offset + length > rbuf_len)
            length = rbuf_len - offset;
        memcpy(rbuf, tstr + offset, length);
        rbuf[length] = '\0';
    } else {
        const char *eq;

        if ((eq = strchr(ops, '='))) {
            const char *last = tstr;
            const char *curr = tstr;
            char *match = strndup(ops, eq - ops);
            size_t copy_left = rbuf_size ? rbuf_size - 1 : 0;
            bool asterisk_match;

            asterisk_match = (*ops == '*');
            if (asterisk_match)
                memmove(match, match + 1, 1 + strlen(match + 1));
            while ((curr = strstr(last, match))) {
                if (!asterisk_match) {
                    size_t move_size = MIN((size_t)(curr - last), copy_left);

                    memcpy(rbuf, last, move_size);
                    copy_left -= move_size;
                    rbuf += move_size;
                } else
                    asterisk_match = false;
                size_t move_size = MIN(strlen(eq + 1), copy_left);

                memcpy(rbuf, eq + 1, move_size);
                copy_left -= move_size;
                rbuf += move_size;
                curr += strlen(match);
                last = curr;
            }
            size_t move_size = MIN(strlen(last), copy_left);

            memcpy(rbuf, last, move_size);
            copy_left -= move_size;
            rbuf += move_size;
            if (rbuf_size)
                *rbuf = '\0';
            free(match);
        }
    }
    free(tstr);
}

/* Capture one external host variable whose name collides with an SCP alias. */
void sim_cmdvars_capture_env_alias(const char *name)
{
    const char *env_value;

    env_value = getenv(name);
    if (env_value == NULL)
        return;
    ++sim_external_env_count;
    sim_external_env = (struct deleted_env_var *)realloc(
        sim_external_env, sim_external_env_count * sizeof(*sim_external_env));
    sim_external_env[sim_external_env_count - 1].name = strdup(name);
    sim_external_env[sim_external_env_count - 1].value = strdup(env_value);
    unsetenv(name);
}

/* Execute one host command while restoring saved external env aliases. */
int sim_cmdvars_system(const char *command)
{
    int i;
    int status;

    for (i = 0; i < sim_external_env_count; i++)
        setenv(sim_external_env[i].name, sim_external_env[i].value, 1);
    status = system(command);
    for (i = 0; i < sim_external_env_count; i++)
        unsetenv(sim_external_env[i].name);
    return status;
}

#if !defined(_WIN32)
/* Replace the underlying uname(3) call used by SIM_OSTYPE lookup. */
void sim_cmdvars_set_test_uname_hook(sim_cmdvars_uname_hook_fn hook)
{
    sim_cmdvars_uname_hook = hook ? hook : sim_cmdvars_default_uname_call;
}
#endif

/* Replace the underlying localtime call used by time-variable lookup. */
void sim_cmdvars_set_test_localtime_hook(sim_cmdvars_localtime_hook_fn hook)
{
    sim_cmdvars_localtime_hook =
        hook ? hook : sim_cmdvars_default_localtime_call;
}

/* Clear the cached host OS type used by SIM_OSTYPE lookup.

   Tests use this to ensure one case does not reuse a previous case's cached
   value. */
void sim_cmdvars_reset_ostype_cache(void)
{
    sim_host_ostype[0] = '\0';
}

/* Reset command-variable state owned by this module. */
void sim_cmdvars_reset(void)
{
    while (sim_external_env_count > 0)
        sim_external_env_remove_index(sim_external_env_count - 1);
    while (sim_sub_var_count > 0)
        sim_sub_var_unset(sim_sub_vars[sim_sub_var_count - 1].name);
    free(sim_sub_instr);
    sim_sub_instr = NULL;
    free(sim_sub_instr_off);
    sim_sub_instr_off = NULL;
    sim_sub_instr_buf = NULL;
    sim_sub_instr_size = 0;
#if !defined(_WIN32)
    sim_cmdvars_set_test_uname_hook(NULL);
#endif
    sim_cmdvars_set_test_localtime_hook(NULL);
    sim_cmdvars_reset_ostype_cache();
    memset(&cmd_time, 0, sizeof(cmd_time));
}

/* Set or replace one SCP-owned substitution variable. */
void sim_sub_var_set(const char *name, const char *value)
{
    int i;

    for (i = 0; i < sim_sub_var_count; ++i)
        if (0 == strcmp(name, sim_sub_vars[i].name)) {
            free(sim_sub_vars[i].value);
            sim_sub_vars[i].value = strdup(value);
            return;
        }
    ++sim_sub_var_count;
    sim_sub_vars = (struct scp_sub_var *)realloc(
        sim_sub_vars, sim_sub_var_count * sizeof(*sim_sub_vars));
    sim_sub_vars[sim_sub_var_count - 1].name = strdup(name);
    sim_sub_vars[sim_sub_var_count - 1].value = strdup(value);
}

/* Remove one SCP-owned substitution variable, if present. */
void sim_sub_var_unset(const char *name)
{
    int i;

    for (i = 0; i < sim_sub_var_count; ++i)
        if (0 == strcmp(name, sim_sub_vars[i].name)) {
            int j;

            free(sim_sub_vars[i].name);
            free(sim_sub_vars[i].value);
            for (j = i; j + 1 < sim_sub_var_count; ++j)
                sim_sub_vars[j] = sim_sub_vars[j + 1];
            --sim_sub_var_count;
            if (sim_sub_var_count == 0) {
                free(sim_sub_vars);
                sim_sub_vars = NULL;
            } else {
                sim_sub_vars = (struct scp_sub_var *)realloc(
                    sim_sub_vars, sim_sub_var_count * sizeof(*sim_sub_vars));
            }
            return;
        }
}

/* Remove every SCP-owned substitution variable with one common prefix. */
void sim_sub_var_clear_prefix(const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    int i;

    for (i = sim_sub_var_count - 1; i >= 0; --i)
        if (0 == strncmp(sim_sub_vars[i].name, prefix, prefix_len))
            sim_sub_var_unset(sim_sub_vars[i].name);
}

/* Resolve one SCP substitution name from built-ins, internal variables,
   saved external aliases, or the host environment. */
const char *_sim_get_env_special(const char *gbuf, char *rbuf, size_t rbuf_size)
{
    int i;
    const char *ap = NULL;
    const char *fixup_needed = strchr(gbuf, ':');
    char *tgbuf = NULL;
    time_t now = (time_t)cmd_time.tv_sec;
    struct tm tm_storage;
    struct tm *tmnow;
    size_t tgbuf_size = MAX(rbuf_size, 1 + (size_t)(fixup_needed - gbuf));

    if (fixup_needed) {
        tgbuf = (char *)calloc(tgbuf_size, 1);
        memcpy(tgbuf, gbuf, (fixup_needed - gbuf));
        gbuf = tgbuf;
    }
    tmnow = sim_cmdvars_localtime(now, &tm_storage) ? &tm_storage : NULL;

    if (tmnow && !strcmp("DATE", gbuf)) {
        snprintf(rbuf, rbuf_size, "%4d-%02d-%02d", tmnow->tm_year + 1900,
                 tmnow->tm_mon + 1, tmnow->tm_mday);
        ap = rbuf;
    } else if (tmnow && !strcmp("TIME", gbuf)) {
        snprintf(rbuf, rbuf_size, "%02d:%02d:%02d", tmnow->tm_hour,
                 tmnow->tm_min, tmnow->tm_sec);
        ap = rbuf;
    } else if (tmnow && !strcmp("DATETIME", gbuf)) {
        snprintf(rbuf, rbuf_size, "%04d-%02d-%02dT%02d:%02d:%02d",
                 tmnow->tm_year + 1900, tmnow->tm_mon + 1, tmnow->tm_mday,
                 tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec);
        ap = rbuf;
    } else if (tmnow && !strcmp("LDATE", gbuf)) {
        strftime(rbuf, rbuf_size, "%x", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("LTIME", gbuf)) {
        strftime(rbuf, rbuf_size, "%r", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("CTIME", gbuf)) {
        strftime(rbuf, rbuf_size, "%c", tmnow);
        ap = rbuf;
    } else if (!strcmp("UTIME", gbuf)) {
        snprintf(rbuf, rbuf_size, "%" PRIdMAX, (intmax_t)now);
        ap = rbuf;
    } else if (tmnow && !strcmp("DATE_YYYY", gbuf)) {
        strftime(rbuf, rbuf_size, "%Y", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("DATE_YY", gbuf)) {
        strftime(rbuf, rbuf_size, "%y", tmnow);
        ap = rbuf;
    } else if (tmnow && ((!strcmp("DATE_19XX_YY", gbuf)) ||
                         (!strcmp("DATE_19XX_YYYY", gbuf)))) {
        int year = tmnow->tm_year + 1900;
        int days = year - 2001;
        int leaps = days / 4 - days / 100 + days / 400;
        int lyear =
            ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
        int selector = ((days + leaps + 7) % 7) + lyear * 7;
        static int years[] = {90, 91, 97, 98, 99, 94, 89,
                              96, 80, 92, 76, 88, 72, 84};
        int cal_year = years[selector];

        if (!strcmp("DATE_19XX_YY", gbuf))
            snprintf(rbuf, rbuf_size, "%d", cal_year);
        else
            snprintf(rbuf, rbuf_size, "%d", cal_year + 1900);
        ap = rbuf;
    } else if (tmnow && !strcmp("DATE_MM", gbuf)) {
        strftime(rbuf, rbuf_size, "%m", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("DATE_MMM", gbuf)) {
        strftime(rbuf, rbuf_size, "%b", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("DATE_MONTH", gbuf)) {
        strftime(rbuf, rbuf_size, "%B", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("DATE_DD", gbuf)) {
        strftime(rbuf, rbuf_size, "%d", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("DATE_D", gbuf)) {
        snprintf(rbuf, rbuf_size, "%d",
                 (tmnow->tm_wday ? tmnow->tm_wday : 7));
        ap = rbuf;
    } else if (tmnow &&
               ((!strcmp("DATE_WW", gbuf)) || (!strcmp("DATE_WYYYY", gbuf)))) {
        int iso_yr = tmnow->tm_year + 1900;
        int iso_wk =
            (tmnow->tm_yday + 11 - (tmnow->tm_wday ? tmnow->tm_wday : 7)) / 7;

        if (iso_wk == 0) {
            iso_yr = iso_yr - 1;
            tmnow->tm_yday += 365 + (((iso_yr % 4) == 0) ? 1 : 0);
            iso_wk =
                (tmnow->tm_yday + 11 - (tmnow->tm_wday ? tmnow->tm_wday : 7)) /
                7;
        } else if ((iso_wk == 53) &&
                   (((31 - tmnow->tm_mday) + tmnow->tm_wday) < 4)) {
            ++iso_yr;
            iso_wk = 1;
        }
        if (!strcmp("DATE_WW", gbuf))
            snprintf(rbuf, rbuf_size, "%02d", iso_wk);
        else
            snprintf(rbuf, rbuf_size, "%04d", iso_yr);
        ap = rbuf;
    } else if (tmnow && !strcmp("DATE_JJJ", gbuf)) {
        strftime(rbuf, rbuf_size, "%j", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("TIME_HH", gbuf)) {
        strftime(rbuf, rbuf_size, "%H", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("TIME_MM", gbuf)) {
        strftime(rbuf, rbuf_size, "%M", tmnow);
        ap = rbuf;
    } else if (tmnow && !strcmp("TIME_SS", gbuf)) {
        strftime(rbuf, rbuf_size, "%S", tmnow);
        ap = rbuf;
    } else if (!strcmp("TIME_MSEC", gbuf)) {
        snprintf(rbuf, rbuf_size, "%03d",
                 (int)(cmd_time.tv_nsec / 1000000));
        ap = rbuf;
    } else if (!strcmp("STATUS", gbuf)) {
        snprintf(rbuf, rbuf_size, "%08X", sim_last_cmd_stat);
        ap = rbuf;
    } else if (!strcmp("TSTATUS", gbuf)) {
        t_stat stat = SCPE_BARE_STATUS(sim_last_cmd_stat);

        if ((stat > SCPE_OK) && (stat < SCPE_BASE) &&
            (sim_stop_messages[stat] != NULL))
            strlcpy(rbuf, sim_stop_messages[stat], rbuf_size);
        else
            strlcpy(rbuf, sim_error_text(stat), rbuf_size);
        ap = rbuf;
    } else if (!strcmp("SIM_VERIFY", gbuf)) {
        strlcpy(rbuf, scp_do_echo_enabled() ? "-V" : "", rbuf_size);
        ap = rbuf;
    } else if (!strcmp("SIM_VERBOSE", gbuf)) {
        strlcpy(rbuf, scp_do_echo_enabled() ? "-V" : "", rbuf_size);
        ap = rbuf;
    } else if (!strcmp("SIM_QUIET", gbuf)) {
        strlcpy(rbuf, sim_quiet ? "-Q" : "", rbuf_size);
        ap = rbuf;
    } else if (!strcmp("SIM_MESSAGE", gbuf)) {
        strlcpy(rbuf, sim_show_message ? "" : "-Q", rbuf_size);
        ap = rbuf;
    } else if (!strcmp("SIM_NULL_DEVICE", gbuf)) {
        strlcpy(rbuf, NULL_DEVICE, rbuf_size);
        ap = rbuf;
    } else if (!strcmp("SIM_TMPDIR", gbuf)) {
        ap = sim_host_temp_dir(rbuf, rbuf_size);
    } else if (!strcmp("SIM_NAME", gbuf)) {
        strlcpy(rbuf, sim_name, rbuf_size);
        ap = rbuf;
    } else if (!strcmp("SIM_BIN_PATH", gbuf)) {
        if (sim_prog_name) {
            strlcpy(rbuf, sim_prog_name, rbuf_size);
            ap = rbuf;
        }
    } else if (!strcmp("SIM_BIN_NAME", gbuf)) {
        ap = sim_bin_name_value(rbuf, rbuf_size);
    } else if (!strcmp("SIM_OSTYPE", gbuf)) {
        ap = sim_ostype_value(rbuf, rbuf_size);
    } else if (!strcmp("SIM_RUNLIMIT", gbuf)) {
        if (sim_runlimit_enabled) {
            snprintf(rbuf, rbuf_size, "%d", sim_runlimit_value);
            ap = rbuf;
        }
    } else if (!strcmp("SIM_RUNLIMIT_UNITS", gbuf)) {
        if (sim_runlimit_enabled && sim_runlimit_units) {
            strlcpy(rbuf, sim_runlimit_units, rbuf_size);
            ap = rbuf;
        }
    } else if (!strcmp("_FILE_COMPARE_DIFF_OFFSET", gbuf)) {
        if (sim_file_compare_diff_valid) {
            snprintf(rbuf, rbuf_size, "%zu", sim_file_compare_diff_offset);
            ap = rbuf;
        }
    }
    if (!ap)
        ap = sim_sub_var_get(gbuf);
    if (!ap) {
        for (i = 0; i < sim_external_env_count; i++) {
            if (0 == strcmp(gbuf, sim_external_env[i].name)) {
                ap = sim_external_env[i].value;
                break;
            }
        }
    }
    if (!ap)
        ap = sim_get_host_env_uplowcase(gbuf, rbuf, rbuf_size);
    if (ap && fixup_needed) {
        strlcpy(tgbuf, ap, tgbuf_size);
        sim_subststr_substr(fixup_needed + 1, tgbuf, tgbuf_size);
        strlcpy(rbuf, tgbuf, rbuf_size);
        ap = rbuf;
    }
    free(tgbuf);
    return ap;
}

/* Expand one command line in place using SCP command-variable rules. */
void sim_sub_args(char *instr, size_t instr_size, char *do_arg[])
{
    char gbuf[CBUFSIZE];
    char *ip = instr, *op, *oend, *istart, *tmpbuf;
    const char *ap;
    char rbuf[CBUFSIZE];
    size_t outstr_off = 0;

    scp_set_exp_argv(do_arg);
    if (sim_clock_gettime(CLOCK_REALTIME, &cmd_time) != 0)
        memset(&cmd_time, 0, sizeof(cmd_time));
    tmpbuf = (char *)malloc(instr_size);
    op = tmpbuf;
    oend = tmpbuf + instr_size - 2;
    if (instr_size > sim_sub_instr_size) {
        sim_sub_instr =
            (char *)realloc(sim_sub_instr, instr_size * sizeof(*sim_sub_instr));
        sim_sub_instr_off = (size_t *)realloc(
            sim_sub_instr_off, instr_size * sizeof(*sim_sub_instr_off));
        sim_sub_instr_size = instr_size;
    }
    sim_sub_instr_buf = instr;
    strlcpy(sim_sub_instr, instr, instr_size * sizeof(*sim_sub_instr));
    while (sim_isspace(*ip)) {
        sim_sub_instr_off[outstr_off++] = ip - instr;
        *op++ = *ip++;
    }
    sim_cmdvars_decode_initial_quoted_line(ip, instr_size, instr, op);
    istart = ip;
    for (; *ip && (op < oend);) {
        if ((ip[0] == '%') && (ip[1] == '%')) {
            sim_sub_instr_off[outstr_off++] = ip - instr;
            ip++;
            *op++ = *ip++;
        } else {
            bool expand_it = false;
            char parts[SIM_CMDVARS_PARTS_SIZE];

            if (*ip == '%') {
                const char *percent_ip;
                dynstr_t star_args;
                bool emit_percent;

                dynstr_init(&star_args);
                ++ip;
                percent_ip = ip;
                ap = sim_cmdvars_parse_percent(
                    &percent_ip, gbuf, rbuf, sizeof(rbuf), do_arg, &expand_it,
                    parts, &star_args, &emit_percent);
                ip = (char *)percent_ip;
                if (emit_percent)
                    *op++ = '%';
                sim_cmdvars_expand_and_copy(ap, expand_it, parts, &op, oend,
                                            instr, ip, &outstr_off);
                dynstr_free(&star_args);
            } else {
                if (ip == istart) {
                    if (!sim_cmdvars_expand_initial_token(
                            &ip, &op, oend, instr, gbuf, rbuf, sizeof(rbuf),
                            &outstr_off)) {
                        sim_sub_instr_off[outstr_off++] = ip - instr;
                        *op++ = *ip++;
                        continue;
                    }
                } else {
                    sim_sub_instr_off[outstr_off++] = ip - instr;
                    *op++ = *ip++;
                }
            }
        }
    }
    *op = 0;
    sim_sub_instr_off[outstr_off] = 0;
    strlcpy(instr, tmpbuf, instr_size);
    free(tmpbuf);
}

/* Map a substituted pointer back to its original command text. */
const char *sim_unsub_args(const char *cptr)
{
    if ((cptr > sim_sub_instr_buf) &&
        ((size_t)(cptr - sim_sub_instr_buf) < sim_sub_instr_size))
        return &sim_sub_instr[sim_sub_instr_off[cptr - sim_sub_instr_buf]];
    return cptr;
}

/* Implement SET ENVIRONMENT, including prompt-driven and decoded-string
   forms. */
t_stat sim_set_environment(int32_t flag, const char *cptr)
{
    char varname[CBUFSIZE], prompt[CBUFSIZE], cbuf[CBUFSIZE];

    (void)flag;

    if ((!cptr) || (*cptr == 0))
        return SCPE_2FARG;
    if (sim_switches & SWMASK('P')) {
        const char *deflt = NULL;

        cptr = get_glyph_quoted(cptr, prompt, 0);
        if (prompt[0] == '\0')
            return sim_messagef(
                SCPE_2FARG, "Missing Prompt and Environment Variable Name\n");
        if ((prompt[0] == '"') || (prompt[0] == '\'')) {
            if (strlen(prompt) < 3)
                return sim_messagef(SCPE_ARG, "Invalid Prompt\n");
            prompt[strlen(prompt) - 1] = '\0';
            memmove(prompt, prompt + 1, strlen(prompt));
        }
        deflt = get_glyph(cptr, varname, '=');
        if (varname[0] == '\0')
            return sim_messagef(SCPE_2FARG,
                                "Missing Environment Variable Name\n");
        if (*deflt) {
            strlcat(prompt, " [", sizeof(prompt));
            strlcat(prompt, deflt, sizeof(prompt));
            strlcat(prompt, "] ", sizeof(prompt));
        } else
            strlcat(prompt, " ", sizeof(prompt));
        if (sim_rem_cmd_active_line == -1) {
            cptr = read_line_p(prompt, cbuf, sizeof(cbuf), stdin);
            if ((cptr == NULL) || (*cptr == 0))
                cptr = deflt;
            else
                cptr = cbuf;
        } else
            cptr = deflt;
    } else {
        cptr = get_glyph(cptr, varname, '=');
        strlcpy(cbuf, cptr, sizeof(cbuf));
        sim_trim_endspc(cbuf);
        if (sim_switches & SWMASK('S')) {
            uint32_t str_size;

            cptr = cbuf;
            get_glyph_quoted(cptr, cbuf, 0);
            if (SCPE_OK !=
                sim_decode_quoted_string(cbuf, (uint8_t *)cbuf, &str_size))
                return sim_messagef(SCPE_ARG, "Invalid quoted string: %s\n",
                                    cbuf);
            cbuf[str_size] = '\0';
        } else {
            if (sim_switches & SWMASK('A')) {
                t_svalue val;
                t_stat stat;
                const char *eptr = sim_unsub_args(cptr);

                cptr = sim_eval_expression(eptr, &val, false, &stat);
                if (stat == SCPE_OK) {
                    snprintf(cbuf, sizeof(cbuf), "%ld", (long)val);
                    cptr = cbuf;
                } else
                    return stat;
            }
        }
    }
    setenv(varname, cptr, 1);
    sim_external_env_remove_name(varname);
    return SCPE_OK;
}
