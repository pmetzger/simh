/* scp_breakpoint.c: SCP breakpoint subsystem

   This file contains the reusable breakpoint implementation extracted from
   scp.c.  It owns breakpoint tables, pending breakpoint actions, and the
   shared breakpoint status consumed by simulators and the SCP command layer.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"
#include "scp.h"

/* Shared breakpoint status exported to simulators and SCP command code. */
uint32_t sim_brk_summ = 0;
uint32_t sim_brk_types = 0;
BRKTYPTAB *sim_brk_type_desc = NULL;
uint32_t sim_brk_dflt = 0;
uint32_t sim_brk_match_type;
t_addr sim_brk_match_addr;

/* Private breakpoint table state local to this subsystem. */
static char *sim_brk_act[MAX_DO_NEST_LVL];
static char *sim_brk_act_buf[MAX_DO_NEST_LVL];
static BRKTAB **sim_brk_tab = NULL;
static int32_t sim_brk_ent = 0;
static int32_t sim_brk_lnt = 0;
static int32_t sim_brk_ins = 0;

/* Search one breakpoint address for an entry matching the given type. */
static BRKTAB *sim_brk_fnd_ex(t_addr loc, uint32_t btyp, bool any_typ,
                              uint32_t spc)
{
    BRKTAB *bp = sim_brk_fnd(loc);

    while (bp) {
        if (any_typ ? ((bp->typ & btyp) && (bp->time_fired[spc] != sim_gtime()))
                    : (bp->typ == btyp))
            return bp;
        bp = bp->next;
    }
    return bp;
}

/* Allocate and insert one breakpoint entry at the current insertion point. */
static BRKTAB *sim_brk_new(t_addr loc, uint32_t btyp)
{
    int32_t i, t;
    BRKTAB *bp, **newp;

    if (sim_brk_ins < 0)
        return NULL;
    if (sim_brk_ent >= sim_brk_lnt) {
        t = sim_brk_lnt + SIM_BRK_INILNT;
        newp = (BRKTAB **)calloc(t, sizeof(BRKTAB *));
        if (newp == NULL)
            return NULL;
        memcpy(newp, sim_brk_tab, sim_brk_lnt * sizeof(*sim_brk_tab));
        memset(newp + sim_brk_lnt, 0, SIM_BRK_INILNT * sizeof(*newp));
        free(sim_brk_tab);
        sim_brk_tab = newp;
        sim_brk_lnt = t;
    }
    if ((sim_brk_ins == sim_brk_ent) ||
        ((sim_brk_ins != sim_brk_ent) &&
         (sim_brk_tab[sim_brk_ins]->addr != loc))) {
        for (i = sim_brk_ent; i > sim_brk_ins; --i)
            sim_brk_tab[i] = sim_brk_tab[i - 1];
        sim_brk_tab[sim_brk_ins] = NULL;
    }
    bp = (BRKTAB *)calloc(1, sizeof(*bp));
    bp->next = sim_brk_tab[sim_brk_ins];
    sim_brk_tab[sim_brk_ins] = bp;
    if (bp->next == NULL)
        sim_brk_ent += 1;
    bp->addr = loc;
    bp->typ = btyp;
    bp->cnt = 0;
    bp->act = NULL;
    for (i = 0; i < SIM_BKPT_N_SPC; i++)
        bp->time_fired[i] = -1.0;
    return bp;
}

/* Initialize the global breakpoint tables. */
t_stat sim_brk_init(void)
{
    int32_t i;

    for (i = 0; i < sim_brk_lnt; i++) {
        BRKTAB *bp = sim_brk_tab[i];

        while (bp) {
            BRKTAB *bpt = bp->next;

            free(bp->act);
            free(bp);
            bp = bpt;
        }
    }
    memset(sim_brk_tab, 0, sim_brk_lnt * sizeof(BRKTAB *));
    sim_brk_lnt = SIM_BRK_INILNT;
    sim_brk_tab =
        (BRKTAB **)realloc(sim_brk_tab, sim_brk_lnt * sizeof(BRKTAB *));
    if (sim_brk_tab == NULL)
        return SCPE_MEM;
    memset(sim_brk_tab, 0, sim_brk_lnt * sizeof(BRKTAB *));
    sim_brk_ent = sim_brk_ins = 0;
    sim_brk_clract();
    sim_brk_npc(0);
    return SCPE_OK;
}

