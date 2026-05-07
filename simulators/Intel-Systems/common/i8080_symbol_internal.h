/* i8080_symbol_internal.h: internal Intel 8080 symbolic I/O helpers */
// SPDX-FileCopyrightText: 1997-2005 Charles E. Owen
// SPDX-FileCopyrightText: 2011 William A. Beech
// SPDX-License-Identifier: X11

#ifndef I8080_SYMBOL_INTERNAL_H_
#define I8080_SYMBOL_INTERNAL_H_ 0

#include "system_defs.h"

t_stat fprint_sym(FILE *of, t_addr addr, t_value *val, UNIT *uptr, int32 sw);
t_stat parse_sym(const char *cptr, t_addr addr, UNIT *uptr, t_value *val,
                 int32 sw);

/*
 * Return the encoded instruction length for one i8080 opcode, or zero for an
 * invalid opcode.
 */
int32 i8080_symbol_instruction_length(uint8 opcode);

#endif
