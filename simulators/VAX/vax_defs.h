/* vax_defs.h: VAX architecture definitions file

   Copyright (c) 1998-2019, Robert M Supnik

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   The author gratefully acknowledges the help of Stephen Shirron, Antonio
   Carlini, and Kevin Peterson in providing specifications for the Qbus VAX's

   23-Apr-19    RMS     Added hook for unpredictable indexed immediate .aw
   05-Nov-11    RMS     Added PSL_IPL17 definition
   09-May-06    RMS     Added system PTE ACV error code
   03-May-06    RMS     Added EDITPC get/put cc's macros
   03-Nov-05    RMS     Added 780 stop codes
   22-Jul-05    RMS     Fixed warning from Solaris C (from Doug Gwyn)
   02-Sep-04    RMS     Added octa specifier definitions
   30-Aug-04    RMS     Added octa, h_floating instruction definitions
   24-Aug-04    RMS     Added compatibility mode definitions
   18-Apr-04    RMS     Added octa, fp, string definitions
   19-May-03    RMS     Revised for new conditional compilation scheme
   14-Jul-02    RMS     Added infinite loop message
   30-Apr-02    RMS     Added CLR_TRAPS macro
*/

#ifndef VAX_DEFS_H
#define VAX_DEFS_H     1

#ifndef VM_VAX
#define VM_VAX          0
#endif

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"
#include "vax_psl.h"

/* Stops and aborts */

#define STOP_HALT       1                               /* halt */
#define STOP_IBKPT      2                               /* breakpoint */
#define STOP_CHMFI      3                               /* chg mode IS */
#define STOP_ILLVEC     4                               /* illegal vector */
#define STOP_INIE       5                               /* exc in intexc */
#define STOP_PPTE       6                               /* proc pte in Px */
#define STOP_UIPL       7                               /* undefined IPL */
#define STOP_RQ         8                               /* fatal RQ err */
#define STOP_LOOP       9                               /* infinite loop */
#define STOP_SANITY     10                              /* sanity timer exp */
#define STOP_SWDN       11                              /* software done (780) */
#define STOP_BOOT       12                              /* reboot (780) */
#define STOP_UNKNOWN    13                              /* unknown reason */
#define STOP_UNKABO     14                              /* unknown abort */
#define STOP_DTOFF      15                              /* DECtape off reel */
#define ABORT_INTR      -1                              /* interrupt */
#define ABORT_MCHK      (-SCB_MCHK)                     /* machine check */
#define ABORT_RESIN     (-SCB_RESIN)                    /* rsvd instruction */
#define ABORT_RESAD     (-SCB_RESAD)                    /* rsvd addr mode */
#define ABORT_RESOP     (-SCB_RESOP)                    /* rsvd operand */
#define ABORT_CMODE     (-SCB_CMODE)                    /* comp mode fault */
#define ABORT_ARITH     (-SCB_ARITH)                    /* arithmetic trap */
#define ABORT_ACV       (-SCB_ACV)                      /* access violation */
#define ABORT_TNV       (-SCB_TNV)                      /* transl not vaid */
#define ABORT(x)        longjmp (save_env, (x))         /* abort */
extern jmp_buf save_env;
#define RSVD_INST_FAULT(opc) do {                                                                       \
        char op_num[20];                                                                                \
        char const *opcd = opcode[opc];                                                                 \
                                                                                                        \
        if (sim_deb && !opcd) {                                                                         \
            opcd = op_num;                                                                              \
            snprintf (op_num, sizeof (op_num), "Opcode 0x%X", opc);                                    \
            sim_debug (LOG_CPU_FAULT_RSVD, &cpu_dev, "%s fault_PC=%08x, PSL=%08x, SP=%08x, PC=%08x\n",  \
                                                     opcd, fault_PC, PSL, SP, PC);                      \
            }                                                                                           \
        ABORT (ABORT_RESIN); } while (0)
#define RSVD_ADDR_FAULT ABORT (ABORT_RESAD)
#define RSVD_OPND_FAULT(opc) do {                                                                       \
        sim_debug (LOG_CPU_FAULT_RSVD, &cpu_dev, #opc " fault_PC=%08x, PSL=%08x, SP=%08x, PC=%08x\n",   \
                                                 fault_PC, PSL, SP, PC);                                \
        ABORT (ABORT_RESOP); } while (0)
#define FLT_OVFL_FAULT  p1 = FLT_OVRFLO, ABORT (ABORT_ARITH)
#define FLT_DZRO_FAULT  p1 = FLT_DIVZRO, ABORT (ABORT_ARITH)
#define FLT_UNFL_FAULT  p1 = FLT_UNDFLO, ABORT (ABORT_ARITH)
#define CMODE_FAULT(cd) do {                                                                            \
        sim_debug (LOG_CPU_FAULT_CMODE, &cpu_dev, #cd " fault_PC=%08x, PSL=%08x, SP=%08x, PC=%08x\n",   \
                                                 fault_PC, PSL, SP, PC);                                \
        p1 = (cd);                                                                                      \
        ABORT (ABORT_CMODE); } while (0)
#define MACH_CHECK(cd)  do {                                                                            \
        sim_debug (LOG_CPU_FAULT_MCHK, &cpu_dev, #cd " fault_PC=%08x, PSL=%08x, SP=%08x, PC=%08x\n",    \
                                                 fault_PC, PSL, SP, PC);                                \
        p1 = (cd);                                                                                      \
        ABORT (ABORT_MCHK); } while (0)

/* Logging */

#define LOG_CPU_I           0x001                       /* intexc */
#define LOG_CPU_R           0x002                       /* REI */
#define LOG_CPU_A           0x004                       /* Abort */
#define LOG_CPU_P           0x008                       /* process context */
#define LOG_CPU_FAULT_RSVD  0x010                       /* reserved faults */
#define LOG_CPU_FAULT_FLT   0x020                       /* floating faults*/
#define LOG_CPU_FAULT_CMODE 0x040                       /* cmode faults */
#define LOG_CPU_FAULT_MCHK  0x080                       /* machine check faults */
#define LOG_CPU_FAULT_EMUL  0x100                       /* emulated instruction fault */

/* Recovery queue */