/* Search for a breakpoint in the sorted breakpoint table. */
BRKTAB *sim_brk_fnd(t_addr loc)
{
    int32_t lo, hi, p;
    BRKTAB *bp;

    if (sim_brk_ent == 0) {
        sim_brk_ins = 0;
        return NULL;
    }
    lo = 0;
    hi = sim_brk_ent - 1;
    do {
        p = (lo + hi) >> 1;
        bp = sim_brk_tab[p];
        if (loc == bp->addr) {
            sim_brk_ins = p;
            return bp;
        } else if (loc < bp->addr) {
            hi = p - 1;
        } else {
            lo = p + 1;
        }
    } while (lo <= hi);
    if (loc < bp->addr)
        sim_brk_ins = p;
    else
        sim_brk_ins = p + 1;
    return NULL;
}

/* Set a breakpoint of the requested type. */
t_stat sim_brk_set(t_addr loc, int32_t sw, int32_t ncnt, const char *act)
{
    BRKTAB *bp;

    if ((sw == 0) || (sw == BRK_TYP_DYN_STEPOVER))
        sw |= sim_brk_dflt;
    if (~sim_brk_types & sw) {
        char gbuf[CBUFSIZE];

        return sim_messagef(
            SCPE_NOFNC, "Unknown breakpoint type; %s\n",
            put_switches(gbuf, sizeof(gbuf), sw & ~sim_brk_types));
    }
    if ((sw & BRK_TYP_DYN_ALL) && act)
        return SCPE_ARG;
    bp = sim_brk_fnd(loc);
    if (!bp)
        bp = sim_brk_new(loc, sw);
    else {
        while (bp && (bp->typ != (uint32_t)sw))
            bp = bp->next;
        if (!bp)
            bp = sim_brk_new(loc, sw);
    }
    if (!bp)
        return SCPE_MEM;
    bp->cnt = ncnt;
    if ((!(sw & BRK_TYP_DYN_ALL)) && (bp->act != NULL) && (act != NULL)) {
        free(bp->act);
        bp->act = NULL;
    }
    if ((act != NULL) && (*act != 0)) {
        char *newp = (char *)calloc(CBUFSIZE + 1, sizeof(char));

        if (newp == NULL)
            return SCPE_MEM;
        strlcpy(newp, act, CBUFSIZE);
        bp->act = newp;
    }
    sim_brk_summ = sim_brk_summ | (sw & ~BRK_TYP_TEMP);
    return SCPE_OK;
}

/* Clear one breakpoint definition. */
t_stat sim_brk_clr(t_addr loc, int32_t sw)
{
    BRKTAB *bpl = NULL;
    BRKTAB *bp = sim_brk_fnd(loc);
    int32_t i;

    if (!bp)
        return SCPE_OK;
    if (sw == 0)
        sw = SIM_BRK_ALLTYP;

    while (bp) {
        if (bp->typ == (bp->typ & sw)) {
            free(bp->act);
            if (bp == sim_brk_tab[sim_brk_ins])
                bpl = sim_brk_tab[sim_brk_ins] = bp->next;
            else
                bpl->next = bp->next;
            free(bp);
            bp = bpl;
        } else {
            bpl = bp;
            bp = bp->next;
        }
    }
    if (sim_brk_tab[sim_brk_ins] == NULL) {
        sim_brk_ent = sim_brk_ent - 1;
        for (i = sim_brk_ins; i < sim_brk_ent; i++)
            sim_brk_tab[i] = sim_brk_tab[i + 1];
    }
    sim_brk_summ = 0;
    for (i = 0; i < sim_brk_ent; i++) {
        bp = sim_brk_tab[i];
        while (bp) {
            sim_brk_summ |= (bp->typ & ~BRK_TYP_TEMP);
            bp = bp->next;
        }
    }
    return SCPE_OK;
}

