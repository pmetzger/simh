/* scp_help_engine.h: SCP help runtime support

   This header exposes the broader HELP-system support implemented by
   scp_help_engine.c: generic HELP command output, device-help rendering,
   and the shared helpers used by the SCP command layer.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#ifndef SCP_HELP_ENGINE_H_
#define SCP_HELP_ENGINE_H_ 0

#include <stdarg.h>
#include <stdbool.h>

/* Generic SCP and device-help output helpers. */
void fprint_help(FILE *st);

void fprint_reg_help(FILE *st, DEVICE *dptr);

void fprint_reg_help_ex(FILE *st, DEVICE *dptr, bool silent);

void fprint_attach_help_ex(FILE *st, DEVICE *dptr, bool silent);

void fprint_set_help(FILE *st, DEVICE *dptr);

void fprint_set_help_ex(FILE *st, DEVICE *dptr, bool silent);

void fprint_show_help(FILE *st, DEVICE *dptr);

void fprint_show_help_ex(FILE *st, DEVICE *dptr, bool silent);

void fprint_brk_help_ex(FILE *st, DEVICE *dptr, bool silent);

t_stat help_dev_help(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                     const char *cptr);

/* Structured help entry points implemented by the help engine. */
t_stat scp_help(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                const char *help, const char *cptr, ...);

t_stat scp_vhelp(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                 const char *help, const char *cptr, va_list ap);

t_stat scp_helpFromFile(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                        const char *helpfile, const char *cptr, ...);

t_stat scp_vhelpFromFile(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag,
                         const char *helpfile, const char *cptr, va_list ap);

#endif