#define RQ_RN           0xF                             /* register */
#define RQ_V_LNT        4                               /* length */
#define RQ_M_LNT        0x7                             /* 0,1,2,3,4 */
#define RQ_DIR          0x800                           /* 0 = -, 1 = + */
#define RQ_REC(d,r)     (((d) << RQ_V_LNT) | (r))
#define RQ_GETRN(x)     ((x) & RQ_RN)
#define RQ_GETLNT(x)    (((x) >> RQ_V_LNT) & RQ_M_LNT)

/* Address space */

#define VAMASK          0xFFFFFFFF                      /* virt addr mask */
#define PAWIDTH         30                              /* phys addr width */
#define PASIZE          (1 << PAWIDTH)                  /* phys addr size */
#define PAMASK          (PASIZE - 1)                    /* phys addr mask */
#define IOPAGE          (1 << (PAWIDTH - 1))            /* start of I/O page */

/* Architectural constants */

#define BMASK           0x000000FF                      /* byte */
#define BSIGN           0x00000080
#define WMASK           0x0000FFFF                      /* word */
#define WSIGN           0x00008000
#define LMASK           0xFFFFFFFF                      /* longword */
#define LSIGN           0x80000000
#define FPSIGN          0x00008000                      /* floating point */
#define L_BYTE          1                               /* bytes per */
#define L_WORD          2                               /* data type */
#define L_LONG          4
#define L_QUAD          8
#define L_OCTA          16
#define NUM_INST        512                             /* one byte+two byte */
#define MAX_SPEC        6                               /* max spec/instr */

/* Floating point formats */

#define FD_V_EXP        7                               /* f/d exponent */
#define FD_M_EXP        0xFF
#define FD_BIAS         0x80                            /* f/d bias */
#define FD_EXP          (FD_M_EXP << FD_V_EXP)
#define FD_HB           (1 << FD_V_EXP)                 /* f/d hidden bit */
#define FD_GUARD        (15 - FD_V_EXP)                 /* # guard bits */
#define FD_GETEXP(x)    (((x) >> FD_V_EXP) & FD_M_EXP)

#define G_V_EXP         4                               /* g exponent */
#define G_M_EXP         0x7FF
#define G_BIAS          0x400                           /* g bias */
#define G_EXP           (G_M_EXP << G_V_EXP)
#define G_HB            (1 << G_V_EXP)                  /* g hidden bit */
#define G_GUARD         (15 - G_V_EXP)                  /* # guard bits */
#define G_GETEXP(x)     (((x) >> G_V_EXP) & G_M_EXP)

#define H_V_EXP         0                               /* h exponent */
#define H_M_EXP         0x7FFF
#define H_BIAS          0x4000                          /* h bias */
#define H_EXP           (H_M_EXP << H_V_EXP)
#define H_HB            (1 << H_V_EXP)                  /* h hidden bit */
#define H_GUARD         (15 - H_V_EXP)                  /* # guard bits */
#define H_GETEXP(x)     (((x) >> H_V_EXP) & H_M_EXP)

/* Memory management modes */

#define KERN            0
#define EXEC            1
#define SUPV            2
#define USER            3

/* Register and stack aliases */

#define nAP             12
#define nFP             13
#define nSP             14
#define nPC             15
#define AP              R[nAP]
#define FP              R[nFP]
#define SP              R[nSP]
#define PC              R[nPC]
#define RGMASK          0xF
#define KSP             STK[KERN]
#define ESP             STK[EXEC]
#define SSP             STK[SUPV]
#define USP             STK[USER]
#define IS              STK[4]

/* Software interrupt summary register */

#define SISR_MASK       0xFFFE
#define SISR_2          (1 << 2)

/* AST register */

#define AST_MASK        7
#define AST_MAX         4

/* Virtual address */

#define VA_N_OFF        9                               /* offset size */
#define VA_PAGSIZE      (1u << VA_N_OFF)                /* page size */
#define VA_M_OFF        ((1u << VA_N_OFF) - 1)          /* offset mask */
#define VA_V_VPN        VA_N_OFF                        /* vpn start */
#define VA_N_VPN        (31 - VA_N_OFF)                 /* vpn size */
#define VA_M_VPN        ((1u << VA_N_VPN) - 1)          /* vpn mask */
#define VA_S0           (1u << 31)                      /* S0 space */
#define VA_P1           (1u << 30)                      /* P1 space */
#define VA_N_TBI        12                              /* TB index size */
#define VA_TBSIZE       (1u << VA_N_TBI)                /* TB size */
#define VA_M_TBI        ((1u << VA_N_TBI) - 1)          /* TB index mask */
#define VA_GETOFF(x)    ((x) & VA_M_OFF)
#define VA_GETVPN(x)    (((x) >> VA_V_VPN) & VA_M_VPN)
#define VA_GETTBI(x)    ((x) & VA_M_TBI)

/* PTE */

#define PTE_V_V         31                              /* valid */
#define PTE_V           (1u << PTE_V_V)
#define PTE_V_ACC       27                              /* access */
#define PTE_M_ACC       0xF
#define PTE_ACC         (PTE_M_ACC << PTE_V_ACC)
#define PTE_V_M         26                              /* modified */
#define PTE_M           (1u << PTE_V_M)
#define PTE_GETACC(x)   (((x) >> PTE_V_ACC) & PTE_M_ACC)

/* TLB entry */

#define TLB_V_RACC      0                               /* rd acc field */
#define TLB_V_WACC      4                               /* wr acc field */
#define TLB_M_ACC       0xF
#define TLB_RACC        (TLB_M_ACC << TLB_V_RACC)
#define TLB_WACC        (TLB_M_ACC << TLB_V_WACC)
#define TLB_V_M         8                               /* m bit */
#define TLB_M           (1u << TLB_V_M)
#define TLB_N_PFN       (PAWIDTH - VA_N_OFF)            /* ppfn size */
#define TLB_M_PFN       ((1u << TLB_N_PFN) - 1)         /* ppfn mask */
#define TLB_PFN         (TLB_M_PFN << VA_V_VPN)

/* Traps and interrupt requests */