/* Clear all breakpoints matching the requested type mask. */
t_stat sim_brk_clrall(int32_t sw)
{
    int32_t i;

    if (sw == 0)
        sw = SIM_BRK_ALLTYP;
    for (i = 0; i < sim_brk_ent;) {
        t_addr loc = sim_brk_tab[i]->addr;
        sim_brk_clr(loc, sw);
        if ((i < sim_brk_ent) && (loc == sim_brk_tab[i]->addr))
            ++i;
    }
    return SCPE_OK;
}

/* Show one breakpoint definition. */
t_stat sim_brk_show(FILE *st, t_addr loc, int32_t sw)
{
    BRKTAB *bp = sim_brk_fnd_ex(loc, sw & (~SWMASK('C')), false, 0);
    DEVICE *dptr;
    int32_t i, any;

    if ((sw == 0) || (sw == SWMASK('C')))
        sw = SIM_BRK_ALLTYP | ((sw == SWMASK('C')) ? SWMASK('C') : 0);
    if (!bp || (!(bp->typ & sw)))
        return SCPE_OK;
    dptr = sim_dflt_dev;
    if (dptr == NULL)
        return SCPE_OK;
    if (sw & SWMASK('C'))
        fprintf(st, "SET BREAK ");
    else {
        if (sim_vm_fprint_addr)
            sim_vm_fprint_addr(st, dptr, loc);
        else
            fprint_val(st, loc, dptr->aradix, dptr->awidth, PV_LEFT);
        fprintf(st, ":\t");
    }
    for (i = any = 0; i < 26; i++) {
        if ((bp->typ >> i) & 1) {
            if ((sw & SWMASK('C')) == 0) {
                if (any)
                    fprintf(st, ", ");
                fputc(i + 'A', st);
            } else {
                fprintf(st, "-%c", i + 'A');
            }
            any = 1;
        }
    }
    if (sw & SWMASK('C')) {
        fprintf(st, " ");
        if (sim_vm_fprint_addr)
            sim_vm_fprint_addr(st, dptr, loc);
        else
            fprint_val(st, loc, dptr->aradix, dptr->awidth, PV_LEFT);
    }
    if (bp->cnt > 0)
        fprintf(st, "[%d]", bp->cnt);
    if (bp->act != NULL)
        fprintf(st, "; %s", bp->act);
    fprintf(st, "\n");
    return SCPE_OK;
}

/* Show all currently defined breakpoints. */
t_stat sim_brk_showall(FILE *st, int32_t sw)
{
    int32_t bit, mask, types;
    BRKTAB **bpt;

    if ((sw == 0) || (sw == SWMASK('C')))
        sw = SIM_BRK_ALLTYP | ((sw == SWMASK('C')) ? SWMASK('C') : 0);
    for (types = bit = 0; bit <= ('Z' - 'A'); bit++)
        if (sim_brk_types & (1 << bit))
            ++types;
    if ((!(sw & SWMASK('C'))) && sim_brk_types && (types > 1)) {
        fprintf(st, "Supported Breakpoint Types:");
        for (bit = 0; bit <= ('Z' - 'A'); bit++)
            if (sim_brk_types & (1 << bit))
                fprintf(st, " -%c", 'A' + bit);
        fprintf(st, "\n");
    }
    if (((sw & sim_brk_types) != sim_brk_types) && (types > 1)) {
        mask = (sw & sim_brk_types);
        fprintf(st, "Displaying Breakpoint Types:");
        for (bit = 0; bit <= ('Z' - 'A'); bit++)
            if (mask & (1 << bit))
                fprintf(st, " -%c", 'A' + bit);
        fprintf(st, "\n");
    }
    for (bpt = sim_brk_tab; bpt < (sim_brk_tab + sim_brk_ent); bpt++) {
        BRKTAB *prev = NULL;
        BRKTAB *cur = *bpt;
        BRKTAB *next;

        while (cur) {
            next = cur->next;
            cur->next = prev;
            prev = cur;
            cur = next;
        }
        *bpt = prev;
        cur = prev;
        while (cur) {
            if (cur->typ & sw)
                sim_brk_show(st, cur->addr,
                             cur->typ | ((sw & SWMASK('C')) ? SWMASK('C') : 0));
            cur = cur->next;
        }
        cur = prev;
        prev = NULL;
        while (cur) {
            next = cur->next;
            cur->next = prev;
            prev = cur;
            cur = next;
        }
        *bpt = prev;
    }
    return SCPE_OK;
}

