/* vax_fpa.h: VAX floating-point instruction interfaces */
// SPDX-FileCopyrightText: 1998-2012 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_FPA_H_
#define VAX_FPA_H_ 1

#include <stdbool.h>
#include <stdint.h>

int32_t op_ashq(uint32_t *opnd, int32_t *rh, int32_t *flg);
int32_t op_emul(int32_t mpy, int32_t mpc, int32_t *rh);
int32_t op_ediv(uint32_t *opnd, int32_t *rh, int32_t *flg);
int32_t op_cmpfd(int32_t h1, int32_t l1, int32_t h2, int32_t l2);
int32_t op_cmpg(int32_t h1, int32_t l1, int32_t h2, int32_t l2);
int32_t op_cvtifdg(int32_t val, int32_t *rh, int32_t opc);
int32_t op_cvtfdgi(uint32_t *opnd, int32_t *flg, int32_t opc);
int32_t op_emodf(uint32_t *opnd, int32_t *intgr, int32_t *flg);
int32_t op_emodd(uint32_t *opnd, int32_t *rh, int32_t *intgr, int32_t *flg);
int32_t op_emodg(uint32_t *opnd, int32_t *rh, int32_t *intgr, int32_t *flg);
int32_t op_movfd(int32_t val);
int32_t op_mnegfd(int32_t val);
int32_t op_movg(int32_t val);
int32_t op_mnegg(int32_t val);
int32_t op_cvtdf(uint32_t *opnd);
int32_t op_cvtfg(uint32_t *opnd, int32_t *rh);
int32_t op_cvtgf(uint32_t *opnd);
int32_t op_addf(uint32_t *opnd, bool sub);
int32_t op_addd(uint32_t *opnd, int32_t *rh, bool sub);
int32_t op_addg(uint32_t *opnd, int32_t *rh, bool sub);
int32_t op_mulf(uint32_t *opnd);
int32_t op_muld(uint32_t *opnd, int32_t *rh);
int32_t op_mulg(uint32_t *opnd, int32_t *rh);
int32_t op_divf(uint32_t *opnd);
int32_t op_divd(uint32_t *opnd, int32_t *rh);
int32_t op_divg(uint32_t *opnd, int32_t *rh);
void op_polyf(uint32_t *opnd, int32_t acc);
void op_polyd(uint32_t *opnd, int32_t acc);
void op_polyg(uint32_t *opnd, int32_t acc);

#endif /* VAX_FPA_H_ */