#define TIR_V_IRQL      0                               /* int request lvl */
#define TIR_V_TRAP      5                               /* trap requests */
#define TIR_M_TRAP      07
#define TIR_TRAP        (TIR_M_TRAP << TIR_V_TRAP)
#define TRAP_INTOV      (1 << TIR_V_TRAP)               /* integer overflow */
#define TRAP_DIVZRO     (2 << TIR_V_TRAP)               /* divide by zero */
#define TRAP_FLTOVF     (3 << TIR_V_TRAP)               /* flt overflow */
#define TRAP_FLTDIV     (4 << TIR_V_TRAP)               /* flt/dec div by zero */
#define TRAP_FLTUND     (5 << TIR_V_TRAP)               /* flt underflow */
#define TRAP_DECOVF     (6 << TIR_V_TRAP)               /* decimal overflow */
#define TRAP_SUBSCR     (7 << TIR_V_TRAP)               /* subscript range */
#define SET_TRAP(x)     trpirq = (trpirq & PSL_M_IPL) | (x)
#define CLR_TRAPS       trpirq = trpirq & ~TIR_TRAP
#define SET_IRQL        trpirq = (trpirq & TIR_TRAP) | eval_int ()
#define GET_TRAP(x)     (((x) >> TIR_V_TRAP) & TIR_M_TRAP)
#define GET_IRQL(x)     (((x) >> TIR_V_IRQL) & PSL_M_IPL)

/* Floating point fault parameters */

#define FLT_OVRFLO      0x8                             /* flt overflow */
#define FLT_DIVZRO      0x9                             /* flt div by zero */
#define FLT_UNDFLO      0xA                             /* flt underflow */

/* Compatability mode fault parameters */

#define CMODE_RSVI      0x0                             /* reserved instr */
#define CMODE_BPT       0x1                             /* BPT */
#define CMODE_IOT       0x2                             /* IOT */
#define CMODE_EMT       0x3                             /* EMT */
#define CMODE_TRAP      0x4                             /* TRAP */
#define CMODE_ILLI      0x5                             /* illegal instr */
#define CMODE_ODD       0x6                             /* odd address */

/* SCB offsets */

#define SCB_MCHK        0x04                            /* machine chk */
#define SCB_KSNV        0x08                            /* ker stk invalid */
#define SCB_PWRFL       0x0C                            /* power fail */
#define SCB_RESIN       0x10                            /* rsvd/priv instr */
#define SCB_XFC         0x14                            /* XFC instr */
#define SCB_RESOP       0x18                            /* rsvd operand */
#define SCB_RESAD       0x1C                            /* rsvd addr mode */
#define SCB_ACV         0x20                            /* ACV */
#define SCB_TNV         0x24                            /* TNV */
#define SCB_TP          0x28                            /* trace pending */
#define SCB_BPT         0x2C                            /* BPT instr */
#define SCB_CMODE       0x30                            /* comp mode fault */
#define SCB_ARITH       0x34                            /* arith fault */
#define SCB_CHMK        0x40                            /* CHMK */
#define SCB_CHME        0x44                            /* CHME */
#define SCB_CHMS        0x48                            /* CHMS */
#define SCB_CHMU        0x4C                            /* CHMU */
#define SCB_CRDERR      0x54                            /* CRD err intr */
#define SCB_MEMERR      0x60                            /* mem err intr */
#define SCB_IPLSOFT     0x80                            /* software intr */
#define SCB_INTTIM      0xC0                            /* timer intr */
#define SCB_EMULATE     0xC8                            /* emulation */
#define SCB_EMULFPD     0xCC                            /* emulation, FPD */
#define SCB_CSI         0xF0                            /* constor input */
#define SCB_CSO         0xF4                            /* constor output */
#define SCB_TTI         0xF8                            /* console input */
#define SCB_TTO         0xFC                            /* console output */
#define SCB_INTR        0x100                           /* hardware intr */

#define IPL_HLTPIN      0x1F                            /* halt pin IPL */
#define IPL_MEMERR      0x1D                            /* mem err IPL */
#define IPL_CRDERR      0x1A                            /* CRD err IPL */

/* Interrupt and exception types */

#define IE_SVE          -1                              /* severe exception */
#define IE_EXC          0                               /* normal exception */
#define IE_INT          1                               /* interrupt */

/*
 * FULL_VAX     If defined, all instructions implemented (780 like)
 * CMPM_VAX     If defined, compatibility mode is implemented.
 *              (Defined for 780, 750, 730 and 8600 only)
 * NOEXS_VAX    If defined, no extra string instructions.
 *              (Defined for MicroVAX I and II only)
 * NOEXF_VAX    If defined, no extra floating point instructions.
 *              (Available for future Rigel, Mariah and NVAX implementations)
 */

/* Decode ROM: opcode entry */

#define DR_F            0x80                            /* FPD ok flag */
#define DR_NSPMASK      0x07                            /* #specifiers */
#define DR_V_USPMASK    4
#define DR_M_USPMASK    0x7                             /* #spec, sym_ */
#define DR_GETNSP(x)    ((x) & DR_NSPMASK)              /* #specifiers */
#define DR_GETUSP(x)    (((x) >> DR_V_USPMASK) & DR_M_USPMASK) /* #specifiers for unimplemented instructions */

/* Extra bits in the opcode flag word of the Decode ROM array only for history results */

#define DR_V_RESMASK    8
#define DR_M_RESMASK    0x000F
#define RB_0    (0 << DR_V_RESMASK)     /* No Results */
#define RB_B    (1 << DR_V_RESMASK)     /* Byte Result */
#define RB_W    (2 << DR_V_RESMASK)     /* Word Result */
#define RB_L    (3 << DR_V_RESMASK)     /* Long Result */
#define RB_Q    (4 << DR_V_RESMASK)     /* Quad Result */
#define RB_O    (5 << DR_V_RESMASK)     /* Octa Result */
#define RB_OB   (6 << DR_V_RESMASK)     /* Octa Byte Result */
#define RB_OW   (7 << DR_V_RESMASK)     /* Octa Word Result */
#define RB_OL   (8 << DR_V_RESMASK)     /* Octa Long Result */
#define RB_OQ   (9 << DR_V_RESMASK)     /* Octa Quad Result */
#define RB_R0  (10 << DR_V_RESMASK)     /* Reg  R0     */
#define RB_R1  (11 << DR_V_RESMASK)     /* Regs R0-R1  */
#define RB_R3  (12 << DR_V_RESMASK)     /* Regs R0-R3  */
#define RB_R5  (13 << DR_V_RESMASK)     /* Regs R0-R5  */
#define RB_SP  (14 << DR_V_RESMASK)     /* @SP         */
#define DR_GETRES(x)    (((x) >> DR_V_RESMASK) & DR_M_RESMASK)

