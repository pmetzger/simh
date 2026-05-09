/* scp_help_engine.c: SCP help runtime and device-help support

   This file contains the structured help runtime extracted from scp.c.
   It owns generic HELP output, device-help rendering, and the VMS-style
   topic parser used by SCP and device help strings.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#include "sim_defs.h"
#include "sim_disk.h"
#include "sim_tape.h"
#include "sim_ether.h"
#include "sim_card.h"
#include "sim_tmxr.h"
#include "sim_tempfile.h"
#include "scp.h"
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>

/* Used when sorting a list of command names. */
static int _cmd_name_compare(const void *pa, const void *pb)
{
    CTAB *const *a = (CTAB *const *)pa;
    CTAB *const *b = (CTAB *const *)pb;

    return strcmp((*a)->name, (*b)->name);
}

void fprint_help(FILE *st)
{
    CTAB *cmd_table = scp_cmd_table();
    CTAB *cmdp;
    CTAB **hlp_cmdp = NULL;
    size_t cmd_cnt = 0;
    size_t cmd_size = 0;
    size_t max_cmdname_size = 0;
    size_t i, line_offset;

    for (cmdp = sim_vm_cmd; cmdp && (cmdp->name != NULL); cmdp++) {
        if (cmdp->help) {
            if (cmd_cnt >= cmd_size) {
                cmd_size += 20;
                hlp_cmdp =
                    (CTAB **)realloc(hlp_cmdp, sizeof(*hlp_cmdp) * cmd_size);
            }
            hlp_cmdp[cmd_cnt] = cmdp;
            ++cmd_cnt;
            if (strlen(cmdp->name) > max_cmdname_size)
                max_cmdname_size = strlen(cmdp->name);
        }
    }
    for (cmdp = cmd_table; cmdp && (cmdp->name != NULL); cmdp++) {
        if (cmdp->help &&
            (NULL == sim_vm_cmd || NULL == find_ctab(sim_vm_cmd, cmdp->name))) {
            if (cmd_cnt >= cmd_size) {
                cmd_size += 20;
                hlp_cmdp =
                    (CTAB **)realloc(hlp_cmdp, sizeof(*hlp_cmdp) * cmd_size);
            }
            hlp_cmdp[cmd_cnt] = cmdp;
            ++cmd_cnt;
            if (strlen(cmdp->name) > max_cmdname_size)
                max_cmdname_size = strlen(cmdp->name);
        }
    }
    fprintf(st, "Help is available for devices\n\n");
    fprintf(st, "   HELP dev\n");
    fprintf(st, "   HELP dev SET\n");
    fprintf(st, "   HELP dev SHOW\n");
    fprintf(st, "   HELP dev REGISTERS\n\n");
    fprintf(st, "Help is available for the following commands:\n\n    ");
    if (hlp_cmdp)
        qsort(hlp_cmdp, cmd_cnt, sizeof(*hlp_cmdp), _cmd_name_compare);
    line_offset = 4;
    for (i = 0; i < cmd_cnt; ++i) {
        fputs(hlp_cmdp[i]->name, st);
        line_offset += 5 + max_cmdname_size;
        if (line_offset + max_cmdname_size > 79) {
            line_offset = 4;
            fprintf(st, "\n    ");
        } else
            fprintf(st, "%*s",
                    (int)(max_cmdname_size + 5 - strlen(hlp_cmdp[i]->name)),
                    "");
    }
    free(hlp_cmdp);
    fprintf(st, "\n");
}

static void fprint_header(FILE *st, bool *pdone, char *context)
{
    if (!*pdone)
        fprintf(st, "%s", context);
    *pdone = true;
}

static void fprint_wrapped(FILE *st, const char *buf, size_t width,
                           const char *gap, const char *extra, size_t max_width)
{
    size_t line_pos = MAX(width, strlen(buf));
    const char *end;

    if ((strlen(buf) >= max_width) && (NULL != strchr(buf, '=')) &&
        (NULL != strchr(strchr(buf, '='), ';'))) {
        size_t chunk_size;
        const char *front_gap = strchr(buf, '=');
        size_t front_gap_size = front_gap - buf + 1;

        line_pos = 0;
        end = buf + strlen(buf);
        while (1) {
            chunk_size = (end - buf);
            if (line_pos + chunk_size >= max_width)
                chunk_size = max_width - line_pos - 1;
            else
                break;
            while ((chunk_size > 0) && (buf[chunk_size - 1] != ';'))
                --chunk_size;
            if (chunk_size == 0)
                chunk_size = strlen(buf);
            fprintf(st, "%*s%*.*s\n", (int)line_pos, "", (int)chunk_size,
                    (int)chunk_size, buf);
            buf += chunk_size;
            while (isspace(buf[0]))
                ++buf;
            if (buf < end)
                line_pos = front_gap_size;
        }
        fprintf(st, "%*s%*.*s", (int)line_pos, "", (int)chunk_size,
                (int)chunk_size, buf);
        line_pos = width + 1;
    } else
        fprintf(st, "%*s", -((int)width), buf);
    if (line_pos > width) {
        fprintf(st, "\n");
        if (extra == NULL)
            return;
        buf = "";
        line_pos = width;
        fprintf(st, "%*s", -((int)width), buf);
    }
    end = extra + (extra ? strlen(extra) : 0);
    line_pos += (gap ? strlen(gap) : 0);
    if (line_pos + (end - extra) >= max_width) {
        size_t chunk_size;

        while (1) {
            chunk_size = (end - extra);
            if (line_pos + chunk_size >= max_width)
                chunk_size = max_width - line_pos - 1;
            else
                break;
            while ((chunk_size > 0) && (extra[chunk_size] != ' '))
                --chunk_size;
            if (chunk_size == 0)
                chunk_size = strlen(extra);
            fprintf(st, "%s%*.*s\n", gap ? gap : "", (int)chunk_size,
                    (int)chunk_size, extra);
            extra += chunk_size;
            while (isspace(extra[0]))
                ++extra;
            if (extra < end) {
                line_pos = width;
                fprintf(st, "%*s", -((int)width), "");
                line_pos += (gap ? strlen(gap) : 0);
            } else
                return;
        }
    }
    fprintf(st, "%s%s\n", gap ? gap : "", extra ? extra : "");
}

