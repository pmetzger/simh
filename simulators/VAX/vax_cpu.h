/* vax_cpu.h: VAX CPU simulator interfaces */
// SPDX-FileCopyrightText: 1998-2017 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_CPU_H_
#define VAX_CPU_H_ 1

#include "vax_defs.h"

extern uint32_t cpu_idle_mask; /* idle mask */
extern int32_t extra_bytes; /* bytes referenced by current string instruction */
extern BITFIELD cpu_psl_bits[];
extern const uint32_t byte_mask[33];

int32_t cpu_emulate_exception(uint32_t *opnd, int32_t cc, int32_t opc,
                              int32_t acc);
void cpu_idle(void);
t_stat cpu_load_bootcode(const char *filename, const uchar_t *builtin_code,
                         size_t size, bool rom, t_addr offset);
t_stat cpu_show_model(FILE *st, UNIT *uptr, int32_t val, const void *desc);
t_stat cpu_set_instruction_set(UNIT *uptr, int32_t val, const char *cptr,
                               void *desc);
t_stat cpu_show_instruction_set(FILE *st, UNIT *uptr, int32_t val,
                                const void *desc);
t_stat cpu_help(FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag,
                const char *cptr);

#endif /* VAX_CPU_H_ */