/* Extra bits in the opcode flag word of the Decode ROM array
   to identify instruction group */

#define DR_V_IGMASK     12
#define DR_M_IGMASK    0x0007
#define IG_RSVD     (0 << DR_V_IGMASK)  /* Reserved Opcode */
#define IG_BASE     (1 << DR_V_IGMASK)  /* Base Instruction Group       */
#define IG_BSGFL    (2 << DR_V_IGMASK)  /*   Base subgroup G-Float      */
#define IG_BSDFL    (3 << DR_V_IGMASK)  /*   Base subgroup D-Float      */
#define IG_PACKD    (4 << DR_V_IGMASK)  /* packed-decimal-string group  */
#define IG_EXTAC    (5 << DR_V_IGMASK)  /* extended-accuracy group      */
#define IG_EMONL    (6 << DR_V_IGMASK)  /* emulated-only group          */
#define IG_VECTR    (7 << DR_V_IGMASK)  /* vector-processing group      */
#define IG_MAX_GRP  7                   /* Maximum Instruction groups */
#define DR_GETIGRP(x)    (((x) >> DR_V_IGMASK) & DR_M_IGMASK)

#define VAX_FULL_BASE ((1 << DR_GETIGRP(IG_BASE))  | \
                       (1 << DR_GETIGRP(IG_BSGFL)) | \
                       (1 << DR_GETIGRP(IG_BSDFL)))
#define VAX_BASE   (1 << DR_GETIGRP(IG_BASE))
#define VAX_GFLOAT (1 << DR_GETIGRP(IG_BSGFL))
#define VAX_DFLOAT (1 << DR_GETIGRP(IG_BSDFL))
#define VAX_PACKED (1 << DR_GETIGRP(IG_PACKD))
#define VAX_EXTAC  (1 << DR_GETIGRP(IG_EXTAC))
#define VAX_EMONL  (1 << DR_GETIGRP(IG_EMONL))
#define VAX_VECTR  (1 << DR_GETIGRP(IG_VECTR))
#define FULL_INSTRUCTION_SET (VAX_FULL_BASE               | \
                              (1 << DR_GETIGRP(IG_PACKD)) | \
                              (1 << DR_GETIGRP(IG_EXTAC)) | \
                              (1 << DR_GETIGRP(IG_EMONL)))

/* Decode ROM: specifier entry */

#define DR_ACMASK       0x300                           /* type */
#define DR_SPFLAG       0x008                           /* special decode */
#define DR_LNMASK       0x007                           /* length mask */
#define DR_LNT(x)       (1 << (x & DR_LNMASK))          /* disp to lnt */

/* Decode ROM: length */

#define DR_BYTE         0x000                           /* byte */
#define DR_WORD         0x001                           /* word */
#define DR_LONG         0x002                           /* long */
#define DR_QUAD         0x003                           /* quad */
#define DR_OCTA         0x004                           /* octa */

/* Decode ROM: operand type  */

#define SH0             0x000                           /* short literal */
#define SH1             0x010
#define SH2             0x020
#define SH3             0x030
#define IDX             0x040                           /* indexed */
#define GRN             0x050                           /* register */
#define RGD             0x060                           /* register def */
#define ADC             0x070                           /* autodecrement */
#define AIN             0x080                           /* autoincrement */
#define AID             0x090                           /* autoinc def */
#define BDP             0x0A0                           /* byte disp */
#define BDD             0x0B0                           /* byte disp def */
#define WDP             0x0C0                           /* word disp */
#define WDD             0x0D0                           /* word disp def */
#define LDP             0x0E0                           /* long disp */
#define LDD             0x0F0                           /* long disp def */

/* Decode ROM: access type */

#define DR_R            0x000                           /* read */
#define DR_M            0x100                           /* modify */
#define DR_A            0x200                           /* address */
#define DR_W            0x300                           /* write */

/* Decode ROM: access type and length */

#define RB              (DR_R|DR_BYTE)
#define RW              (DR_R|DR_WORD)
#define RL              (DR_R|DR_LONG)
#define RQ              (DR_R|DR_QUAD)
#define RO              (DR_R|DR_OCTA)
#define MB              (DR_M|DR_BYTE)
#define MW              (DR_M|DR_WORD)
#define ML              (DR_M|DR_LONG)
#define MQ              (DR_M|DR_QUAD)
#define MO              (DR_M|DR_OCTA)
#define AB              (DR_A|DR_BYTE)
#define AW              (DR_A|DR_WORD)
#define AL              (DR_A|DR_LONG)
#define AQ              (DR_A|DR_QUAD)
#define AO              (DR_A|DR_OCTA)
#define WB              (DR_W|DR_BYTE)
#define WW              (DR_W|DR_WORD)
#define WL              (DR_W|DR_LONG)
#define WQ              (DR_W|DR_QUAD)
#define WO              (DR_W|DR_OCTA)

/* Special dispatches.

   vb   =       variable bit field, treated as wb except for register
   rf   =       f_floating, treated as rl except for short literal
   rd   =       d_floating, treated as rq except for short literal
   rg   =       g_floating, treated as rq except for short literal
   rh   =       h_floating, treated as ro except for short literal
   bb   =       branch byte displacement
   bw   =       branch word displacement

   Length field must be correct
*/

#define VB              (DR_SPFLAG|WB)                  /* .vb */
#define RF              (DR_SPFLAG|RL)                  /* .rf */
#define RD              (DR_SPFLAG|RQ)                  /* .rd */
#define RG              (DR_SPFLAG|MQ)                  /* .rg */
#define RH              (DR_SPFLAG|RO)                  /* .rh */
#define BB              (DR_SPFLAG|WB|6)                /* byte branch */
#define BW              (DR_SPFLAG|WB|7)                /* word branch */

/* Probe results and memory management fault codes */