void fprint_reg_help_ex(FILE *st, DEVICE *dptr, bool silent)
{
    REG *rptr, *trptr;
    bool found = false;
    bool all_unique = true;
    size_t max_namelen = 0;
    DEVICE *tdptr;
    const char *tptr;
    char *namebuf;
    char rangebuf[32];
    int side_effects = 0;

    if (!silent)
        fprintf(st, "\n");
    if (dptr->registers)
        for (rptr = dptr->registers; rptr->name != NULL; rptr++) {
            if (rptr->flags & REG_HIDDEN)
                continue;
            side_effects += ((rptr->flags & REG_DEPOSIT) != 0);
            if (rptr->depth > 1)
                snprintf(rangebuf, sizeof(rangebuf), "[%d:%d]", 0,
                         rptr->depth - 1);
            else
                rangebuf[0] = '\0';
            if (max_namelen < (strlen(rptr->name) + strlen(rangebuf)))
                max_namelen = strlen(rptr->name) + strlen(rangebuf);
            found = true;
            trptr = find_reg_glob(rptr->name, &tptr, &tdptr);
            if ((trptr == NULL) || (tdptr != dptr))
                all_unique = false;
        }
    if (!found) {
        if (!silent)
            fprintf(st, "No register help is available for the %s device\n",
                    dptr->name);
    } else {
        int i,
            hdr_len = 13 + (int)(max_namelen +
                                 (all_unique ? 0 : (1 + strlen(dptr->name))) +
                                 ((side_effects > 0) ? 0 : 1));

        namebuf = (char *)calloc(max_namelen + 1, sizeof(*namebuf));
        fprintf(st, "\nThe %s device implements these registers:\n\n",
                dptr->name);
        fprintf(
            st, "  %*s Size %*sPurpose\n",
            -((int)(max_namelen + (all_unique ? 0 : (1 + strlen(dptr->name))))),
            "Name", (side_effects > 0) ? 0 : 1, "");
        fprintf(st, "  ");
        for (i = 0; i < hdr_len; i++)
            fprintf(st, "-");
        fprintf(st, "\n");
        for (rptr = dptr->registers; rptr->name != NULL; rptr++) {
            char note[2];
            char buf[CBUFSIZE];

            if (rptr->flags & REG_HIDDEN)
                continue;
            strlcpy(note,
                    (side_effects != 0)
                        ? ((rptr->flags & REG_DEPOSIT) ? "+" : " ")
                        : "",
                    sizeof(note));
            if (rptr->depth <= 1)
                snprintf(namebuf, max_namelen + 1, "%*s", -((int)max_namelen),
                         rptr->name);
            else {
                snprintf(rangebuf, sizeof(rangebuf), "[%d:%d]", 0,
                         rptr->depth - 1);
                snprintf(namebuf, max_namelen + 1, "%s%*s", rptr->name,
                         (int)(strlen(rptr->name)) - ((int)max_namelen),
                         rangebuf);
            }
            if (all_unique) {
                snprintf(buf, sizeof(buf), "  %s %4d %s", namebuf, rptr->width,
                         note);
                fprint_wrapped(st, buf, strlen(buf), " ", rptr->desc, 80);
                continue;
            }
            trptr = find_reg_glob(rptr->name, &tptr, &tdptr);
            if ((trptr == NULL) || (tdptr != dptr))
                snprintf(buf, sizeof(buf), "  %s %s %4d %s", dptr->name,
                         namebuf, rptr->width, note);
            else
                snprintf(buf, sizeof(buf), "  %*s %s %4d %s",
                         (int)strlen(dptr->name), "", namebuf, rptr->width,
                         note);
            fprint_wrapped(st, buf, strlen(buf), " ", rptr->desc, 80);
        }
        free(namebuf);
        if (side_effects)
            fprintf(
                st,
                "\n  +  Deposits to %s register%s will have some additional\n"
                "     side effects which can be suppressed if the deposit is\n"
                "     done with the -Z switch specified\n",
                (side_effects == 1) ? "this" : "these",
                (side_effects == 1) ? "" : "s");
    }
}

void fprint_reg_help(FILE *st, DEVICE *dptr)
{
    fprint_reg_help_ex(st, dptr, true);
}

void fprint_attach_help_ex(FILE *st, DEVICE *dptr, bool silent)
{
    if (dptr->attach_help) {
        fprintf(st, "\n%s device attach commands:\n\n", dptr->name);
        dptr->attach_help(st, dptr, NULL, 0, NULL);
        return;
    }
    if (DEV_TYPE(dptr) == DEV_MUX) {
        fprintf(st, "\n%s device attach commands:\n\n", dptr->name);
        tmxr_attach_help(st, dptr, NULL, 0, NULL);
        return;
    }
    if (DEV_TYPE(dptr) == DEV_DISK) {
        fprintf(st, "\n%s device attach commands:\n\n", dptr->name);
        sim_disk_attach_help(st, dptr, NULL, 0, NULL);
        return;
    }
    if (DEV_TYPE(dptr) == DEV_TAPE) {
        fprintf(st, "\n%s device attach commands:\n\n", dptr->name);
        sim_tape_attach_help(st, dptr, NULL, 0, NULL);
        return;
    }
    if (DEV_TYPE(dptr) == DEV_ETHER) {
        fprintf(st, "\n%s device attach commands:\n\n", dptr->name);
        eth_attach_help(st, dptr, NULL, 0, NULL);
        return;
    }
#if defined(USE_SIM_CARD) && defined(SIM_CARD_API)
    if (DEV_TYPE(dptr) == DEV_CARD) {
        fprintf(st, "\n%s device attach commands:\n\n", dptr->name);
        sim_card_attach_help(st, dptr, NULL, 0, NULL);
        return;
    }
#endif
    if (!silent) {
        fprintf(st, "\nNo ATTACH help is available for the %s device\n",
                dptr->name);
        if (dptr->help)
            dptr->help(st, dptr, NULL, 0, NULL);
    }
}

