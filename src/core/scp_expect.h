/* scp_expect.h: SCP expect/send subsystem interfaces

   This header collects the public command and helper interfaces for the
   SCP expect/send subsystem.  The implementation manages queued console
   input, expect-match rules, and the internal device used to stop the
   simulator when an expect rule fires.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#ifndef SCP_EXPECT_H_
#define SCP_EXPECT_H_

#include <stdbool.h>
#include <stdint.h>

/* Internal device used to stop simulation when an expect rule matches. */
extern DEVICE sim_expect_dev;

/* Identify the internal expect trigger unit in generic SCP code. */
bool sim_expect_is_unit(const UNIT *uptr);

/* SCP command entry points for SEND/NOSEND and EXPECT/NOEXPECT. */
t_stat send_cmd(int32_t flag, const char *ptr);
t_stat expect_cmd(int32_t flag, const char *ptr);

/* SHOW command helpers for queued SEND and EXPECT state. */
t_stat sim_show_send(FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag,
                     const char *cptr);
t_stat sim_show_expect(FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag,
                       const char *cptr);

/* Queue data for SEND processing on a console or multiplexer line. */
t_stat sim_send_input(SEND *snd, uint8_t *data, size_t size, uint32_t after,
                      uint32_t delay);

/* Initialize one SEND context with its typed default timing state. */
void sim_send_init_context(SEND *snd, DEVICE *dptr, uint32_t dbit);

/* Display queued SEND state for a console or multiplexer line. */
t_stat sim_show_send_input(FILE *st, const SEND *snd);

/* Poll queued SEND state and return one pending character when available. */
bool sim_send_poll_data(SEND *snd, t_stat *stat);

/* Clear queued SEND data for a console or multiplexer line. */
t_stat sim_send_clear(SEND *snd);

/* Parse an EXPECT command and install a rule in the target context. */
t_stat sim_set_expect(EXPECT *exp, const char *cptr);

/* Parse a NOEXPECT command and remove rules from the target context. */
t_stat sim_set_noexpect(EXPECT *exp, const char *cptr);

/* Add one expect rule to a target context. */
t_stat sim_exp_set(EXPECT *exp, const char *match, int32_t cnt, uint32_t after,
                   int32_t switches, const char *act);

/* Remove expect rules matching one display-format pattern. */
t_stat sim_exp_clr(EXPECT *exp, const char *match);

/* Remove all expect rules and buffered match state. */
t_stat sim_exp_clrall(EXPECT *exp);

/* Show expect state for one display-format pattern or all rules. */
t_stat sim_exp_show(FILE *st, const EXPECT *exp, const char *match);

/* Show all expect rules for one context. */
t_stat sim_exp_showall(FILE *st, const EXPECT *exp);

/* Feed one byte of output to the expect matcher. */
t_stat sim_exp_check(EXPECT *exp, uint8_t data);

/* Initialize one EXPECT context with its typed default halt state. */
void sim_expect_init_context(EXPECT *exp, DEVICE *dptr, uint32_t dbit);

#endif
