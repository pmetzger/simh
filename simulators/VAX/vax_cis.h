/* vax_cis.h: VAX commercial instruction set interface */
// SPDX-FileCopyrightText: 2004-2016 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_CIS_H_
#define VAX_CIS_H_ 1

#include <stdint.h>

#include "vax_psl.h"

/* EDITPC suboperators */

#define EO_END          0x00 /* end */
#define EO_END_FLOAT    0x01 /* end float */
#define EO_CLR_SIGNIF   0x02 /* clear signif */
#define EO_SET_SIGNIF   0x03 /* set signif */
#define EO_STORE_SIGN   0x04 /* store sign */
#define EO_LOAD_FILL    0x40 /* load fill */
#define EO_LOAD_SIGN    0x41 /* load sign */
#define EO_LOAD_PLUS    0x42 /* load sign if + */
#define EO_LOAD_MINUS   0x43 /* load sign if - */
#define EO_INSERT       0x44 /* insert */
#define EO_BLANK_ZERO   0x45 /* blank zero */
#define EO_REPL_SIGN    0x46 /* replace sign */
#define EO_ADJUST_LNT   0x47 /* adjust length */
#define EO_FILL         0x80 /* fill */
#define EO_MOVE         0x90 /* move */
#define EO_FLOAT        0xA0 /* float */
#define EO_RPT_MASK     0x0F /* rpt mask */
#define EO_RPT_FLAG     0x80 /* rpt flag */

/* EDITPC R2 packup parameters */

#define ED_V_CC         16 /* condition codes */
#define ED_M_CC         0xFF
#define ED_CC           (ED_M_CC << ED_V_CC)
#define ED_V_SIGN       8 /* sign */
#define ED_M_SIGN       0xFF
#define ED_SIGN         (ED_M_SIGN << ED_V_SIGN)
#define ED_V_FILL       0 /* fill */
#define ED_M_FILL       0xFF
#define ED_FILL         (ED_M_FILL << ED_V_FILL)
#define ED_GETCC(x)     (((x) >> ED_V_CC) & CC_MASK)
#define ED_GETSIGN(x)   (((x) >> ED_V_SIGN) & ED_M_SIGN)
#define ED_GETFILL(x)   (((x) >> ED_V_FILL) & ED_M_FILL)
#define ED_PUTCC(r, x)  (((r) & ~ED_CC) | (((x) << ED_V_CC) & ED_CC))
#define ED_PUTSIGN(r, x) \
    (((r) & ~ED_SIGN) | (((x) << ED_V_SIGN) & ED_SIGN))
#define ED_PUTFILL(r, x) \
    (((r) & ~ED_FILL) | (((x) << ED_V_FILL) & ED_FILL))

int32_t op_cis(uint32_t *opnd, int32_t cc, int32_t opc, int32_t acc);

#endif /* VAX_CIS_H_ */