void fprint_set_help_ex(FILE *st, DEVICE *dptr, bool silent)
{
    MTAB *mptr;
    DEBTAB *dep;
    bool found = false;
    bool deb_desc_available = false;
    char buf[CBUFSIZE], header[CBUFSIZE], extra[CBUFSIZE];
    uint32 enabled_units = dptr->numunits;
    char unit_spec[50];
    uint32 unit, found_unit = 0;
    const char *gap = "  ";

    snprintf(header, sizeof(header), "\n%s device SET commands:\n\n",
             dptr->name);
    for (unit = 0; unit < dptr->numunits; unit++)
        if (dptr->units[unit].flags & UNIT_DIS)
            --enabled_units;
        else
            found_unit = unit;
    if (enabled_units == 1) {
        if (found_unit == 0)
            snprintf(unit_spec, sizeof(unit_spec), "%s", sim_dname(dptr));
        else
            snprintf(unit_spec, sizeof(unit_spec), "%s%u", sim_dname(dptr),
                     found_unit);
    } else
        snprintf(unit_spec, sizeof(unit_spec), "%sn", sim_dname(dptr));
    if (dptr->modifiers) {
        for (mptr = dptr->modifiers; mptr->mask != 0; mptr++) {
            if (!MODMASK(mptr, MTAB_VDV) && MODMASK(mptr, MTAB_VUN) &&
                (dptr->numunits != 1))
                continue;
            if ((enabled_units != 1) && !(mptr->mask & MTAB_XTD))
                continue;
            if (mptr->mstring) {
                fprint_header(st, &found, header);
                snprintf(
                    buf, sizeof(buf), "set %s %s%s", sim_dname(dptr),
                    mptr->mstring,
                    (strchr(mptr->mstring, '='))
                        ? ""
                        : (MODMASK(mptr, MTAB_VALR)
                               ? "=val"
                               : (MODMASK(mptr, MTAB_VALO) ? "{=val}" : "")));
                if ((mptr->valid != NULL) && (mptr->disp != NULL) &&
                    (mptr->help != NULL)) {
                    char gbuf[CBUFSIZE];
                    const char *rem;

                    rem = get_glyph(mptr->help, gbuf, 0);
                    if ((strcasecmp(gbuf, "Display") == 0) ||
                        (strcasecmp(gbuf, "Show") == 0)) {
                        char *thelp = (char *)malloc(9 + strlen(rem));

                        snprintf(thelp, 9 + strlen(rem), "Specify %s", rem);
                        fprint_wrapped(st, buf, 30, gap, thelp, 80);
                        free(thelp);
                    } else
                        fprint_wrapped(st, buf, 30, gap, mptr->help, 80);
                } else
                    fprint_wrapped(st, buf, 30, gap, mptr->help, 80);
            }
        }
    }
    if (dptr->flags & DEV_DISABLE) {
        fprint_header(st, &found, header);
        snprintf(buf, sizeof(buf), "set %s ENABLE", sim_dname(dptr));
        snprintf(extra, sizeof(extra), "Enables device %s", sim_dname(dptr));
        fprint_wrapped(st, buf, 30, gap, extra, 80);
        snprintf(buf, sizeof(buf), "set %s DISABLE", sim_dname(dptr));
        snprintf(extra, sizeof(extra), "Disables device %s", sim_dname(dptr));
        fprint_wrapped(st, buf, 30, gap, extra, 80);
    }
    if ((dptr->flags & DEV_DEBUG) || (dptr->debflags)) {
        fprint_header(st, &found, header);
        snprintf(buf, sizeof(buf), "set %s DEBUG", sim_dname(dptr));
        snprintf(extra, sizeof(extra), "Enables debugging for device %s",
                 sim_dname(dptr));
        fprint_wrapped(st, buf, 30, gap, extra, 80);
        snprintf(buf, sizeof(buf), "set %s NODEBUG", sim_dname(dptr));
        snprintf(extra, sizeof(extra), "Disables debugging for device %s",
                 sim_dname(dptr));
        fprint_wrapped(st, buf, 30, gap, extra, 80);
        if (dptr->debflags) {
            snprintf(buf, sizeof(buf), "set %s DEBUG=", sim_dname(dptr));
            for (dep = dptr->debflags; dep->name != NULL; dep++) {
                strlcat(buf, ((dep == dptr->debflags) ? "" : ";"), sizeof(buf));
                strlcat(buf, dep->name, sizeof(buf));
                deb_desc_available |=
                    ((dep->desc != NULL) && (dep->desc[0] != '\0'));
            }
            snprintf(extra, sizeof(extra),
                     "Enables specific debugging for device %s",
                     sim_dname(dptr));
            fprint_wrapped(st, buf, 30, gap, extra, 80);
            snprintf(buf, sizeof(buf), "set %s NODEBUG=", sim_dname(dptr));
            for (dep = dptr->debflags; dep->name != NULL; dep++) {
                strlcat(buf, ((dep == dptr->debflags) ? "" : ";"), sizeof(buf));
                strlcat(buf, dep->name, sizeof(buf));
            }
            snprintf(extra, sizeof(extra),
                     "Disables specific debugging for device %s",
                     sim_dname(dptr));
            fprint_wrapped(st, buf, 30, gap, extra, 80);
        }
    }
    if ((dptr->modifiers) && (dptr->units)) {
        if (dptr->units->flags & UNIT_DISABLE) {
            fprint_header(st, &found, header);
            snprintf(buf, sizeof(buf), "set %s ENABLE", unit_spec);
            snprintf(extra, sizeof(extra), "Enables unit %s", unit_spec);
            fprint_wrapped(st, buf, 30, gap, extra, 80);
            snprintf(buf, sizeof(buf), "set %s DISABLE", unit_spec);
            snprintf(extra, sizeof(extra), "Disables unit %s", unit_spec);
            fprint_wrapped(st, buf, 30, gap, extra, 80);
        }
        if (((dptr->flags & DEV_DEBUG) || (dptr->debflags)) &&
            ((DEV_TYPE(dptr) == DEV_DISK) || (DEV_TYPE(dptr) == DEV_TAPE))) {
            snprintf(buf, sizeof(buf), "set %s DEBUG", unit_spec);
            snprintf(extra, sizeof(extra),
                     "Enables debugging for device unit %s", unit_spec);
            fprint_wrapped(st, buf, 30, gap, extra, 80);
            snprintf(buf, sizeof(buf), "set %s NODEBUG", unit_spec);
            snprintf(extra, sizeof(extra),
                     "Disables debugging for device unit %s", unit_spec);
            fprint_wrapped(st, buf, 30, gap, extra, 80);
            if (dptr->debflags) {
                buf[0] = '\0';
                fprintf(st, "set %s DEBUG=", unit_spec);
                for (dep = dptr->debflags; dep->name != NULL; dep++)
                    fprintf(st, "%s%s", ((dep == dptr->debflags) ? "" : ";"),
                            dep->name);
                fprintf(st, "\n");
                snprintf(extra, sizeof(extra),
                         "Enables specific debugging for device unit %s",
                         unit_spec);
                fprint_wrapped(st, "", 30, gap, extra, 80);
                fprintf(st, "set %s NODEBUG=", unit_spec);
                for (dep = dptr->debflags; dep->name != NULL; dep++)
                    fprintf(st, "%s%s", ((dep == dptr->debflags) ? "" : ";"),
                            dep->name);
                fprintf(st, "\n");
                snprintf(extra, sizeof(extra),
                         "Disables specific debugging for device unit %s",
                         unit_spec);
                fprint_wrapped(st, "", 30, gap, extra, 80);
            }
        }
        for (mptr = dptr->modifiers; mptr->mask != 0; mptr++) {
            if ((!MODMASK(mptr, MTAB_VUN)) && MODMASK(mptr, MTAB_XTD))
                continue;
            if ((NULL == mptr->valid) && MODMASK(mptr, MTAB_XTD))
                continue;
            if ((enabled_units == 1) && (found_unit == 0))
                continue;
            if (mptr->mstring) {
                fprint_header(st, &found, header);
                snprintf(
                    buf, sizeof(buf), "set %s %s%s", unit_spec, mptr->mstring,
                    (strchr(mptr->mstring, '='))
                        ? ""
                        : (MODMASK(mptr, MTAB_VALR)
                               ? "=val"
                               : (MODMASK(mptr, MTAB_VALO) ? "{=val}" : "")));
                if ((mptr->valid != NULL) && (mptr->disp != NULL) &&
                    (mptr->help != NULL)) {
                    char gbuf[CBUFSIZE];
                    const char *rem;

                    rem = get_glyph(mptr->help, gbuf, 0);
                    if ((strcasecmp(gbuf, "Display") == 0) ||
                        (strcasecmp(gbuf, "Show") == 0)) {
                        char *thelp = (char *)malloc(9 + strlen(rem));

                        snprintf(thelp, 9 + strlen(rem), "Specify %s", rem);
                        fprint_wrapped(st, buf, 30, gap, thelp, 80);
                        free(thelp);
                    } else
                        fprint_wrapped(st, buf, 30, gap, mptr->help, 80);
                } else
                    fprint_wrapped(st, buf, 30, gap, mptr->help, 80);
            }
        }
    }
    if (enabled_units) {
        for (unit = 0; unit < dptr->numunits; unit++)
            if ((!(dptr->units[unit].flags & UNIT_DIS)) &&
                (dptr->units[unit].flags & UNIT_SEQ) &&
                (!(dptr->units[unit].flags & UNIT_MUSTBUF))) {
                snprintf(buf, sizeof(buf), "set %s%s APPEND",
                         sim_uname(&dptr->units[unit]),
                         (enabled_units > 1) ? "n" : "");
                snprintf(extra, sizeof(extra), "Sets %s%s position to EOF",
                         sim_uname(&dptr->units[unit]),
                         (enabled_units > 1) ? "n" : "");
                fprint_wrapped(st, buf, 30, gap, extra, 80);
                break;
            }
    }
    if (deb_desc_available) {
        fprintf(st, "\n*%s device DEBUG settings:\n", sim_dname(dptr));
        for (dep = dptr->debflags; dep->name != NULL; dep++)
            fprintf(st, "%4s%-12s%s\n", "", dep->name,
                    dep->desc ? dep->desc : "");
    }
    if (!found && !silent)
        fprintf(st, "No SET help is available for the %s device\n", dptr->name);
}