#define PR_ACV          0                               /* ACV */
#define PR_LNV          1                               /* length viol */
#define PR_PACV         2                               /* pte ACV (780) */
#define PR_PLNV         3                               /* pte len viol */
#define PR_TNV          4                               /* TNV */
/* #define PR_TB        5                             *//* impossible */
#define PR_PTNV         6                               /* pte TNV */
#define PR_OK           7                               /* ok */
#define MM_PARAM(w,p)   (((w)? 4: 0) | ((p) & 3))       /* fault param */

/* Memory management errors */

#define MM_WRITE        4                               /* write */
#define MM_EMASK        3                               /* against probe */

/* Privileged registers */

#define MT_KSP          0
#define MT_ESP          1
#define MT_SSP          2
#define MT_USP          3
#define MT_IS           4
#define MT_P0BR         8
#define MT_P0LR         9
#define MT_P1BR         10
#define MT_P1LR         11
#define MT_SBR          12
#define MT_SLR          13
#define MT_PCBB         16
#define MT_SCBB         17
#define MT_IPL          18
#define MT_ASTLVL       19
#define MT_SIRR         20
#define MT_SISR         21
#define MT_ICCS         24
#define MT_NICR         25
#define MT_ICR          26
#define MT_TODR         27
#define MT_CSRS         28
#define MT_CSRD         29
#define MT_CSTS         30
#define MT_CSTD         31
#define MT_RXCS         32
#define MT_RXDB         33
#define MT_TXCS         34
#define MT_TXDB         35
#define MT_MAPEN        56
#define MT_TBIA         57
#define MT_TBIS         58
#define MT_PME          61
#define MT_SID          62
#define MT_TBCHK        63

#define BR_MASK         0xFFFFFFFC
#define LR_MASK         0x003FFFFF

/* Opcodes */

enum opcodes {
 HALT, NOP, REI, BPT, RET, RSB, LDPCTX, SVPCTX,
 CVTPS, CVTSP, INDEX, CRC, PROBER, PROBEW, INSQUE, REMQUE,
 BSBB, BRB, BNEQ, BEQL, BGTR, BLEQ, JSB, JMP,
 BGEQ, BLSS, BGTRU, BLEQU, BVC, BVS, BGEQU, BLSSU,
 ADDP4, ADDP6, SUBP4, SUBP6, CVTPT, MULP, CVTTP, DIVP,
 MOVC3, CMPC3, SCANC, SPANC, MOVC5, CMPC5, MOVTC, MOVTUC,
 BSBW, BRW, CVTWL, CVTWB, MOVP, CMPP3, CVTPL, CMPP4,
 EDITPC, MATCHC, LOCC, SKPC, MOVZWL, ACBW, MOVAW, PUSHAW,
 ADDF2, ADDF3, SUBF2, SUBF3, MULF2, MULF3, DIVF2, DIVF3,
 CVTFB, CVTFW, CVTFL, CVTRFL, CVTBF, CVTWF, CVTLF, ACBF,
 MOVF, CMPF, MNEGF, TSTF, EMODF, POLYF, CVTFD,
 ADAWI = 0x58, INSQHI = 0x5C, INSQTI, REMQHI, REMQTI,
 ADDD2, ADDD3, SUBD2, SUBD3, MULD2, MULD3, DIVD2, DIVD3,
 CVTDB, CVTDW, CVTDL, CVTRDL, CVTBD, CVTWD, CVTLD, ACBD,
 MOVD, CMPD, MNEGD, TSTD, EMODD, POLYD, CVTDF,
 ASHL = 0x78, ASHQ, EMUL, EDIV, CLRQ, MOVQ, MOVAQ, PUSHAQ,
 ADDB2, ADDB3, SUBB2, SUBB3, MULB2, MULB3, DIVB2, DIVB3,
 BISB2, BISB3, BICB2, BICB3, XORB2, XORB3, MNEGB, CASEB,
 MOVB, CMPB, MCOMB, BITB, CLRB, TSTB, INCB, DECB,
 CVTBL, CVTBW, MOVZBL, MOVZBW, ROTL, ACBB, MOVAB, PUSHAB,
 ADDW2, ADDW3, SUBW2, SUBW3, MULW2, MULW3, DIVW2, DIVW3,
 BISW2, BISW3, BICW2, BICW3, XORW2, XORW3, MNEGW, CASEW,
 MOVW, CMPW, MCOMW, BITW, CLRW, TSTW, INCW, DECW,
 BISPSW, BICPSW, POPR, PUSHR, CHMK, CHME, CHMS, CHMU,
 ADDL2, ADDL3, SUBL2, SUBL3, MULL2, MULL3, DIVL2, DIVL3,
 BISL2, BISL3, BICL2, BICL3, XORL2, XORL3, MNEGL, CASEL,
 MOVL, CMPL, MCOML, BITL, CLRL, TSTL, INCL, DECL,
 ADWC, SBWC, MTPR, MFPR, MOVPSL, PUSHL, MOVAL, PUSHAL,
 BBS, BBC, BBSS, BBCS, BBSC, BBCC, BBSSI, BBCCI,
 BLBS, BLBC, FFS, FFC, CMPV, CMPZV, EXTV, EXTZV,
 INSV, ACBL, AOBLSS, AOBLEQ, SOBGEQ, SOBGTR, CVTLB, CVTLW,
 ASHP, CVTLP, CALLG, CALLS, XFC, CVTDH = 0x132, CVTGF = 0x133,
 ADDG2 = 0x140, ADDG3, SUBG2, SUBG3, MULG2, MULG3, DIVG2, DIVG3,
 CVTGB, CVTGW, CVTGL, CVTRGL, CVTBG, CVTWG, CVTLG, ACBG,
 MOVG, CMPG, MNEGG, TSTG, EMODG, POLYG, CVTGH,
 ADDH2 = 0x160, ADDH3, SUBH2, SUBH3, MULH2, MULH3, DIVH2, DIVH3,
 CVTHB, CVTHW, CVTHL, CVTRHL, CVTBH, CVTWH, CVTLH, ACBH,
 MOVH, CMPH, MNEGH, TSTH, EMODH, POLYH, CVTHG,
 CLRO = 0x17C, MOVO, MOVAO, PUSHAO,
 CVTFH = 0x198, CVTFG = 0x199,
 CVTHF = 0x1F6, CVTHD = 0x1F7 };

/* Repeated operations */

