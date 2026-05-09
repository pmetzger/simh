/* scp_breakpoint.h: SCP breakpoint subsystem interfaces

   This header collects the breakpoint APIs and shared state extracted from
   scp.c.  The implementation manages breakpoint tables, pending actions,
   and the public status used by simulators when checking breakpoint hits.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#ifndef SCP_BREAKPOINT_H_
#define SCP_BREAKPOINT_H_

#include <stdint.h>

/* Breakpoint status shared with simulators and the SCP command layer. */
#define SIM_BRK_INILNT 4096
#define SIM_BRK_ALLTYP 0xFFFFFFFB

extern uint32_t sim_brk_types;
extern uint32_t sim_brk_dflt;
extern uint32_t sim_brk_summ;
extern uint32_t sim_brk_match_type;
extern t_addr sim_brk_match_addr;
extern BRKTYPTAB *sim_brk_type_desc;

/* Initialize the global breakpoint tables. */
t_stat sim_brk_init(void);

/* Set, clear, and display breakpoint definitions. */
t_stat sim_brk_set(t_addr loc, int32_t sw, int32_t ncnt, const char *act);
t_stat sim_brk_clr(t_addr loc, int32_t sw);
t_stat sim_brk_clrall(int32_t sw);
t_stat sim_brk_show(FILE *st, t_addr loc, int32_t sw);
t_stat sim_brk_showall(FILE *st, int32_t sw);

/* Find and test breakpoint table entries. */
BRKTAB *sim_brk_fnd(t_addr loc);
uint32_t sim_brk_test(t_addr bloc, uint32_t btyp);

/* Clear breakpoint-match timestamps for one space or all spaces. */
void sim_brk_clrspc(uint32_t spc, uint32_t btyp);
void sim_brk_npc(uint32_t cnt);

/* Manage pending breakpoint actions. */
const char *sim_brk_getact(char *buf, int32_t size);
char *sim_brk_clract(void);
void sim_brk_setact(const char *action);
char *sim_brk_replace_act(char *new_action);
const char *sim_brk_message(void);

#endif
