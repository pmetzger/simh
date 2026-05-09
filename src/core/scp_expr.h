/* scp_expr.h: SCP expression evaluation interface

   This header exposes the shared expression-evaluation entrypoints used
   by the simulator control program.  The implementation owns the parser,
   operator tables, and temporary evaluation state.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#ifndef SCP_EXPR_H
#define SCP_EXPR_H

#include <stdbool.h>
#include "sim_defs.h"

/* Install the active argument vector used for $-style substitutions. */
void scp_set_exp_argv(char **argv);

/* Evaluate one SCP expression and return the unconsumed remainder. */
const char *sim_eval_expression(const char *cptr, t_svalue *value,
                                bool parens_required, t_stat *stat);

#endif