void fprint_set_help(FILE *st, DEVICE *dptr)
{
    fprint_set_help_ex(st, dptr, true);
}

void fprint_show_help_ex(FILE *st, DEVICE *dptr, bool silent)
{
    MTAB *mptr;
    bool found = false;
    char buf[CBUFSIZE], header[CBUFSIZE], extra[CBUFSIZE];
    uint32 enabled_units = dptr->numunits;
    char unit_spec[50];
    uint32 unit, found_unit = 0;
    const char *gap = "  ";

    snprintf(header, sizeof(header), "\n%s device SHOW commands:\n\n",
             dptr->name);
    for (unit = 0; unit < dptr->numunits; unit++)
        if (dptr->units[unit].flags & UNIT_DIS)
            --enabled_units;
        else
            found_unit = unit;
    if (enabled_units == 1)
        snprintf(unit_spec, sizeof(unit_spec), "%s%u", sim_dname(dptr),
                 found_unit);
    else
        snprintf(unit_spec, sizeof(unit_spec), "%sn", sim_dname(dptr));
    if (dptr->modifiers) {
        for (mptr = dptr->modifiers; mptr->mask != 0; mptr++) {
            if (!MODMASK(mptr, MTAB_VDV) && MODMASK(mptr, MTAB_VUN) &&
                (dptr->numunits != 1))
                continue;
            if ((enabled_units != 1) && !(mptr->mask & MTAB_XTD))
                continue;
            if ((!mptr->disp) || (!mptr->pstring) || !(*mptr->pstring))
                continue;
            fprint_header(st, &found, header);
            snprintf(buf, sizeof(buf), "show %s %s%s", sim_dname(dptr),
                     mptr->pstring, MODMASK(mptr, MTAB_SHP) ? "{=arg}" : "");
            fprint_wrapped(st, buf, 30, gap, mptr->help, 80);
        }
    }
    if ((dptr->flags & DEV_DEBUG) || (dptr->debflags)) {
        fprint_header(st, &found, header);
        snprintf(buf, sizeof(buf), "show %s DEBUG", sim_dname(dptr));
        snprintf(extra, sizeof(extra),
                 "Displays debugging status for device %s",
                 sim_dname(dptr));
        fprint_wrapped(st, buf, 30, gap, extra, 80);
    }
    if (((dptr->flags & DEV_DEBUG) || (dptr->debflags)) &&
        ((DEV_TYPE(dptr) == DEV_DISK) || (DEV_TYPE(dptr) == DEV_TAPE))) {
        snprintf(buf, sizeof(buf), "show %s DEBUG", unit_spec);
        snprintf(extra, sizeof(extra),
                 "Displays debugging status for device unit %s",
                 unit_spec);
        fprint_wrapped(st, buf, 30, gap, extra, 80);
    }

    if ((dptr->modifiers) && (dptr->units)) {
        for (mptr = dptr->modifiers; mptr->mask != 0; mptr++) {
            if ((!MODMASK(mptr, MTAB_VUN)) && MODMASK(mptr, MTAB_XTD))
                continue;
            if ((!mptr->disp) || (!mptr->pstring))
                continue;
            fprint_header(st, &found, header);
            snprintf(buf, sizeof(buf), "show %s %s%s", unit_spec,
                     mptr->pstring, MODMASK(mptr, MTAB_SHP) ? "=arg" : "");
            fprint_wrapped(st, buf, 30, gap, mptr->help, 80);
        }
    }
    if (!found && !silent)
        fprintf(st, "No SHOW help is available for the %s device\n",
                dptr->name);
}

void fprint_show_help(FILE *st, DEVICE *dptr)
{
    fprint_show_help_ex(st, dptr, true);
}

void fprint_brk_help_ex(FILE *st, DEVICE *dptr, bool silent)
{
    BRKTYPTAB *brkt = dptr->brk_types;
    char gbuf[CBUFSIZE];

    fprintf(st, "\n");
    if (sim_brk_types == 0) {
        if ((dptr != sim_dflt_dev) && (!silent)) {
            fprintf(st, "Breakpoints are not supported in the %s simulator\n",
                    sim_name);
            if (dptr->help)
                dptr->help(st, dptr, NULL, 0, NULL);
        }
        return;
    }
    if (brkt == NULL) {
        int i;

        if (dptr == sim_dflt_dev) {
            if (sim_brk_types & ~sim_brk_dflt) {
                fprintf(st, "%s supports the following breakpoint types:\n",
                        sim_dname(dptr));
                for (i = 0; i < 26; i++) {
                    if (sim_brk_types & (1 << i))
                        fprintf(st, "  -%c\n", 'A' + i);
                }
            }
            fprintf(st, "The default breakpoint type is: %s\n",
                    put_switches(gbuf, sizeof(gbuf), sim_brk_dflt));
        }
        return;
    }
    fprintf(st, "%s supports the following breakpoint types:\n",
            sim_dname(dptr));
    while (brkt->btyp) {
        fprintf(st, "  %s     %s\n",
                put_switches(gbuf, sizeof(gbuf), brkt->btyp), brkt->desc);
        ++brkt;
    }
    fprintf(st, "The default breakpoint type is: %s\n",
            put_switches(gbuf, sizeof(gbuf), sim_brk_dflt));
}