#define SXTB(x)         (((x) & BSIGN)? ((x) | ~BMASK): ((x) & BMASK))
#define SXTW(x)         (((x) & WSIGN)? ((x) | ~WMASK): ((x) & WMASK))
#define SXTBW(x)        (((x) & BSIGN)? ((x) | (WMASK - BMASK)): ((x) & BMASK))
#define SXTL(x)         (((x) & LSIGN)? ((x) | ~LMASK): ((x) & LMASK))
#define INTOV           if (PSL & PSW_IV) SET_TRAP (TRAP_INTOV)
#define V_INTOV         cc = cc | CC_V; INTOV
#define NEG(x)          ((~(x) + 1) & LMASK)

/* Istream access */

#define PCQ_SIZE        64                              /* must be 2**n */
#define PCQ_MASK        (PCQ_SIZE - 1)
#define PCQ_ENTRY       pcq[pcq_p = (pcq_p - 1) & PCQ_MASK] = fault_PC
#define GET_ISTR(d,l)   d = get_istr (l, acc)
#define CHECK_FOR_IDLE_LOOP if (PC == fault_PC) {                           /* to self? */ \
                                if (PSL_GETIPL (PSL) == 0x1F)               /* int locked out? */ \
                                    ABORT (STOP_LOOP);                      /* infinite loop */ \
                                cpu_idle ();                                /* idle loop */ \
                                }
/* Instructions which have side effects (ACB, AOBLSS, BBSC, BBCS, etc.) can't be an idle loop so avoid the idle check */
#define BRANCHB_ALWAYS(d)      do {PCQ_ENTRY; PC = PC + SXTB (d); FLUSH_ISTR; } while (0)
#define BRANCHW_ALWAYS(d)      do {PCQ_ENTRY; PC = PC + SXTW (d); FLUSH_ISTR; } while (0)
#define JUMP_ALWAYS(d)         do {PCQ_ENTRY; PC = (d);           FLUSH_ISTR; } while (0)
/* Any basic branch instructions could be an idle loop */
#define BRANCHB(d)      do {PCQ_ENTRY; PC = PC + SXTB (d); FLUSH_ISTR; CHECK_FOR_IDLE_LOOP; } while (0)
#define BRANCHW(d)      do {PCQ_ENTRY; PC = PC + SXTW (d); FLUSH_ISTR; CHECK_FOR_IDLE_LOOP; } while (0)
#define JUMP(d)         do {PCQ_ENTRY; PC = (d); FLUSH_ISTR; CHECK_FOR_IDLE_LOOP; } while (0)
#define CMODE_JUMP(d)   do {PCQ_ENTRY; PC = (d); CHECK_FOR_IDLE_LOOP; } while (0)
#define SETPC(d)        PC = (d), FLUSH_ISTR
#define FLUSH_ISTR      ibcnt = 0, ppc = -1

/* Character string instructions */

#define STR_V_DPC       24                              /* delta PC */
#define STR_M_DPC       0xFF
#define STR_V_CHR       16                              /* char argument */
#define STR_M_CHR       0xFF
#define STR_LNMASK      0xFFFF                          /* string length */
#define STR_GETDPC(x)   (((x) >> STR_V_DPC) & STR_M_DPC)
#define STR_GETCHR(x)   (((x) >> STR_V_CHR) & STR_M_CHR)
#define STR_PACK(m,x)   ((((PC - fault_PC) & STR_M_DPC) << STR_V_DPC) | \
                        (((m) & STR_M_CHR) << STR_V_CHR) | ((x) & STR_LNMASK))

/* Read and write */

#define RA              (acc)
#define WA              ((acc) << TLB_V_WACC)
#define ACC_MASK(x)     (1 << (x))
#define TLB_ACCR(x)     (ACC_MASK (x) << TLB_V_RACC)
#define TLB_ACCW(x)     (ACC_MASK (x) << TLB_V_WACC)
#define REF_V           0
#define REF_P           1

/* Condition code macros */

#define CC_ZZ1P cc = CC_Z | (cc & CC_C)

#define CC_IIZZ_B(r) \
            if ((r) & BSIGN) cc = CC_N; \
            else if ((r) == 0) cc = CC_Z; \
            else cc = 0
#define CC_IIZZ_W(r) \
            if ((r) & WSIGN) cc = CC_N; \
            else if ((r) == 0) cc = CC_Z; \
            else cc = 0
#define CC_IIZZ_L(r) \
            if ((r) & LSIGN) cc = CC_N; \
            else if ((r) == 0) cc = CC_Z; \
            else cc = 0
#define CC_IIZZ_Q(rl,rh) \
            if ((rh) & LSIGN) cc = CC_N; \
            else if (((rl) | (rh)) == 0) cc = CC_Z; \
            else cc = 0
#define CC_IIZZ_FP      CC_IIZZ_W

#define CC_IIZP_B(r) \
            if ((r) & BSIGN) cc = CC_N | (cc & CC_C); \
            else if ((r) == 0) cc = CC_Z | (cc & CC_C); \
            else cc = cc & CC_C
#define CC_IIZP_W(r) \
            if ((r) & WSIGN) cc = CC_N | (cc & CC_C); \
            else if ((r) == 0) cc = CC_Z | (cc & CC_C); \
            else cc = cc & CC_C
#define CC_IIZP_L(r) \
            if ((r) & LSIGN) cc = CC_N | (cc & CC_C); \
            else if ((r) == 0) cc = CC_Z | (cc & CC_C); \
            else cc = cc & CC_C
#define CC_IIZP_Q(rl,rh) \
            if ((rh) & LSIGN) cc = CC_N | (cc & CC_C); \
            else if (((rl) | (rh)) == 0) cc = CC_Z | (cc & CC_C); \
            else cc = cc & CC_C
#define CC_IIZP_O(rl,rm2,rm1,rh) \
            if ((rh) & LSIGN) cc = CC_N | (cc & CC_C); \
            else if (((rl) | (rm2) | (rm1) | (rh)) == 0) cc = CC_Z | (cc & CC_C); \
            else cc = cc & CC_C
#define CC_IIZP_FP      CC_IIZP_W

#define V_ADD_B(r,s1,s2) \
            if (((~(s1) ^ (s2)) & ((s1) ^ (r))) & BSIGN) { V_INTOV; }
