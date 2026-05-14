/* vax_psl.h: VAX processor status longword definitions */
// SPDX-FileCopyrightText: 1998-2019 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_PSL_H_
#define VAX_PSL_H_ 1

/* Processor Status Longword, Processor Status Word, and condition codes.

   This header owns the architectural bit layout for the VAX status word
   state.  It intentionally does not define instruction execution helper
   macros that compute or mutate condition codes. */

#define PSL_V_CM 31 /* compatibility mode */
#define PSL_CM (1u << PSL_V_CM)
#define PSL_V_TP 30 /* trace pending */
#define PSL_TP (1 << PSL_V_TP)
#define PSL_V_FPD 27 /* first part done */
#define PSL_FPD (1 << PSL_V_FPD)
#define PSL_V_IS 26 /* interrupt stack */
#define PSL_IS (1 << PSL_V_IS)
#define PSL_V_CUR 24   /* current mode */
#define PSL_V_PRV 22   /* previous mode */
#define PSL_M_MODE 0x3 /* mode mask */
#define PSL_CUR (PSL_M_MODE << PSL_V_CUR)
#define PSL_PRV (PSL_M_MODE << PSL_V_PRV)
#define PSL_V_IPL 16 /* int priority lvl */
#define PSL_M_IPL 0x1F
#define PSL_IPL (PSL_M_IPL << PSL_V_IPL)
#define PSL_IPL1 (0x01 << PSL_V_IPL)
#define PSL_IPL17 (0x17 << PSL_V_IPL)
#define PSL_IPL1F (0x1F << PSL_V_IPL)
#define PSL_MBZ (0x30200000 | PSW_MBZ) /* must be zero */
#define PSW_MBZ 0xFF00                 /* must be zero */
#define PSW_DV 0x80                    /* dec ovflo enable */
#define PSW_FU 0x40                    /* flt undflo enable */
#define PSW_IV 0x20                    /* int ovflo enable */
#define PSW_T 0x10                     /* trace enable */
#define CC_N 0x08                      /* negative */
#define CC_Z 0x04                      /* zero */
#define CC_V 0x02                      /* overflow */
#define CC_C 0x01                      /* carry */
#define CC_MASK (CC_N | CC_Z | CC_V | CC_C)
#define PSL_GETCUR(x) (((x) >> PSL_V_CUR) & PSL_M_MODE)
#define PSL_GETPRV(x) (((x) >> PSL_V_PRV) & PSL_M_MODE)
#define PSL_GETIPL(x) (((x) >> PSL_V_IPL) & PSL_M_IPL)

#endif /* VAX_PSL_H_ */