t_stat help_dev_help(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                     const char *cptr)
{
    char gbuf[CBUFSIZE];
    CTAB *cmdp;

    if (*cptr) {
        get_glyph(cptr, gbuf, 0);
        if ((cmdp = find_cmd(gbuf))) {
            if (cmdp->action == &exdep_cmd) {
                if (dptr->help)
                    return dptr->help(st, dptr, uptr, flag, cmdp->name);
                else
                    fprintf(st,
                            "No device specific help available for the %s %s "
                            "command\n",
                            cmdp->name, sim_dname(dptr));
                return SCPE_OK;
            }
            if (cmdp->action == &set_cmd) {
                fprint_set_help_ex(st, dptr, false);
                return SCPE_OK;
            }
            if (cmdp->action == &show_cmd) {
                fprint_show_help_ex(st, dptr, false);
                return SCPE_OK;
            }
            if (cmdp->action == &attach_cmd) {
                fprint_attach_help_ex(st, dptr, false);
                return SCPE_OK;
            }
            if (cmdp->action == &brk_cmd) {
                fprint_brk_help_ex(st, dptr, false);
                return SCPE_OK;
            }
            if (dptr->help)
                return dptr->help(st, dptr, uptr, flag, cptr);
            fprintf(st, "No %s help is available for the %s device\n",
                    cmdp->name, dptr->name);
            return SCPE_OK;
        }
        if (MATCH_CMD(gbuf, "REGISTERS") == 0) {
            fprint_reg_help_ex(st, dptr, false);
            return SCPE_OK;
        }
        if (dptr->help)
            return dptr->help(st, dptr, uptr, flag, cptr);
        fprintf(st, "No %s help is available for the %s device\n", gbuf,
                dptr->name);
        return SCPE_OK;
    }
    if (dptr->help) {
        return dptr->help(st, dptr, uptr, flag, cptr);
    }
    if (dptr->description)
        fprintf(st, "%s %s help\n", dptr->description(dptr), dptr->name);
    else
        fprintf(st, "%s help\n", dptr->name);
    fprint_set_help_ex(st, dptr, true);
    fprint_show_help_ex(st, dptr, true);
    fprint_attach_help_ex(st, dptr, true);
    fprint_reg_help_ex(st, dptr, true);
    fprint_brk_help_ex(st, dptr, true);
    return SCPE_OK;
}

/*
 * This is a relatively rich piece of code, and ought to be in a module
 * of its own.  If you don't use the macros, it is not necessary to
 * #include "scp_help.h".  Indeed, since the available strings may have
 * other device references, it can be used for non-device help.
 */

#define blankch(x) ((x) == ' ' || (x) == '\t')

typedef struct topic {
    size_t level;
    char *title;
    char *label;
    struct topic *parent;
    struct topic **children;
    uint32 kids;
    char *text;
    size_t len;
    uint32 flags;
    size_t kidwid;
#define HLP_MAGIC_TOPIC 1
} TOPIC;

static volatile struct {
    const char *error;
    const char *prox;
    size_t block;
    size_t line;
} help_where = {"", NULL, 0, 0};
jmp_buf help_env;
#define FAIL(why, text, here)                                                  \
    {                                                                          \
        help_where.error = #text;                                              \
        help_where.prox = here;                                                \
        longjmp(help_env, (why));                                              \
    }

static void appendText(TOPIC *topic, const char *text, size_t len)
{
    char *newt;

    if (!len)
        return;

    newt = (char *)realloc(topic->text, topic->len + len + 1);
    if (!newt) {
        FAIL(SCPE_MEM, No memory, NULL);
    }
    topic->text = newt;
    memcpy(newt + topic->len, text, len);
    topic->len += len;
    newt[topic->len] = '\0';
}

static void cleanHelp(TOPIC *topic)
{
    TOPIC *child;
    size_t i;

    free(topic->title);
    free(topic->text);
    free(topic->label);
    for (i = 0; i < topic->kids; i++) {
        child = topic->children[i];
        cleanHelp(child);
        free(child);
    }
    free(topic->children);
}