#define V_ADD_W(r,s1,s2) \
            if (((~(s1) ^ (s2)) & ((s1) ^ (r))) & WSIGN) { V_INTOV; }
#define V_ADD_L(r,s1,s2) \
            if (((~(s1) ^ (s2)) & ((s1) ^ (r))) & LSIGN) { V_INTOV; }
#define C_ADD(r,s1,s2) \
            if (((uint32_t) r) < ((uint32_t) s2)) cc = cc | CC_C

#define CC_ADD_B(r,s1,s2) \
            CC_IIZZ_B (r); \
            V_ADD_B (r, s1, s2); \
            C_ADD (r, s1, s2)
#define CC_ADD_W(r,s1,s2) \
            CC_IIZZ_W (r); \
            V_ADD_W (r, s1, s2); \
            C_ADD (r, s1, s2)
#define CC_ADD_L(r,s1,s2) \
            CC_IIZZ_L (r); \
            V_ADD_L (r, s1, s2); \
            C_ADD (r, s1, s2)

#define V_SUB_B(r,s1,s2) \
            if ((((s1) ^ (s2)) & (~(s1) ^ (r))) & BSIGN) { V_INTOV; }
#define V_SUB_W(r,s1,s2) \
            if ((((s1) ^ (s2)) & (~(s1) ^ (r))) & WSIGN) { V_INTOV; }
#define V_SUB_L(r,s1,s2) \
            if ((((s1) ^ (s2)) & (~(s1) ^ (r))) & LSIGN) { V_INTOV; }
#define C_SUB(r,s1,s2) \
            if (((uint32_t) s2) < ((uint32_t) s1)) cc = cc | CC_C

#define CC_SUB_B(r,s1,s2) \
            CC_IIZZ_B (r); \
            V_SUB_B (r, s1, s2); \
            C_SUB (r, s1, s2)
#define CC_SUB_W(r,s1,s2) \
            CC_IIZZ_W (r); \
            V_SUB_W (r, s1, s2); \
            C_SUB (r, s1, s2)
#define CC_SUB_L(r,s1,s2) \
            CC_IIZZ_L (r); \
            V_SUB_L (r, s1, s2); \
            C_SUB (r, s1, s2)

#define CC_CMP_B(s1,s2) \
            cc = vax_cmp_b_cc ((uint32_t) (s1), (uint32_t) (s2))
#define CC_CMP_W(s1,s2) \
            cc = vax_cmp_w_cc ((uint32_t) (s1), (uint32_t) (s2))

/* Interpret VAX integer bit patterns as signed host values.

   The VAX simulator stores architectural integer values in unsigned
   containers so that arithmetic wraparound and bit manipulation are
   defined by C.  Use these helpers at the points where the architecture
   requires a signed byte, word, or longword interpretation. */

/* Return the signed byte value represented by the low 8 bits of val. */
static inline int32_t vax_sbyte (uint32_t val)
{
uint32_t mag;

val = val & BMASK;
if ((val & BSIGN) == 0)
    return (int32_t) val;
mag = ((~val) + 1) & BMASK;
return -(int32_t) mag;
}

/* Return the signed word value represented by the low 16 bits of val. */
static inline int32_t vax_sword (uint32_t val)
{
uint32_t mag;

val = val & WMASK;
if ((val & WSIGN) == 0)
    return (int32_t) val;
mag = ((~val) + 1) & WMASK;
return -(int32_t) mag;
}

/* Return the signed longword value represented by all 32 bits of val. */
static inline int32_t vax_slong (uint32_t val)
{
uint32_t mag;

if ((val & LSIGN) == 0)
    return (int32_t) val;
if (val == LSIGN)
    return -2147483647 - 1;
mag = ((~val) + 1) & LMASK;
return -(int32_t) mag;
}

/* Compare two unsigned longword bit patterns as signed VAX longwords. */
static inline bool vax_signed_lt_l (uint32_t s1, uint32_t s2)
{
if ((s1 ^ s2) & LSIGN)
    return (s1 & LSIGN) != 0;
return s1 < s2;
}

static inline bool vax_signed_le_l (uint32_t s1, uint32_t s2)
{
return (s1 == s2) || vax_signed_lt_l (s1, s2);
}

/* Return CMPB condition codes for two VAX byte bit patterns.

   VAX compare uses signed ordering for N, equality for Z, unsigned
   ordering for C, and leaves V clear. */
static inline int32_t vax_cmp_b_cc (uint32_t s1, uint32_t s2)
{
int32_t cmp_cc = 0;

s1 = s1 & BMASK;
s2 = s2 & BMASK;
if (vax_sbyte (s1) < vax_sbyte (s2))
    cmp_cc = CC_N;
else if (s1 == s2)
    cmp_cc = CC_Z;
if (s1 < s2)
    cmp_cc = cmp_cc | CC_C;
return cmp_cc;
}

/* Return CMPW condition codes for two VAX word bit patterns.

   VAX compare uses signed ordering for N, equality for Z, unsigned
   ordering for C, and leaves V clear. */
static inline int32_t vax_cmp_w_cc (uint32_t s1, uint32_t s2)
{
int32_t cmp_cc = 0;

s1 = s1 & WMASK;
s2 = s2 & WMASK;
if (vax_sword (s1) < vax_sword (s2))
    cmp_cc = CC_N;
else if (s1 == s2)
    cmp_cc = CC_Z;
if (s1 < s2)
    cmp_cc = cmp_cc | CC_C;
return cmp_cc;
}

/* Return CMPL condition codes for two VAX longword bit patterns.

   VAX compare uses signed ordering for N, equality for Z, unsigned
   ordering for C, and leaves V clear. */
static inline int32_t vax_cmp_l_cc (uint32_t s1, uint32_t s2)
{
int32_t cmp_cc = 0;

if (vax_signed_lt_l (s1, s2))
    cmp_cc = CC_N;
else if (s1 == s2)
    cmp_cc = CC_Z;
if (s1 < s2)
    cmp_cc = cmp_cc | CC_C;
return cmp_cc;
}