/* Test one address against the currently defined breakpoint set. */
uint32_t sim_brk_test(t_addr loc, uint32_t btyp)
{
    BRKTAB *bp;
    uint32_t spc = (btyp >> SIM_BKPT_V_SPC) & (SIM_BKPT_N_SPC - 1);

    if (sim_brk_summ & BRK_TYP_DYN_ALL)
        btyp |= BRK_TYP_DYN_ALL;

    if ((bp = sim_brk_fnd_ex(loc, btyp, true, spc))) {
        double s_gtime = sim_gtime();

        if (bp->time_fired[spc] == s_gtime)
            return 0;
        bp->time_fired[spc] = s_gtime;
        if (--bp->cnt > 0)
            return 0;
        bp->cnt = 0;
        sim_brk_setact(bp->act);
        sim_brk_match_type = btyp & bp->typ;
        if (bp->typ & BRK_TYP_TEMP)
            sim_brk_clr(loc, bp->typ);
        sim_brk_match_addr = loc;
        return sim_brk_match_type;
    }
    return 0;
}

/* Get the next pending breakpoint action command, if any. */
const char *sim_brk_getact(char *buf, int32_t size)
{
    char *ep;
    size_t lnt;
    int32_t do_depth = scp_do_depth();

    if (sim_brk_act[do_depth] == NULL)
        return NULL;
    while (sim_isspace(*sim_brk_act[do_depth]))
        sim_brk_act[do_depth]++;
    if (*sim_brk_act[do_depth] == 0)
        return sim_brk_clract();
    ep = strpbrk(sim_brk_act[do_depth], ";\"'");
    if ((ep != NULL) && (*ep != ';')) {
        char quote = *ep++;

        while ((ep[0] != '\0') && (ep[0] != quote))
            if ((ep[0] == '\\') && (ep[1] == quote))
                ep = ep + 2;
            else
                ep = ep + 1;
        ep = strchr(ep, ';');
    }
    if (ep != NULL) {
        lnt = ep - sim_brk_act[do_depth];
        memcpy(buf, sim_brk_act[do_depth], lnt + 1);
        buf[lnt] = 0;
        sim_brk_act[do_depth] += lnt + 1;
    } else {
        strlcpy(buf, sim_brk_act[do_depth], size);
        sim_brk_act[do_depth] = NULL;
        sim_brk_clract();
    }
    sim_trim_endspc(buf);
    sim_debug(SIM_DBG_BRK_ACTION, &sim_scp_dev,
              "sim_brk_getact(%d) - Returning: '%s'\n", do_depth, buf);
    return buf;
}

/* Clear any pending breakpoint actions for the current DO depth. */
char *sim_brk_clract(void)
{
    int32_t do_depth = scp_do_depth();

    if (sim_brk_act[do_depth])
        sim_debug(SIM_DBG_BRK_ACTION, &sim_scp_dev,
                  "sim_brk_clract(%d) - Clearing: '%s'\n", do_depth,
                  sim_brk_act[do_depth]);
    free(sim_brk_act_buf[do_depth]);
    return sim_brk_act[do_depth] = sim_brk_act_buf[do_depth] = NULL;
}