static TOPIC *buildHelp(TOPIC *topic, DEVICE *dptr, UNIT *uptr,
                        const char *htext, va_list ap)
{
    char *end;
    size_t n, ilvl;
#define VSMAX 100
    char *vstrings[VSMAX];
    size_t vsnum = 0;
    char *astrings[VSMAX + 1];
    size_t asnum = 0;
    char *const *hblock;
    const char *ep;
    bool excluded = false;

    memset(vstrings, 0, sizeof(vstrings));
    memset(astrings, 0, sizeof(astrings));
    astrings[asnum++] = (char *)htext;

    for (hblock = astrings; (htext = *hblock) != NULL; hblock++) {
        help_where.block = hblock - astrings;
        help_where.line = 0;
        while (*htext) {
            const char *start;

            help_where.line++;
            if (sim_isspace(*htext) || *htext == '+') {
                if (excluded) {
                    while (*htext && *htext != '\n')
                        htext++;
                    if (*htext)
                        ++htext;
                    continue;
                }
                ilvl = 1;
                appendText(topic, "    ", 4);
                if (*htext == '+') {
                    while (*htext == '+') {
                        ilvl++;
                        appendText(topic, "    ", 4);
                        htext++;
                    }
                }
                while (*htext && *htext != '\n' && sim_isspace(*htext))
                    htext++;
                if (!*htext)
                    break;
                start = htext;
                while (*htext) {
                    if (*htext == '%') {
                        appendText(topic, start, htext - start);
                        switch (*++htext) {
                        case 'U':
                            if (dptr) {
                                char buf[129];
                                n = uptr ? uptr - dptr->units : 0;
                                snprintf(buf, sizeof(buf), "%s%u",
                                         dptr->name, (int)n);
                                appendText(topic, buf, strlen(buf));
                            }
                            break;
                        case 'D':
                            if (dptr)
                                appendText(topic, dptr->name,
                                           strlen(dptr->name));
                            break;
                        case 'S':
                            appendText(topic, sim_name, strlen(sim_name));
                            break;
                        case 'C':
                            appendText(topic, sim_vm_interval_units,
                                       strlen(sim_vm_interval_units));
                            break;
                        case 'I':
                            appendText(topic, sim_vm_step_unit,
                                       strlen(sim_vm_step_unit));
                            break;
                        case '%':
                            appendText(topic, "%", 1);
                            break;
                        case '+':
                            appendText(topic, "+", 1);
                            break;
                        default:
                            if (sim_isdigit(*htext)) {
                                n = 0;
                                while (sim_isdigit(*htext))
                                    n += (n * 10) + (*htext++ - '0');
                                if ((*htext != 'H' && *htext != 's') ||
                                    n == 0 || n >= VSMAX)
                                    FAIL(SCPE_ARG, Invalid escape, htext);
                                while (n > vsnum)
                                    vstrings[vsnum++] = va_arg(ap, char *);
                                start = vstrings[n - 1];
                                if (*htext == 'H') {
                                    if (asnum >= VSMAX) {
                                        FAIL(SCPE_ARG, Too many blocks, htext);
                                    }
                                    astrings[asnum++] = (char *)start;
                                    break;
                                }
                                ep = start;
                                while (*ep) {
                                    if (*ep == '\n') {
                                        ep++;
                                        appendText(topic, start, ep - start);
                                        if (*ep) {
                                            size_t i;
                                            for (i = 0; i < ilvl; i++)
                                                appendText(topic, "    ", 4);
                                        }
                                        start = ep;
                                    } else
                                        ep++;
                                }
                                appendText(topic, start, ep - start);
                                break;
                            }
                            FAIL(SCPE_ARG, Invalid escape, htext);
                        }
                        start = ++htext;
                        continue;
                    }
                    if (*htext == '\n') {
                        htext++;
                        appendText(topic, start, htext - start);
                        break;
                    }
                    htext++;
                }
                continue;
            }
            if (sim_isdigit(*htext)) {
                TOPIC **children;
                TOPIC *newt;
                char nbuf[100];

                n = 0;
                start = htext;
                while (sim_isdigit(*htext))
                    n += (n * 10) + (*htext++ - '0');
                if ((htext == start) || !n) {
                    FAIL(SCPE_ARG, Invalid topic heading, htext);
                }
                if (n <= topic->level) {
                    while (n <= topic->level)
                        topic = topic->parent;
                } else {
                    if (n > topic->level + 1) {
                        FAIL(SCPE_ARG, Level not contiguous, htext);
                    }
                }
                while (*htext && (*htext != '\n') && sim_isspace(*htext))
                    htext++;
                if (!*htext || (*htext == '\n')) {
                    FAIL(SCPE_ARG, Missing topic name, htext);
                }
                start = htext;
                while (*htext && (*htext != '\n'))
                    htext++;
                if (start == htext) {
                    FAIL(SCPE_ARG, Null topic name, htext);
                }
                excluded = false;
                if (*start == '?') {
                    size_t n = 0;
                    start++;
                    while (sim_isdigit(*start))
                        n += (n * 10) + (*start++ - '0');
                    if (!*start || *start == '\n' || n == 0 || n >= VSMAX)
                        FAIL(SCPE_ARG, Invalid parameter number, start);
                    while (n > vsnum)
                        vstrings[vsnum++] = va_arg(ap, char *);
                    end = vstrings[n - 1];
                    if (!end || !(sim_toupper(*end) == 'T' || *end == '1')) {
                        excluded = true;
                        if (*htext)
                            htext++;
                        continue;
                    }
                }
                newt = (TOPIC *)calloc(1, sizeof(TOPIC));
                if (!newt) {
                    FAIL(SCPE_MEM, No memory, NULL);
                }
                newt->title = (char *)malloc((htext - start) + 1);
                if (!newt->title) {
                    free(newt);
                    FAIL(SCPE_MEM, No memory, NULL);
                }
                memcpy(newt->title, start, htext - start);
                newt->title[htext - start] = '\0';
                if (*htext)
                    htext++;

                if (newt->title[0] == '$')
                    newt->flags |= HLP_MAGIC_TOPIC;

                children = (TOPIC **)realloc(
                    topic->children, (topic->kids + 1) * sizeof(TOPIC *));
                if (NULL == children) {
                    free(newt->title);
                    free(newt);
                    FAIL(SCPE_MEM, No memory, NULL);
                }
                topic->children = children;
                topic->children[topic->kids++] = newt;
                newt->level = n;
                newt->parent = topic;
                n = strlen(newt->title);
                if (n > topic->kidwid)
                    topic->kidwid = n;
                snprintf(nbuf, sizeof(nbuf), ".%u", topic->kids);
                n = strlen(topic->label) + strlen(nbuf) + 1;
                newt->label = (char *)malloc(n);
                if (NULL == newt->label) {
                    free(newt->title);
                    topic->children[topic->kids - 1] = NULL;
                    free(newt);
                    FAIL(SCPE_MEM, No memory, NULL);
                }
                snprintf(newt->label, n, "%s%s", topic->label, nbuf);
                topic = newt;
                continue;
            }
            if (*htext == ';') {
                while (*htext && *htext != '\n')
                    htext++;
                continue;
            }
            FAIL(SCPE_ARG, Unknown line type, htext);
        }
        memset(vstrings, 0, VSMAX * sizeof(char *));
        vsnum = 0;
    }

    return topic;
}

static char *helpPrompt(TOPIC *topic, const char *pstring, bool oneword)
{
    char *prefix;
    char *newp, *newt;
    size_t newpsz;

    if (topic->level == 0) {
        prefix = (char *)calloc(2, 1);
        if (!prefix) {
            FAIL(SCPE_MEM, No memory, NULL);
        }
        prefix[0] = '\n';
    } else
        prefix = helpPrompt(topic->parent, "", oneword);

    newpsz = strlen(prefix) + 1 + strlen(topic->title) + 1 +
             strlen(pstring) + 1;
    newp = (char *)malloc(newpsz);
    if (!newp) {
        free(prefix);
        FAIL(SCPE_MEM, No memory, NULL);
    }
    strlcpy(newp, prefix, newpsz);
    if (topic->children) {
        if (topic->level != 0)
            strlcat(newp, " ", newpsz);
        newt =
            (topic->flags & HLP_MAGIC_TOPIC) ? topic->title + 1 : topic->title;
        if (oneword) {
            char *np = newp + strlen(newp);
            while (*newt) {
                *np++ = blankch(*newt) ? '_' : *newt;
                newt++;
            }
            *np = '\0';
        } else
            strlcat(newp, newt, newpsz);
        if (*pstring && *pstring != '?')
            strlcat(newp, " ", newpsz);
    }
    strlcat(newp, pstring, newpsz);
    free(prefix);
    return newp;
}