/* Shift a VAX longword right arithmetically using defined operations. */
static inline uint32_t vax_arith_rsh_l (uint32_t val, uint32_t sc)
{
if (sc == 0)
    return val;
if (sc >= 32)
    return (val & LSIGN)? LMASK: 0;
if (val & LSIGN)
    return (val >> sc) | (LMASK << (32 - sc));
return val >> sc;
}

#define CC_CMP_L(s1,s2) \
            cc = vax_cmp_l_cc ((uint32_t) (s1), (uint32_t) (s2))

/* Operand Memory vs Register Indicator */
#define OP_MEM          0xFFFFFFFF

/* Instruction History */
#define HIST_MIN        64
#define HIST_MAX        250000

#define OPND_SIZE       16
#define INST_SIZE       52

typedef struct {
    double              time;
    uint32_t            iPC;
    uint32_t            PSL;
    int32_t             opc;
    uint8_t             inst[INST_SIZE];
    uint32_t            opnd[OPND_SIZE];
    uint32_t            res[6];
    } InstHistory;


/* CPU Register definitions */

extern uint32_t R[16];                                  /* registers */
extern uint32_t STK[5];                                 /* stack pointers */
extern uint32_t PSL;                                    /* PSL */
extern uint32_t SCBB;                                   /* SCB base */
extern uint32_t PCBB;                                   /* PCB base */
extern uint32_t SBR, SLR;                               /* S0 mem mgt */
extern uint32_t P0BR, P0LR;                             /* P0 mem mgt */
extern uint32_t P1BR, P1LR;                             /* P1 mem mgt */
extern uint32_t ASTLVL;                                 /* AST Level */
extern uint32_t SISR;                                   /* swre int req */
extern int32_t pme;                                     /* perf mon enable */
extern uint32_t trpirq;                                 /* trap/intr req */
extern uint32_t fault_PC;                               /* fault PC */
extern uint32_t p1, p2;                                 /* fault parameters */
extern int32_t recq[];                                  /* recovery queue */
extern int32_t recqptr;                                 /* recq pointer */
extern uint32_t pcq[PCQ_SIZE];                          /* PC queue */
extern int32_t pcq_p;                                   /* PC queue ptr */
extern int32_t in_ie;                                   /* in exc, int */
extern int32_t ibcnt, ppc;                              /* prefetch ctl */
extern int32_t hlt_pin;                                 /* HLT pin intr */
extern int32_t mxpr_cc_vc;                              /* cc V & C bits from mtpr/mfpr operations */
extern int32_t mem_err;
extern int32_t crd_err;

/* Model dependent definitions */
int32_t eval_int (void);
int32_t machine_check (int32_t p1, int32_t opc, int32_t cc, int32_t delta);
int32_t get_vector (int32_t lvl);
int32_t con_halt (int32_t code, int32_t cc);
int32_t ReadIPR (int32_t rg);
void WriteIPR (int32_t rg, int32_t val);
t_stat cpu_boot (int32_t unitno, DEVICE *dptr);
t_stat build_dib_tab (void);
void rom_wr_B (int32_t pa, int32_t val);
extern int32_t cpu_instruction_set;

#if defined (VAX_780)
#include "vax780_defs.h"
#elif defined (VAX_750)
#include "vax750_defs.h"
#elif defined (VAX_730)
#include "vax730_defs.h"
#elif defined (VAX_410)
#include "vax410_defs.h"
#elif defined (VAX_420)
#include "vax420_defs.h"
#elif defined (VAX_43)
#include "vax43_defs.h"
#elif defined (VAX_440)
#include "vax440_defs.h"
#elif defined (IS_1000)
#include "is1000_defs.h"
#elif defined (VAX_610)
#include "vax610_defs.h"
#elif defined (VAX_620) || defined (VAX_630)
#include "vax630_defs.h"
#elif defined (VAX_820)
#include "vax820_defs.h"
#elif defined (VAX_860)
#include "vax860_defs.h"
#else /* VAX 3900 */
#include "vaxmod_defs.h"
#endif
#ifndef CPU_INSTRUCTION_SET
#if defined (FULL_VAX)
#define CPU_INSTRUCTION_SET FULL_INSTRUCTION_SET
#else
#define CPU_INSTRUCTION_SET VAX_FULL_BASE
#endif
#endif
#ifndef CPU_MODEL_MODIFIERS
#define CPU_MODEL_MODIFIERS             /* No model specific CPU modifiers */
#endif
#ifndef CPU_INST_MODIFIERS
#define CPU_INST_MODIFIERS  { MTAB_XTD|MTAB_VDV|MTAB_VALR|MTAB_NMO, 0, "INSTRUCTIONS", "INSTRUCTIONS={{NO}G-FLOAT|{NO}D-FLOAT|{NO}PACKED|{NO}EXTENDED|{NO}EMULATED}", \
                              &cpu_set_instruction_set, NULL, NULL,                 "Set the CPU Instruction Set" },                    \
                            { MTAB_XTD|MTAB_VDV, 0, "INSTRUCTIONS", NULL,                                                                    \
                              NULL,                     &cpu_show_instruction_set, NULL, "Show the CPU Instruction Set (SHOW -V)" },
#endif
#ifndef IDX_IMM_TEST
#define IDX_IMM_TEST RSVD_ADDR_FAULT
#endif

#include "sim_types.h"
#include "vax_watch.h"                  /* Watch chip definitions */

#ifdef DONT_USE_INTERNAL_ROM
#define BOOT_CODE_ARRAY NULL
#define BOOT_CODE_SIZE 0
#endif

t_stat cpu_print_model (FILE *st);
t_stat cpu_set_model (UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat cpu_model_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);
void vax_init(void);
extern int32_t autcon_enb;                              /* autoconfig enable */
extern int32_t int_req[IPL_HLVL];                       /* intr, IPL 14-17 */
extern int32_t ssc_bto;                                 /* SSC timeout */
extern int32_t sys_model;                               /* system model */
extern int32_t tmr_poll;                                /* program timer poll */
extern int32_t tmxr_poll;                               /* terminal mux poll */
extern uint32_t *M;                                     /* Memory */
extern DEVICE cpu_dev;                                  /* CPU */
extern UNIT cpu_unit;                                   /* CPU */
extern UNIT clk_unit;                                   /* clock */
extern REG cpu_reg[];                                   /* CPU registers */

#endif                                                  /* _VAX_DEFS_H */