/* Install or prepend the current breakpoint action string. */
void sim_brk_setact(const char *action)
{
    int32_t do_depth = scp_do_depth();

    if (action) {
        if (sim_brk_act[do_depth] && (*sim_brk_act[do_depth])) {
            size_t old_size = strlen(sim_brk_act[do_depth]);
            size_t new_size = strlen(action) + old_size + 3;
            char *old_action = strdup(sim_brk_act[do_depth]);

            sim_brk_act_buf[do_depth] =
                (char *)realloc(sim_brk_act_buf[do_depth], new_size);
            strlcpy(sim_brk_act_buf[do_depth], action, new_size);
            strlcat(sim_brk_act_buf[do_depth], "; ", new_size);
            strlcat(sim_brk_act_buf[do_depth], old_action, new_size);
            sim_debug(SIM_DBG_BRK_ACTION, &sim_scp_dev,
                      "sim_brk_setact(%d) - Pushed: '%s' ahead of: '%s'\n",
                      do_depth, action, old_action);
            free(old_action);
        } else {
            size_t action_size = strlen(action) + 1;

            sim_brk_act_buf[do_depth] =
                (char *)realloc(sim_brk_act_buf[do_depth], action_size);
            strlcpy(sim_brk_act_buf[do_depth], action, action_size);
            sim_debug(SIM_DBG_BRK_ACTION, &sim_scp_dev,
                      "sim_brk_setact(%d) - Set to: '%s'\n", do_depth, action);
        }
        sim_brk_act[do_depth] = sim_brk_act_buf[do_depth];
    } else {
        sim_brk_clract();
    }
}

/* Replace the current action buffer and return the previous allocation. */
char *sim_brk_replace_act(char *new_action)
{
    int32_t do_depth = scp_do_depth();
    char *old_action = sim_brk_act_buf[do_depth];

    sim_brk_act_buf[do_depth] = new_action;
    return old_action;
}

/* Mark all breakpoint spaces as unfired after the PC changes. */
void sim_brk_npc(uint32_t cnt)
{
    uint32_t spc;
    BRKTAB **bpt, *bp;

    if ((cnt == 0) || (cnt > SIM_BKPT_N_SPC))
        cnt = SIM_BKPT_N_SPC;
    for (bpt = sim_brk_tab; bpt < (sim_brk_tab + sim_brk_ent); bpt++) {
        for (bp = *bpt; bp; bp = bp->next) {
            for (spc = 0; spc < cnt; spc++)
                bp->time_fired[spc] = -1.0;
        }
    }
}

/* Clear fired timestamps for one breakpoint space and type mask. */
void sim_brk_clrspc(uint32_t spc, uint32_t btyp)
{
    BRKTAB **bpt, *bp;

    if (spc < SIM_BKPT_N_SPC) {
        for (bpt = sim_brk_tab; bpt < (sim_brk_tab + sim_brk_ent); bpt++) {
            for (bp = *bpt; bp; bp = bp->next) {
                if (bp->typ & btyp)
                    bp->time_fired[spc] = -1.0;
            }
        }
    }
}

/* Format the most recent breakpoint hit into a user-facing message. */
const char *sim_brk_message(void)
{
    static char msg[256];
    char addr[MAX_WIDTH + 1];
    char buf[32];

    msg[0] = '\0';
    if (sim_vm_sprint_addr)
        sim_vm_sprint_addr(addr, sim_dflt_dev, (t_value)sim_brk_match_addr);
    else
        sprint_val(addr, (t_value)sim_brk_match_addr, sim_dflt_dev->aradix,
                   sim_dflt_dev->awidth, PV_LEFT);
    if (sim_brk_type_desc) {
        BRKTYPTAB *brk = sim_brk_type_desc;

        while (2 == strlen(put_switches(buf, sizeof(buf), brk->btyp))) {
            if (brk->btyp == sim_brk_match_type) {
                snprintf(msg, sizeof(msg), "%s: %s", brk->desc, addr);
                break;
            }
            brk++;
        }
    }
    if (!msg[0])
        snprintf(msg, sizeof(msg), "%s Breakpoint at: %s\n",
                 put_switches(buf, sizeof(buf), sim_brk_match_type), addr);

    return msg;
}
