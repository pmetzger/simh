/* scp_parse.h: shared SCP token and switch parsing helpers

   These helpers were extracted from scp.c so command parsing logic can
   be reused by smaller core modules and by host-side tests without
   dragging in the full command loop implementation.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#ifndef SIM_SCP_PARSE_H_
#define SIM_SCP_PARSE_H_ 1

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"

/* Result codes returned by switch parsing. */
typedef enum { SW_ERROR, SW_BITMASK, SW_NUMBER } SWITCH_PARSE;

/* Parse one token with selectable case-folding and quote handling. */
const char *get_glyph_gen(const char *iptr, char *optr, char mchar, bool uc,
                          bool quote, char escape_char);

/* Parse the next token and fold alphabetic characters to upper case. */
const char *get_glyph(const char *iptr, char *optr, char mchar);

/* Parse the next token without changing its case. */
const char *get_glyph_nc(const char *iptr, char *optr, char mchar);

/* Parse one token, allowing it to be enclosed in quotes. */
const char *get_glyph_quoted(const char *iptr, char *optr, char mchar);

/* Parse the leading command token, handling SCP's special '!' form. */
const char *get_glyph_cmd(const char *iptr, char *optr);

/* Decode either symbolic switches or a numeric switch argument. */
SWITCH_PARSE get_switches(const char *cptr, int32_t *sw, int32_t *number);

/* Consume leading simulator switches from an SCP command string. */
const char *get_sim_sw(const char *cptr);

#endif
