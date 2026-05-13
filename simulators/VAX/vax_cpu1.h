/* vax_cpu1.h: VAX complex-instruction interfaces */
// SPDX-FileCopyrightText: 1998-2017 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_CPU1_H_
#define VAX_CPU1_H_ 1

#include "vax_defs.h"

int32_t op_bb_n(uint32_t *opnd, int32_t acc);
int32_t op_bb_x(uint32_t *opnd, int32_t newb, int32_t acc);
int32_t op_extv(uint32_t *opnd, int32_t vfldrp1, int32_t acc);
void op_insv(uint32_t *opnd, int32_t vfldrp1, int32_t acc);
int32_t op_ffs(uint32_t fld, int32_t size);
int32_t op_call(uint32_t *opnd, bool gs, int32_t acc);
int32_t op_ret(int32_t acc);
int32_t op_insque(uint32_t *opnd, int32_t acc);
int32_t op_remque(uint32_t *opnd, int32_t acc);
int32_t op_insqhi(uint32_t *opnd, int32_t acc);
int32_t op_insqti(uint32_t *opnd, int32_t acc);
int32_t op_remqhi(uint32_t *opnd, int32_t acc);
int32_t op_remqti(uint32_t *opnd, int32_t acc);
void op_pushr(uint32_t *opnd, int32_t acc);
void op_popr(uint32_t *opnd, int32_t acc);
int32_t op_movc(uint32_t *opnd, int32_t opc, int32_t acc);
int32_t op_cmpc(uint32_t *opnd, int32_t opc, int32_t acc);
int32_t op_locskp(uint32_t *opnd, int32_t opc, int32_t acc);
int32_t op_scnspn(uint32_t *opnd, int32_t opc, int32_t acc);
int32_t op_chm(uint32_t *opnd, int32_t cc, int32_t opc);
int32_t op_rei(int32_t acc);
void op_ldpctx(int32_t acc);
void op_svpctx(int32_t acc);
int32_t op_probe(uint32_t *opnd, int32_t opc);
int32_t op_mtpr(uint32_t *opnd);
int32_t op_mfpr(uint32_t *opnd);
int32_t intexc(int32_t vec, int32_t cc, int32_t ipl, int ei);

#endif /* VAX_CPU1_H_ */