static void displayMagicTopic(FILE *st, DEVICE *dptr, TOPIC *topic)
{
    char tbuf[CBUFSIZE];
    char tmp_path[PATH_MAX + 1];
    size_t i, skiplines = 0;
    FILE *tmp =
        sim_tempfile_open_stream(tmp_path, sizeof(tmp_path), "simh-help-",
                                 NULL, "w+");

    if (!tmp) {
        fprintf(st, "Unable to create temporary file: %s\n", strerror(errno));
        return;
    }

    if (topic->title) {
        fprintf(st, "%s\n", topic->title + 1);

        skiplines = 0;
        if (dptr) {
            if (!strcmp(topic->title + 1, "Registers")) {
                fprint_reg_help(tmp, dptr);
                skiplines = 1;
            } else if (!strcmp(topic->title + 1, "Set commands")) {
                fprint_set_help(tmp, dptr);
                skiplines = 3;
            } else if (!strcmp(topic->title + 1, "Show commands")) {
                fprint_show_help(tmp, dptr);
                skiplines = 3;
            }
        }
    }
    rewind(tmp);

    for (i = 0; i < skiplines; i++)
        if (fgets(tbuf, sizeof(tbuf), tmp)) {
        };

    while (fgets(tbuf, sizeof(tbuf), tmp)) {
        if (tbuf[0] != '\n')
            fputs("    ", st);
        fputs(tbuf, st);
    }
    fclose(tmp);
    remove(tmp_path);
}

static t_stat displayFlatHelp(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                              TOPIC *topic, va_list ap)
{
    size_t i;

    if (topic->flags & HLP_MAGIC_TOPIC) {
        fprintf(st, "\n%s ", topic->label);
        displayMagicTopic(st, dptr, topic);
    } else
        fprintf(st, "\n%s %s\n", topic->label, topic->title);

    if (topic->text)
        fputs(topic->text, st);

    for (i = 0; i < topic->kids; i++)
        displayFlatHelp(st, dptr, uptr, flag, topic->children[i], ap);

    return SCPE_OK;
}

#define HLP_MATCH_AMBIGUOUS (~0u)
#define HLP_MATCH_WILDCARD (~1U)
#define HLP_MATCH_NONE 0
static size_t matchHelpTopicName(TOPIC *topic, const char *token)
{
    size_t i, match;
    char cbuf[CBUFSIZE], *cptr;

    if (!strcmp(token, "*"))
        return HLP_MATCH_WILDCARD;

    match = 0;
    for (i = 0; i < topic->kids; i++) {
        strlcpy(cbuf,
                topic->children[i]->title +
                    ((topic->children[i]->flags & HLP_MAGIC_TOPIC) ? 1 : 0),
                sizeof(cbuf));
        cptr = cbuf;
        while (*cptr) {
            if (blankch(*cptr)) {
                *cptr++ = '_';
            } else {
                *cptr = (char)sim_toupper(*cptr);
                cptr++;
            }
        }
        if (!strcmp(cbuf, token))
            return i + 1;
        if (!strncmp(cbuf, token, strlen(token))) {
            if (match)
                return HLP_MATCH_AMBIGUOUS;
            match = i + 1;
        }
    }
    return match;
}

t_stat scp_vhelp(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                 const char *help, const char *cptr, va_list ap)
{

    TOPIC top;
    TOPIC *topic = &top;
    int failed;
    size_t match;
    size_t i;
    const char *p;
    bool flat_help = false;
    char cbuf[CBUFSIZE], gbuf[CBUFSIZE];

    static const char attach_help[] = {" ATTACH"};
    static const char brief_help[] = {
        "%s help.  Type <CR> to exit, HELP for navigation help"};
    static const char onecmd_help[] = {"%s help."};
    static const char help_help[] = {
        "    This help command provides hierarchical help.  To see more "
        "information,\n"
        "    type an offered subtopic name.  To move back a level, just type "
        "<CR>.\n"
        "    To review the current topic/subtopic, type \"?\".\n"
        "    To view all subtopics, type \"*\".\n"
        "    To exit help at any time, type EXIT.\n"};

    memset(&top, 0, sizeof(top));
    top.parent = &top;
    if ((failed = setjmp(help_env)) != 0) {
        fprintf(stderr,
                "\nHelp was unable to process the help for this device.\n"
                "Error in block %u line %u: %s\n"
                "%s%*.*s%s"
                " Please contact the device maintainer.\n",
                (int)help_where.block, (int)help_where.line, help_where.error,
                help_where.prox ? "Near '" : "", help_where.prox ? 15 : 0,
                help_where.prox ? 15 : 0,
                help_where.prox ? help_where.prox : "",
                help_where.prox ? "'" : "");
        cleanHelp(&top);
        return failed;
    }

    if (dptr) {
        p = dptr->name;
        flat_help = (dptr->flags & DEV_FLATHELP) != 0;
    } else
        p = sim_name;
    top.title = (char *)malloc(
        strlen(p) + ((flag & SCP_HELP_ATTACH) ? sizeof(attach_help) - 1 : 0) +
        1);
    for (i = 0; p[i]; i++)
        top.title[i] = (char)sim_toupper(p[i]);
    top.title[i] = '\0';
    if (flag & SCP_HELP_ATTACH)
        memcpy(top.title + i, attach_help, sizeof(attach_help));

    top.label = (char *)malloc(sizeof("1"));
    strlcpy(top.label, "1", 2);

    flat_help = flat_help || !sim_ttisatty() || (flag & SCP_HELP_FLAT);

    if (flat_help) {
        flag |= SCP_HELP_FLAT;
        if (sim_ttisatty() && !scp_has_oline())
            fprintf(
                st,
                "%s help.\nThis help is also available in hierarchical form.\n",
                top.title);
        else
            fprintf(st, "%s help.\n", top.title);
    } else
        fprintf(st, ((flag & SCP_HELP_ONECMD) ? onecmd_help : brief_help),
                top.title);

    (void)buildHelp(&top, dptr, uptr, help, ap);

    while (cptr && *cptr) {
        cptr = get_glyph(cptr, gbuf, 0);
        if (!gbuf[0])
            break;
        if (!strcmp(gbuf, "HELP")) {
            fprintf(st, "\n");
            fputs(help_help, st);
            break;
        }
        match = matchHelpTopicName(topic, gbuf);
        if (match == HLP_MATCH_WILDCARD) {
            displayFlatHelp(st, dptr, uptr, flag, topic, ap);
            cleanHelp(&top);
            return SCPE_OK;
        }
        if (match == HLP_MATCH_AMBIGUOUS) {
            fprintf(st, "\n%s is ambiguous in %s\n", gbuf, topic->title);
            break;
        }
        if (match == HLP_MATCH_NONE) {
            fprintf(st, "\n%s is not available in %s\n", gbuf, topic->title);
            break;
        }
        topic = topic->children[match - 1];
    }
    cptr = NULL;

    if (flat_help) {
        displayFlatHelp(st, dptr, uptr, flag, topic, ap);
        cleanHelp(&top);
        return SCPE_OK;
    }

    while (true) {
        char *pstring;
        const char *prompt[2] = {"? ", "Subtopic? "};

        if (topic->flags & HLP_MAGIC_TOPIC) {
            fputc('\n', st);
            displayMagicTopic(st, dptr, topic);
        } else
            fprintf(st, "\n%s\n", topic->title);

        if (topic->text)
            fputs(topic->text, st);

        if (topic->kids) {
            size_t w = 0;
            char *p;
            char tbuf[CBUFSIZE];

            fprintf(st, "\n    Additional information available:\n\n");
            for (i = 0; i < topic->kids; i++) {
                strlcpy(
                    tbuf,
                    topic->children[i]->title +
                        ((topic->children[i]->flags & HLP_MAGIC_TOPIC) ? 1 : 0),
                    sizeof(tbuf));
                for (p = tbuf; *p; p++) {
                    if (blankch(*p))
                        *p = '_';
                }
                w += 4 + topic->kidwid;
                if (w > 80) {
                    w = 4 + topic->kidwid;
                    fputc('\n', st);
                }
                fprintf(st, "    %-*s", (int)topic->kidwid, tbuf);
            }
            fprintf(st, "\n\n");
            if (flag & SCP_HELP_ONECMD) {
                pstring = helpPrompt(topic, "", true);
                fprintf(st,
                        "To view additional topics, type HELP %s topicname\n",
                        pstring + 1);
                free(pstring);
                break;
            }
        }

        if (!sim_ttisatty() || (flag & SCP_HELP_ONECMD))
            break;

    reprompt:
        if (!cptr || !*cptr) {
            if (topic->kids == 0)
                topic = topic->parent;
            pstring = helpPrompt(topic, prompt[topic->kids != 0], false);

            cptr = read_line_p(pstring, cbuf, sizeof(cbuf), stdin);
            free(pstring);
            if ((cptr != NULL) &&
                ((0 == strcmp(cptr, "\x04")) || (0 == strcmp(cptr, "\x1A"))))
                cptr = NULL;
        }

        if (!cptr) {
#if !defined(_WIN32)
            printf("\n");
#endif
            break;
        }

        cptr = get_glyph(cptr, gbuf, 0);
        if (!strcmp(gbuf, "*")) {
            displayFlatHelp(st, dptr, uptr, flag, topic, ap);
            gbuf[0] = '\0';
        }
        if (!gbuf[0]) {
            if (topic->level == 0)
                break;
            topic = topic->parent;
            continue;
        }
        if (!strcmp(gbuf, "?"))
            continue;
        if (!strcmp(gbuf, "HELP")) {
            fputs(help_help, st);
            goto reprompt;
        }
        if (!strcmp(gbuf, "EXIT") || !strcmp(gbuf, "QUIT"))
            break;

        if (!topic->kids) {
            fprintf(st, "No additional help at this level.\n");
            cptr = NULL;
            goto reprompt;
        }
        match = matchHelpTopicName(topic, gbuf);
        if (match == HLP_MATCH_AMBIGUOUS) {
            fprintf(st, "%s is ambiguous, please type more of the topic name\n",
                    gbuf);
            cptr = NULL;
            goto reprompt;
        }

        if (match == HLP_MATCH_NONE) {
            fprintf(st, "Help for %s is not available\n", gbuf);
            cptr = NULL;
            goto reprompt;
        }

        topic = topic->children[match - 1];
    }

    cleanHelp(&top);

    return SCPE_OK;
}

t_stat scp_help(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                const char *help, const char *cptr, ...)
{
    t_stat r;
    va_list ap;

    va_start(ap, cptr);
    r = scp_vhelp(st, dptr, uptr, flag, help, cptr, ap);
    va_end(ap);

    return r;
}

t_stat scp_vhelpFromFile(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                         const char *helpfile, const char *cptr, va_list ap)
{
    FILE *fp;
    char *help, *p;
    t_offset size, n;
    int c;
    t_stat r;

    fp = sim_fopen(helpfile, "r");
    if (fp == NULL) {
        const char *argv0 = scp_argv0();

        if (argv0 && *argv0) {
            char fbuf[(4 * PATH_MAX) + 1];
            const char *separator = NULL;

            fbuf[sizeof(fbuf) - 1] = '\0';
            strlcpy(fbuf, argv0, sizeof(fbuf));
            if ((p = (char *)match_ext(fbuf, "EXE")))
                *p = '\0';
            if ((p = strrchr(fbuf, '\\'))) {
                p[1] = '\0';
                separator = "\\";
            } else {
                if ((p = strrchr(fbuf, '/'))) {
                    p[1] = '\0';
                    separator = "/";
                }
            }
            if (p && (strlen(fbuf) + strlen(helpfile) + 1) <= sizeof(fbuf)) {
                strlcat(fbuf, helpfile, sizeof(fbuf));
                fp = sim_fopen(fbuf, "r");
            }
            if (!fp && p &&
                (strlen(fbuf) + strlen(separator) + sizeof("help") +
                 strlen(helpfile) + 1) <= sizeof(fbuf)) {
                snprintf(p + 1, sizeof(fbuf) - ((p + 1) - fbuf), "help%s",
                         separator);
                strlcat(p + 1, helpfile, sizeof(fbuf) - ((p + 1) - fbuf));
                fp = sim_fopen(fbuf, "r");
            }
        }
    }
    if (fp == NULL) {
        fprintf(stderr, "Unable to open %s\n", helpfile);
        return SCPE_UNATT;
    }

    size = sim_fsize_ex(fp);

    help = (char *)malloc((size_t)size + 1);
    if (!help) {
        fclose(fp);
        return SCPE_MEM;
    }
    p = help;
    n = 0;

    while ((c = fgetc(fp)) != EOF) {
        if (++n > size) {
#define XPANDQ 512
            p = (char *)realloc(help, ((size_t)size) + XPANDQ + 1);
            if (!p) {
                free(help);
                fclose(fp);
                return SCPE_MEM;
            }
            help = p;
            size += XPANDQ;
            p += n;
        }
        *p++ = (char)c;
    }
    *p++ = '\0';

    fclose(fp);

    r = scp_vhelp(st, dptr, uptr, flag, help, cptr, ap);
    free(help);

    return r;
}

t_stat scp_helpFromFile(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                        const char *helpfile, const char *cptr, ...)
{
    t_stat r;
    va_list ap;

    va_start(ap, cptr);
    r = scp_vhelpFromFile(st, dptr, uptr, flag, helpfile, cptr, ap);
    va_end(ap);

    return r;
}
