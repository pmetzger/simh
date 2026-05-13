/* vax_fpa.c - VAX f_, d_, g_floating instructions

   Copyright (c) 1998-2012, Robert M Supnik

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

   23-Mar-12    RMS     Fixed missing arguments in 32b floating add (Mark Pizzolato)
   15-Sep-11    RMS     Fixed integer overflow bug in EMODx
                        Fixed POLYx normalizing before add mask bug
                        (Camiel Vanderhoeven)
   28-May-08    RMS     Inlined physical memory routines
   16-May-06    RMS     Fixed bug in 32b floating multiply routine
                        Fixed bug in 64b extended modulus routine
   03-May-06    RMS     Fixed POLYD, POLYG to clear R4, R5
                        Fixed POLYD, POLYG to set R3 correctly
                        Fixed POLYD, POLYG to not exit prematurely if arg = 0
                        Fixed POLYD, POLYG to do full 64b multiply
                        Fixed POLYF, POLYD, POLYG to remove truncation on add
                        Fixed POLYF, POLYD, POLYG to mask mul reslt to 31b/63b/63b
                        Fixed fp add routine to test for zero via fraction
                         to support "denormal" argument from POLYF, POLYD, POLYG
                        (Tim Stark)
   27-Sep-05    RMS     Fixed bug in 32b structure definitions (Jason Stevens)
   30-Sep-04    RMS     Comment and formating changes based on vax_octa.c
   18-Apr-04    RMS     Moved format definitions to vax_defs.h
   19-Jun-03    RMS     Simplified add algorithm
   16-May-03    RMS     Fixed bug in floating to integer convert overflow
                        Fixed multiple bugs in EMODx
                        Integrated 32b only code
   05-Jul-02    RMS     Changed internal routine names for C library conflict
   17-Apr-02    RMS     Fixed bug in EDIV zero quotient

   This module contains the instruction simulators for

        - 64 bit arithmetic (ASHQ, EMUL, EDIV)
        - single precision floating point
        - double precision floating point, D and G format
*/

#include "vax_defs.h"
#include "vax_fpa.h"
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#if defined (USE_INT64)

#define M64             0xFFFFFFFFFFFFFFFF              /* 64b */
#define FD_FRACW        (0xFFFF & ~(FD_EXP | FPSIGN))
#define FD_FRACL        (FD_FRACW | 0xFFFF0000)         /* f/d fraction */
#define G_FRACW         (0xFFFF & ~(G_EXP | FPSIGN))
#define G_FRACL         (G_FRACW | 0xFFFF0000)          /* g fraction */
#define UNSCRAM(h,l)    (((((uint64_t) (h)) << 48) & 0xFFFF000000000000) | \
                        ((((uint64_t) (h)) << 16) & 0x0000FFFF00000000) | \
                        ((((uint64_t) (l)) << 16) & 0x00000000FFFF0000) | \
                        ((((uint64_t) (l)) >> 16) & 0x000000000000FFFF))
#define CONCAT(h,l)     ((((uint64_t) (h)) << 32) | ((uint32_t) (l)))

typedef struct {
    int32_t             sign;
    int32_t             exp;
    uint64_t            frac;
    } UFP;

#define UF_NM           0x8000000000000000              /* normalized */
#define UF_FRND         0x0000008000000000              /* F round */
#define UF_DRND         0x0000000000000080              /* D round */
#define UF_GRND         0x0000000000000400              /* G round */
#define UF_V_NM         63
#define UF_V_FDHI       40
#define UF_V_FDLO       (UF_V_FDHI - 32)
#define UF_V_GHI        43
#define UF_V_GLO        (UF_V_GHI - 32)
#define UF_GETFDHI(x)   (int32_t) ((((x) >> (16 + UF_V_FDHI)) & FD_FRACW) | \
                        (((x) >> (UF_V_FDHI - 16)) & ~0xFFFF))
#define UF_GETFDLO(x)   (int32_t) ((((x) >> (16 + UF_V_FDLO)) & 0xFFFF) | \
                        (((x) << (16 - UF_V_FDLO)) & ~0xFFFF))
#define UF_GETGHI(x)    (int32_t) ((((x) >> (16 + UF_V_GHI)) & G_FRACW) | \
                        (((x) >> (UF_V_GHI - 16)) & ~0xFFFF))
#define UF_GETGLO(x)    (int32_t) ((((x) >> (16 + UF_V_GLO)) & 0xFFFF) | \
                        (((x) << (16 - UF_V_GLO)) & ~0xFFFF))

static void unpackf (int32_t hi, UFP *a);
static void unpackd (int32_t hi, int32_t lo, UFP *a);
static void unpackg (int32_t hi, int32_t lo, UFP *a);
static void norm (UFP *a);
static int32_t rpackfd (UFP *a, int32_t *rh);
static int32_t rpackg (UFP *a, int32_t *rh);
static void vax_fadd (UFP *a, UFP *b, uint32_t mhi, uint32_t mlo);
static void vax_fmul (UFP *a, UFP *b, bool qd, int32_t bias, uint32_t mhi, uint32_t mlo);
static void vax_fdiv (UFP *a, UFP *b, int32_t prec, int32_t bias);
static void vax_fmod (UFP *a, int32_t bias, int32_t *intgr, int32_t *flg);

/* Shift a VAX quadword right arithmetically using defined operations. */
static inline uint64_t vax_arith_rsh_q (uint64_t val, uint32_t sc)
{
if (sc == 0)
    return val;
if (sc >= 64)
    return (val & (((uint64_t) LSIGN) << 32))? M64: 0;
if (val & (((uint64_t) LSIGN) << 32))
    return (val >> sc) | (M64 << (64 - sc));
return val >> sc;
}

/* Quadword arithmetic shift

        opnd[0]         =       shift count (cnt.rb)
        opnd[1:2]       =       source (src.rq)
        opnd[3:4]       =       destination (dst.wq)
*/

int32_t op_ashq (uint32_t *opnd, int32_t *rh, int32_t *flg)
{
uint64_t src, r;
uint32_t sc = opnd[0];

src = CONCAT (opnd[2], opnd[1]);                        /* build src */
if (sc & BSIGN) {                                       /* right shift? */
    *flg = 0;                                           /* no ovflo */
    sc = 0x100 - sc;                                    /* |shift| */
    r = vax_arith_rsh_q (src, sc);                      /* shift */
    }
else {
    if (sc > 63) {                                      /* left shift */
        r = 0;                                          /* sc > 63? */
        *flg = (src != 0);                              /* ovflo test */
        }
    else {
        r = (src << sc) & M64;                          /* do shift */
        *flg = (src != vax_arith_rsh_q (r, sc));        /* ovflo test */
        }
    }
*rh = (int32_t) ((r >> 32) & LMASK);                    /* hi result */
return ((int32_t) (r & LMASK));                         /* lo result */
}

/* Extended multiply subroutine */

int32_t op_emul (int32_t mpy, int32_t mpc, int32_t *rh)
{
int64_t lmpy = mpy;
int64_t lmpc = mpc;

lmpy = lmpy * lmpc;
*rh = (int32_t) ((lmpy >> 32) & LMASK);
return ((int32_t) (lmpy & LMASK));
}

/* Extended divide

        opnd[0]         =       divisor (non-zero)
        opnd[1:2]       =       dividend
*/

int32_t op_ediv (uint32_t *opnd, int32_t *rh, int32_t *flg)
{
uint64_t ldvd, ldvr;
uint32_t quo, rem;

*flg = CC_V;                                            /* assume error */
*rh = 0;
ldvr = (opnd[0] & LSIGN)? NEG (opnd[0]): opnd[0];       /* |divisor| */
ldvr = ldvr & LMASK;
ldvd = CONCAT (opnd[2], opnd[1]);                       /* 64b dividend */
if (opnd[2] & LSIGN)                                    /* |dividend| */
    ldvd = (~ldvd + 1) & M64;
if (((ldvd >> 32) & LMASK) >= ldvr)                     /* divide work? */
    return opnd[1];
quo = (uint32_t) (ldvd / ldvr);                         /* do divide */
rem = (uint32_t) (ldvd % ldvr);
if ((opnd[0] ^ opnd[2]) & LSIGN) {                      /* result -? */
    quo = NEG (quo);                                    /* negate */
    if (quo && ((quo & LSIGN) == 0))                    /* right sign? */
        return opnd[1];
    }
else if (quo & LSIGN)
    return opnd[1];
if (opnd[2] & LSIGN)                                    /* sign of rem */
    rem = NEG (rem);
*flg = 0;                                               /* no overflow */
*rh = rem & LMASK;                                      /* set rem */
return (quo & LMASK);                                   /* return quo */
}

/* Compare floating */

int32_t op_cmpfd (int32_t h1, int32_t l1, int32_t h2, int32_t l2)
{
uint64_t n1, n2;

if ((h1 & FD_EXP) == 0) {
    if (h1 & FPSIGN)
        RSVD_OPND_FAULT(op_cmpfd);
    h1 = l1 = 0;
    }
if ((h2 & FD_EXP) == 0) {
    if (h2 & FPSIGN)
        RSVD_OPND_FAULT(op_cmpfd);
    h2 = l2 = 0;
    }
if ((h1 ^ h2) & FPSIGN)
    return ((h1 & FPSIGN)? CC_N: 0);
n1 = UNSCRAM (h1, l1);
n2 = UNSCRAM (h2, l2);
if (n1 == n2)
    return CC_Z;
return (((n1 < n2) ^ ((h1 & FPSIGN) != 0))? CC_N: 0);
}

int32_t op_cmpg (int32_t h1, int32_t l1, int32_t h2, int32_t l2)
{
uint64_t n1, n2;

if ((h1 & G_EXP) == 0) {
    if (h1 & FPSIGN)
        RSVD_OPND_FAULT(op_cmpg);
    h1 = l1 = 0;
    }
if ((h2 & G_EXP) == 0) {
    if (h2 & FPSIGN)
        RSVD_OPND_FAULT(op_cmpg);
    h2 = l2 = 0;
    }
if ((h1 ^ h2) & FPSIGN)
    return ((h1 & FPSIGN)? CC_N: 0);
n1 = UNSCRAM (h1, l1);
n2 = UNSCRAM (h2, l2);
if (n1 == n2)
    return CC_Z;
return (((n1 < n2) ^ ((h1 & FPSIGN) != 0))? CC_N: 0);
}

/* Integer to floating convert */

int32_t op_cvtifdg (int32_t val, int32_t *rh, int32_t opc)
{
UFP a;
uint32_t mag;

if (val == 0) {
    if (rh)
        *rh = 0;
    return 0;
    }
if (val < 0) {
    a.sign = FPSIGN;
    mag = NEG ((uint32_t) val);
    }
else {
    a.sign = 0;
    mag = (uint32_t) val;
    }
a.exp = 32 + ((opc & 0x100)? G_BIAS: FD_BIAS);
a.frac = ((uint64_t) mag) << (UF_V_NM - 31);
norm (&a);
if (opc & 0x100)
    return rpackg (&a, rh);
return rpackfd (&a, rh);
}

/* Floating to integer convert */

int32_t op_cvtfdgi (uint32_t *opnd, int32_t *flg, int32_t opc)
{
UFP a;
int32_t lnt = opc & 03;
int32_t ubexp;
static uint64_t maxv[4] = { 0x7F, 0x7FFF, 0x7FFFFFFF, 0x7FFFFFFF };

*flg = 0;
if (opc & 0x100) {
    unpackg (opnd[0], opnd[1], &a);
    ubexp = a.exp - G_BIAS;
    }
else {
    if (opc & 0x20)
        unpackd (opnd[0], opnd[1], &a);
    else unpackf (opnd[0], &a);
    ubexp = a.exp - FD_BIAS;
    }
if ((a.exp == 0) || (ubexp < 0))
    return 0;
if (ubexp <= UF_V_NM) {
    a.frac = a.frac >> (UF_V_NM - ubexp);               /* leave rnd bit */
    if ((opc & 03) == 03)                               /* if CVTR, round */
        a.frac = a.frac + 1;
    a.frac = a.frac >> 1;                               /* now justified */
    if (a.frac > (maxv[lnt] + (a.sign? 1: 0)))
        *flg = CC_V;
    }
else {
    *flg = CC_V;                                        /* set overflow */
    if (ubexp > (UF_V_NM + 32))
        return 0;
    a.frac = a.frac << (ubexp - UF_V_NM - 1);           /* no rnd bit */
    }
return ((int32_t) ((a.sign? (a.frac ^ LMASK) + 1: a.frac) & LMASK));
}

/* Extended modularize

   One of three floating point instructions dropped from the architecture,
   EMOD presents two sets of complications.  First, it requires an extended
   fraction multiply, with precise (and unusual) truncation conditions.
   Second, it has two write operands, a dubious distinction it shares
   with EDIV.
*/

int32_t op_emodf (uint32_t *opnd, int32_t *intgr, int32_t *flg)
{
UFP a, b;

unpackf (opnd[0], &a);                                  /* unpack operands */
unpackf (opnd[2], &b);
a.frac = a.frac | (((uint64_t) opnd[1]) << 32);         /* extend src1 */
vax_fmul (&a, &b, 0, FD_BIAS, 0, LMASK);                /* multiply */
vax_fmod (&a, FD_BIAS, intgr, flg);                     /* sep int & frac */
return rpackfd (&a, NULL);                              /* return frac */
}

int32_t op_emodd (uint32_t *opnd, int32_t *flo, int32_t *intgr, int32_t *flg)
{
UFP a, b;

unpackd (opnd[0], opnd[1], &a);                         /* unpack operands */
unpackd (opnd[3], opnd[4], &b);
a.frac = a.frac | opnd[2];                              /* extend src1 */
vax_fmul (&a, &b, 1, FD_BIAS, 0, 0);                    /* multiply */
vax_fmod (&a, FD_BIAS, intgr, flg);                     /* sep int & frac */
return rpackfd (&a, flo);                               /* return frac */
}

int32_t op_emodg (uint32_t *opnd, int32_t *flo, int32_t *intgr, int32_t *flg)
{
UFP a, b;

unpackg (opnd[0], opnd[1], &a);                         /* unpack operands */
unpackg (opnd[3], opnd[4], &b);
a.frac = a.frac | (opnd[2] >> 5);                       /* extend src1 */
vax_fmul (&a, &b, 1, G_BIAS, 0, 0);                     /* multiply */
vax_fmod (&a, G_BIAS, intgr, flg);                      /* sep int & frac */
return rpackg (&a, flo);                                /* return frac */
}

/* Unpacked floating point routines */

static void vax_fadd (UFP *a, UFP *b, uint32_t mhi, uint32_t mlo)
{
int32_t ediff;
UFP t;
uint64_t mask = (((uint64_t) mhi) << 32) | ((uint64_t) mlo);
uint64_t neg_frac;

if (a->frac == 0) {                                     /* s1 = 0? */
    *a = *b;
    return;
    }
if (b->frac == 0)                                       /* s2 = 0? */
    return;
if ((a->exp < b->exp) ||                                /* |s1| < |s2|? swap */
    ((a->exp == b->exp) && (a->frac < b->frac))) {
    t = *a;
    *a = *b;
    *b = t;
    }
ediff = a->exp - b->exp;                                /* exp diff */
if (a->sign ^ b->sign) {                                /* eff sub? */
    if (ediff) {                                        /* exp diff? */
        if (ediff > 63)                                 /* retain sticky */
            b->frac = M64;
        else {
            neg_frac = (~b->frac + 1) & M64;
            b->frac = (neg_frac >> ediff) |             /* denormalize */
                (M64 << (64 - ediff));                  /* preserve sign */
            }
        a->frac = a->frac + b->frac;                    /* add frac */
        }
    else a->frac = a->frac - b->frac;                   /* sub frac */
    a->frac = a->frac & ~mask;                          /* mask before norm */
    norm (a);                                           /* normalize */
    }
else {
    if (ediff > 63)                                     /* add */
        b->frac = 0;
    else if (ediff)                                     /* denormalize */
        b->frac = b->frac >> ediff;
    a->frac = a->frac + b->frac;                        /* add frac */
    if (a->frac < b->frac) {                            /* chk for carry */
        a->frac = UF_NM | (a->frac >> 1);               /* shift in carry */
        a->exp = a->exp + 1;                            /* skip norm */
        }
    a->frac = a->frac & ~mask;                          /* mask */
    }
return;
}

/* Floating multiply - 64b * 64b with cross products */

static void vax_fmul (UFP *a, UFP *b, bool qd, int32_t bias, uint32_t mhi, uint32_t mlo)
{
uint64_t ah, bh, al, bl, rhi, rlo, rmid1, rmid2;
uint64_t mask = (((uint64_t) mhi) << 32) | ((uint64_t) mlo);

if ((a->exp == 0) || (b->exp == 0)) {                   /* zero argument? */
    a->frac = a->sign = a->exp = 0;                     /* result is zero */
    return;
    }
a->sign = a->sign ^ b->sign;                            /* sign of result */
a->exp = a->exp + b->exp - bias;                        /* add exponents */
ah = (a->frac >> 32) & LMASK;                           /* split operands */
bh = (b->frac >> 32) & LMASK;                           /* into 32b chunks */
rhi = ah * bh;                                          /* high result */
if (qd) {                                               /* 64b needed? */
    al = a->frac & LMASK;
    bl = b->frac & LMASK;
    rmid1 = ah * bl;
    rmid2 = al * bh;
    rlo = al * bl;
    rhi = rhi + ((rmid1 >> 32) & LMASK) + ((rmid2 >> 32) & LMASK);
    rmid1 = rlo + (rmid1 << 32);                        /* add mid1 to lo */
    if (rmid1 < rlo)                                    /* carry? incr hi */
        rhi = rhi + 1;
    rmid2 = rmid1 + (rmid2 << 32);                      /* add mid2 to lo */
    if (rmid2 < rmid1)                                  /* carry? incr hi */
        rhi = rhi + 1;
    }
a->frac = rhi & ~mask;
norm (a);                                               /* normalize */
return;
}

/* Floating modulus - there are three cases

   exp <= bias                  - integer is 0, fraction is input,
                                  no overflow
   bias < exp <= bias+64        - separate integer and fraction,
                                  integer overflow may occur
   bias+64 < exp                - result is integer, fraction is 0
                                  integer overflow
*/

static void vax_fmod (UFP *a, int32_t bias, int32_t *intgr, int32_t *flg)
{
if (a->exp <= bias)                                     /* 0 or <1? int = 0 */
    *intgr = *flg = 0;
else if (a->exp <= (bias + 64)) {                       /* in range [1,64]? */
    *intgr = (int32_t) (a->frac >> (64 - (a->exp - bias)));
    if ((a->exp > (bias + 32)) ||                       /* test ovflo */
        ((a->exp == (bias + 32)) &&
         (((uint32_t) *intgr) > (a->sign? 0x80000000: 0x7FFFFFFF))))
        *flg = CC_V;
    else *flg = 0;
    if (a->sign)                                        /* -? comp int */
        *intgr = NEG ((uint32_t) *intgr);
    if (a->exp == (bias + 64))                          /* special case 64 */
        a->frac = 0;
    else a->frac = a->frac << (a->exp - bias);
    a->exp = bias;
    }
else {
    if (a->exp < (bias + 96))                           /* need left shift? */
        *intgr = (int32_t) (a->frac << (a->exp - bias - 64));
    else *intgr = 0;                                    /* out of range */
    if (a->sign)
        *intgr = NEG ((uint32_t) *intgr);
    a->frac = a->sign = a->exp = 0;                     /* result 0 */
    *flg = CC_V;                                        /* overflow */
    }
norm (a);                                               /* normalize */
return;
}

/* Floating divide
   Needs to develop at least one rounding bit.  Since the first
   divide step can fail, caller should specify 2 more bits than
   the precision of the fraction.
*/

static void vax_fdiv (UFP *a, UFP *b, int32_t prec, int32_t bias)
{
int32_t i;
uint64_t quo = 0;

if (a->exp == 0)                                        /* divr = 0? */
    FLT_DZRO_FAULT;
if (b->exp == 0)                                        /* divd = 0? */
    return;
b->sign = b->sign ^ a->sign;                            /* result sign */
b->exp = b->exp - a->exp + bias + 1;                    /* unbiased exp */
a->frac = a->frac >> 1;                                 /* allow 1 bit left */
b->frac = b->frac >> 1;
for (i = 0; (i < prec) && b->frac; i++) {               /* divide loop */
    quo = quo << 1;                                     /* shift quo */
    if (b->frac >= a->frac) {                           /* div step ok? */
        b->frac = b->frac - a->frac;                    /* subtract */
        quo = quo + 1;                                  /* quo bit = 1 */
        }
    b->frac = b->frac << 1;                             /* shift divd */
    }
b->frac = quo << (UF_V_NM - i + 1);                     /* shift quo */
norm (b);                                               /* normalize */
return;
}

/* Support routines */

static void unpackf (int32_t hi, UFP *r)
{
r->sign = hi & FPSIGN;                                  /* get sign */
r->exp = FD_GETEXP (hi);                                /* get exponent */
if (r->exp == 0) {                                      /* exp = 0? */
    if (r->sign)                                        /* if -, rsvd op */
        RSVD_OPND_FAULT(unpackf);
    r->frac = 0;                                        /* else 0 */
    return;
    }
hi = (((hi & FD_FRACW) | FD_HB) << 16) | ((hi >> 16) & 0xFFFF);
r->frac = ((uint64_t) hi) << (32 + UF_V_FDLO);
return;
}

static void unpackd (int32_t hi, int32_t lo, UFP *r)
{
r->sign = hi & FPSIGN;                                  /* get sign */
r->exp = FD_GETEXP (hi);                                /* get exponent */
if (r->exp == 0) {                                      /* exp = 0? */
    if (r->sign)                                        /* if -, rsvd op */
        RSVD_OPND_FAULT(unpackd);
    r->frac = 0;                                        /* else 0 */
    return;
    }
hi = (hi & FD_FRACL) | FD_HB;                           /* canonical form */
r->frac = UNSCRAM (hi, lo) << UF_V_FDLO;                /* guard bits */
return;
}

static void unpackg (int32_t hi, int32_t lo, UFP *r)
{
r->sign = hi & FPSIGN;                                  /* get sign */
r->exp = G_GETEXP (hi);                                 /* get exponent */
if (r->exp == 0) {                                      /* exp = 0? */
    if (r->sign)                                        /* if -, rsvd op */
        RSVD_OPND_FAULT(unpackg);
    r->frac = 0;                                        /* else 0 */
    return;
    }
hi = (hi & G_FRACL) | G_HB;                             /* canonical form */
r->frac = UNSCRAM (hi, lo) << UF_V_GLO;                 /* guard bits */
return;
}

static void norm (UFP *r)
{
int32_t i;
static uint64_t normmask[5] = {
 0xc000000000000000, 0xf000000000000000, 0xff00000000000000,
 0xffff000000000000, 0xffffffff00000000
 };
static int32_t normtab[6] = { 1, 2, 4, 8, 16, 32};

if (r->frac == 0) {                                     /* if fraction = 0 */
    r->sign = r->exp = 0;                               /* result is 0 */
    return;
    }
while ((r->frac & UF_NM) == 0) {                        /* normalized? */
    for (i = 0; i < 5; i++) {                           /* find first 1 */
        if (r->frac & normmask[i])
            break;
        }
    r->frac = r->frac << normtab[i];                    /* shift frac */
    r->exp = r->exp - normtab[i];                       /* decr exp */
    }
return;
}

static int32_t rpackfd (UFP *r, int32_t *rh)
{
if (rh)                                                 /* assume 0 */
    *rh = 0;
if (r->frac == 0)                                       /* result 0? */
    return 0;
r->frac = r->frac + (rh? UF_DRND: UF_FRND);             /* round */
if ((r->frac & UF_NM) == 0) {                           /* carry out? */
    r->frac = r->frac >> 1;                             /* renormalize */
    r->exp = r->exp + 1;
    }
if (r->exp > (int32_t) FD_M_EXP)                        /* ovflo? fault */
    FLT_OVFL_FAULT;
if (r->exp <= 0) {                                      /* underflow? */
    if (PSL & PSW_FU)                                   /* fault if fu */
        FLT_UNFL_FAULT;
    return 0;                                           /* else 0 */
    }
if (rh)                                                 /* get low */
    *rh = UF_GETFDLO (r->frac);
return r->sign | (r->exp << FD_V_EXP) | UF_GETFDHI (r->frac);
}

static int32_t rpackg (UFP *r, int32_t *rh)
{
*rh = 0;                                                /* assume 0 */
if (r->frac == 0)                                       /* result 0? */
    return 0;
r->frac = r->frac + UF_GRND;                            /* round */
if ((r->frac & UF_NM) == 0) {                           /* carry out? */
    r->frac = r->frac >> 1;                             /* renormalize */
    r->exp = r->exp + 1;
    }
if (r->exp > (int32_t) G_M_EXP)                         /* ovflo? fault */
    FLT_OVFL_FAULT;
if (r->exp <= 0) {                                      /* underflow? */
    if (PSL & PSW_FU)                                   /* fault if fu */
        FLT_UNFL_FAULT;
    return 0;                                           /* else 0 */
    }
*rh = UF_GETGLO (r->frac);                              /* get low */
return r->sign | (r->exp << G_V_EXP) | UF_GETGHI (r->frac);
}

#else                                                   /* 32b code */

#define WORDSWAP(x)     ((((x) & WMASK) << 16) | (((x) >> 16) & WMASK))

typedef struct {
    uint32_t            lo;
    uint32_t            hi;
    } UDP;

typedef struct {
    int32_t             sign;
    int32_t             exp;
    UDP                 frac;
    } UFP;

#define UF_NM_H         0x80000000                      /* normalized */
#define UF_FRND_H       0x00000080                      /* F round */
#define UF_FRND_L       0x00000000
#define UF_DRND_H       0x00000000                      /* D round */
#define UF_DRND_L       0x00000080
#define UF_GRND_H       0x00000000                      /* G round */
#define UF_GRND_L       0x00000400
#define UF_V_NM         63

static void unpackf (uint32_t hi, UFP *a);
static void unpackd (uint32_t hi, uint32_t lo, UFP *a);
static void unpackg (uint32_t hi, uint32_t lo, UFP *a);
static void norm (UFP *a);
static int32_t rpackfd (UFP *a, int32_t *rh);
static int32_t rpackg (UFP *a, int32_t *rh);
static void vax_fadd (UFP *a, UFP *b, uint32_t mhi, uint32_t mlo);
static void vax_fmul (UFP *a, UFP *b, bool qd, int32_t bias, uint32_t mhi, uint32_t mlo);
static void vax_fmod (UFP *a, int32_t bias, int32_t *intgr, int32_t *flg);
static void vax_fdiv (UFP *a, UFP *b, int32_t prec, int32_t bias);
static void dp_add (UDP *a, UDP *b);
static void dp_inc (UDP *a);
static void dp_sub (UDP *a, UDP *b);
static void dp_imul (uint32_t a, uint32_t b, UDP *r);
static void dp_lsh (UDP *a, uint32_t sc);
static void dp_rsh (UDP *a, uint32_t sc);
static void dp_rsh_s (UDP *a, uint32_t sc, uint32_t neg);
static void dp_neg (UDP *a);
static int32_t dp_cmp (UDP *a, UDP *b);

/* Quadword arithmetic shift

        opnd[0]         =       shift count (cnt.rb)
        opnd[1:2]       =       source (src.rq)
        opnd[3:4]       =       destination (dst.wq)
*/

int32_t op_ashq (uint32_t *opnd, int32_t *rh, int32_t *flg)
{
UDP r, sr;
uint32_t sc = opnd[0];

r.lo = opnd[1];                                         /* get source */
r.hi = opnd[2];
*flg = 0;                                               /* assume no ovflo */
if (sc & BSIGN)                                         /* right shift? */
    dp_rsh_s (&r, 0x100 - sc, r.hi & LSIGN);            /* signed right */
else {
    dp_lsh (&r, sc);                                    /* left shift */
    sr = r;                                             /* copy result */
    dp_rsh_s (&sr, sc, sr.hi & LSIGN);                  /* signed right */
    if ((sr.hi != ((uint32_t) opnd[2])) ||              /* reshift != orig? */
        (sr.lo != ((uint32_t) opnd[1]))) *flg = 1;      /* overflow */
    }
*rh = r.hi;                                             /* hi result */
return r.lo;                                            /* lo result */
}

/* Extended multiply subroutine */

int32_t op_emul (int32_t mpy, int32_t mpc, int32_t *rh)
{
UDP r;
uint32_t umpy = (uint32_t) mpy;
uint32_t umpc = (uint32_t) mpc;
uint32_t sign = umpy ^ umpc;                            /* sign of result */

if (umpy & LSIGN)                                       /* abs value */
    umpy = NEG (umpy);
if (umpc & LSIGN)
    umpc = NEG (umpc);
dp_imul (umpy & LMASK, umpc & LMASK, &r);               /* 32b * 32b -> 64b */
if (sign & LSIGN)                                       /* negative result? */
    dp_neg (&r);
*rh = r.hi;
return r.lo;
}

/* Extended divide

        opnd[0]         =       divisor (non-zero)
        opnd[1:2]       =       dividend
*/

int32_t op_ediv (uint32_t *opnd, int32_t *rh, int32_t *flg)
{
UDP dvd;
uint32_t i, dvr, quo;

dvr = opnd[0];                                          /* get divisor */
dvd.lo = opnd[1];                                       /* get dividend */
dvd.hi = opnd[2];
*flg = CC_V;                                            /* assume error */
*rh = 0;
if (dvd.hi & LSIGN)                                     /* |dividend| */
    dp_neg (&dvd);
if (dvr & LSIGN)                                        /* |divisor| */
    dvr = NEG (dvr);
if (dvd.hi >= dvr)                                      /* divide work? */
    return opnd[1];
for (i = quo = 0; i < 32; i++) {                        /* 32 iterations */
    quo = quo << 1;                                     /* shift quotient */
    dp_lsh (&dvd, 1);                                   /* shift dividend */
    if (dvd.hi >= dvr) {                                /* step work? */
        dvd.hi = (dvd.hi - dvr) & LMASK;                /* subtract dvr */
        quo = quo + 1;
        }
    }
if ((opnd[0] ^ opnd[2]) & LSIGN) {                      /* result -? */
    quo = NEG (quo);                                    /* negate */
    if (quo && ((quo & LSIGN) == 0))                    /* right sign? */
        return opnd[1];
    }
else if (quo & LSIGN)
    return opnd[1];
if (opnd[2] & LSIGN)                                    /* sign of rem */
    *rh = NEG (dvd.hi);
else *rh = dvd.hi;
*flg = 0;                                               /* no overflow */
return quo;                                             /* return quo */
}

/* Compare floating */

int32_t op_cmpfd (int32_t h1, int32_t l1, int32_t h2, int32_t l2)
{
UFP a, b;
int32_t r;

unpackd (h1, l1, &a);
unpackd (h2, l2, &b);
if (a.sign != b.sign)
    return (a.sign? CC_N: 0);
r = a.exp - b.exp;
if (r == 0)
    r = dp_cmp (&a.frac, &b.frac);
if (r < 0)
    return (a.sign? 0: CC_N);
if (r > 0)
    return (a.sign? CC_N: 0);
return CC_Z;
}

int32_t op_cmpg (int32_t h1, int32_t l1, int32_t h2, int32_t l2)
{
UFP a, b;
int32_t r;

unpackg (h1, l1, &a);
unpackg (h2, l2, &b);
if (a.sign != b.sign)
    return (a.sign? CC_N: 0);
r = a.exp - b.exp;
if (r == 0)
    r = dp_cmp (&a.frac, &b.frac);
if (r < 0)
    return (a.sign? 0: CC_N);
if (r > 0)
    return (a.sign? CC_N: 0);
return CC_Z;
}

/* Integer to floating convert */

int32_t op_cvtifdg (int32_t val, int32_t *rh, int32_t opc)
{
UFP a;
uint32_t mag;

if (val == 0) {                                         /* zero? */
    if (rh) *rh = 0;                                    /* return true 0 */
    return 0;
    }
if (val < 0) {                                          /* negative? */
    a.sign = FPSIGN;                                    /* sign = - */
    mag = NEG ((uint32_t) val);
    }
else {
    a.sign = 0;                                         /* else sign = + */
    mag = (uint32_t) val;
    }
a.exp = 32 + ((opc & 0x100)? G_BIAS: FD_BIAS);          /* initial exp */
a.frac.hi = mag & LMASK;                                /* fraction */
a.frac.lo = 0;
norm (&a);                                              /* normalize */
if (opc & 0x100)                                        /* pack and return */
    return rpackg (&a, rh);
return rpackfd (&a, rh);
}

/* Floating to integer convert */

int32_t op_cvtfdgi (uint32_t *opnd, int32_t *flg, int32_t opc)
{
UFP a;
int32_t lnt = opc & 03;
int32_t ubexp;
static uint32_t maxv[4] = { 0x7F, 0x7FFF, 0x7FFFFFFF, 0x7FFFFFFF };

*flg = 0;
if (opc & 0x100) {                                      /* G? */
    unpackg (opnd[0], opnd[1], &a);                     /* unpack */
    ubexp = a.exp - G_BIAS;                             /* unbiased exp */
    }
else {
    if (opc & 0x20)                                     /* F or D */
        unpackd (opnd[0], opnd[1], &a);
    else unpackf (opnd[0], &a);                         /* unpack */
    ubexp = a.exp - FD_BIAS;                            /* unbiased exp */
    }
if ((a.exp == 0) || (ubexp < 0))                        /* true zero or frac? */
    return 0;
if (ubexp <= UF_V_NM) {                                 /* exp in range? */
    dp_rsh (&a.frac, UF_V_NM - ubexp);                  /* leave rnd bit */
    if (lnt == 03)                                      /* if CVTR, round */
        dp_inc (&a.frac);
    dp_rsh (&a.frac, 1);                                /* now justified */
    if ((a.frac.hi != 0) ||
        (a.frac.lo > (maxv[lnt] + (a.sign? 1: 0))))
        *flg = CC_V;
    }
else {
    *flg = CC_V;                                        /* always ovflo */
    if (ubexp > (UF_V_NM + 32))                         /* in ext range? */
        return 0;
    dp_lsh (&a.frac, ubexp - UF_V_NM - 1);              /* no rnd bit */
    }
return (a.sign? NEG (a.frac.lo): a.frac.lo);            /* return lo frac */
}

/* Extended modularize

   One of three floating point instructions dropped from the architecture,
   EMOD presents two sets of complications.  First, it requires an extended
   fraction multiply, with precise (and unusual) truncation conditions.
   Second, it has two write operands, a dubious distinction it shares
   with EDIV.
*/

int32_t op_emodf (uint32_t *opnd, int32_t *intgr, int32_t *flg)
{
UFP a, b;

unpackf (opnd[0], &a);                                  /* unpack operands */
unpackf (opnd[2], &b);
a.frac.hi = a.frac.hi | opnd[1];                        /* extend src1 */
vax_fmul (&a, &b, 0, FD_BIAS, 0, LMASK);                /* multiply */
vax_fmod (&a, FD_BIAS, intgr, flg);                     /* sep int & frac */
return rpackfd (&a, NULL);                              /* return frac */
}

int32_t op_emodd (uint32_t *opnd, int32_t *flo, int32_t *intgr, int32_t *flg)
{
UFP a, b;

unpackd (opnd[0], opnd[1], &a);                         /* unpack operands */
unpackd (opnd[3], opnd[4], &b);
a.frac.lo = a.frac.lo | opnd[2];                        /* extend src1 */
vax_fmul (&a, &b, 1, FD_BIAS, 0, 0);                    /* multiply */
vax_fmod (&a, FD_BIAS, intgr, flg);                     /* sep int & frac */
return rpackfd (&a, flo);                               /* return frac */
}

int32_t op_emodg (uint32_t *opnd, int32_t *flo, int32_t *intgr, int32_t *flg)
{
UFP a, b;

unpackg (opnd[0], opnd[1], &a);                         /* unpack operands */
unpackg (opnd[3], opnd[4], &b);
a.frac.lo = a.frac.lo | (opnd[2] >> 5);                 /* extend src1 */
vax_fmul (&a, &b, 1, G_BIAS, 0, 0);                     /* multiply */
vax_fmod (&a, G_BIAS, intgr, flg);                      /* sep int & frac */
return rpackg (&a, flo);                                /* return frac */
}

/* Unpacked floating point routines */

/* Floating add */

static void vax_fadd (UFP *a, UFP *b, uint32_t mhi, uint32_t mlo)
{
int32_t ediff;
UFP t;

if ((a->frac.hi == 0) && (a->frac.lo == 0)) {           /* s1 = 0? */
    *a = *b;
    return;
    }
if ((b->frac.hi == 0) && (b->frac.lo == 0))             /* s2 = 0? */
    return;
if ((a->exp < b->exp) ||                                /* |s1| < |s2|? swap */
    ((a->exp == b->exp) && (dp_cmp (&a->frac, &b->frac) < 0))) {
    t = *a;
    *a = *b;
    *b = t;
    }
ediff = a->exp - b->exp;                                /* exp diff */
if (a->sign ^ b->sign) {                                /* eff sub? */
    if (ediff) {                                        /* exp diff? */
        dp_neg (&b->frac);                              /* negate fraction */
        dp_rsh_s (&b->frac, ediff, 1);                  /* signed right */
        dp_add (&a->frac, &b->frac);                    /* "add" frac */
        }
    else dp_sub (&a->frac, &b->frac);                   /* a >= b */
    a->frac.hi = a->frac.hi & ~mhi;                     /* mask before norm */
    a->frac.lo = a->frac.lo & ~mlo;
    norm (a);                                           /* normalize */
    }
else {
    if (ediff)                                          /* add, denormalize */
        dp_rsh (&b->frac, ediff);
    dp_add (&a->frac, &b->frac);                        /* add frac */
    if (dp_cmp (&a->frac, &b->frac) < 0) {              /* chk for carry */
        dp_rsh (&a->frac, 1);                           /* renormalize */
        a->frac.hi = a->frac.hi | UF_NM_H;              /* add norm bit */
        a->exp = a->exp + 1;                            /* skip norm */
        }
    a->frac.hi = a->frac.hi & ~mhi;                     /* mask */
    a->frac.lo = a->frac.lo & ~mlo;
    }
return;
}

/* Floating multiply - 64b * 64b with cross products */

static void vax_fmul (UFP *a, UFP *b, bool qd, int32_t bias, uint32_t mhi, uint32_t mlo)
{
UDP rhi, rlo, rmid1, rmid2;

if ((a->exp == 0) || (b->exp == 0)) {                   /* zero argument? */
    a->frac.hi = a->frac.lo = 0;                        /* result is zero */
    a->sign = a->exp = 0;
    return;
    }
a->sign = a->sign ^ b->sign;                            /* sign of result */
a->exp = a->exp + b->exp - bias;                        /* add exponents */
dp_imul (a->frac.hi, b->frac.hi, &rhi);                 /* high result */
if (qd) {                                               /* 64b needed? */
    dp_imul (a->frac.hi, b->frac.lo, &rmid1);           /* cross products */
    dp_imul (a->frac.lo, b->frac.hi, &rmid2);
    dp_imul (a->frac.lo, b->frac.lo, &rlo);             /* low result */
    rhi.lo = (rhi.lo + rmid1.hi) & LMASK;               /* add hi cross */
    if (rhi.lo < rmid1.hi)                              /* to low high res */
        rhi.hi = (rhi.hi + 1) & LMASK;
    rhi.lo = (rhi.lo + rmid2.hi) & LMASK;
    if (rhi.lo < rmid2.hi)
         rhi.hi = (rhi.hi + 1) & LMASK;
    rlo.hi = (rlo.hi + rmid1.lo) & LMASK;               /* add mid1 to low res */
    if (rlo.hi < rmid1.lo)                              /* carry? incr high res */
        dp_inc (&rhi);
    rlo.hi = (rlo.hi + rmid2.lo) & LMASK;               /* add mid2 to low res */
    if (rlo.hi < rmid2.lo)                              /* carry? incr high res */
        dp_inc (&rhi);
    }
a->frac.hi = rhi.hi & ~mhi;                             /* mask fraction */
a->frac.lo = rhi.lo & ~mlo;
norm (a);                                               /* normalize */
return;
}

/* Floating modulus - there are three cases

   exp <= bias                  - integer is 0, fraction is input,
                                  no overflow
   bias < exp <= bias+64        - separate integer and fraction,
                                  integer overflow may occur
   bias+64 < exp                - result is integer, fraction is 0
                                  integer overflow
*/

static void vax_fmod (UFP *a, int32_t bias, int32_t *intgr, int32_t *flg)
{
UDP ifr;

if (a->exp <= bias)                                     /* 0 or <1? int = 0 */
    *intgr = *flg = 0;
else if (a->exp <= (bias + 64)) {                       /* in range [1,64]? */
    ifr = a->frac;
    dp_rsh (&ifr, 64 - (a->exp - bias));                /* separate integer */
    if ((a->exp > (bias + 32)) ||                       /* test ovflo */
        ((a->exp == (bias + 32)) &&
         (ifr.lo > (a->sign? 0x80000000: 0x7FFFFFFF))))
        *flg = CC_V;
    else *flg = 0;
    *intgr = ifr.lo;
    if (a->sign)                                        /* -? comp int */
        *intgr = NEG ((uint32_t) *intgr);
    dp_lsh (&a->frac, a->exp - bias);                   /* excise integer */
    a->exp = bias;
    }
else {
    if (a->exp < (bias + 96)) {                         /* need left shift? */
        ifr = a->frac;
        dp_lsh (&ifr, a->exp - bias - 64);
        *intgr = ifr.lo;
        }
    else *intgr = 0;                                    /* out of range */
    if (a->sign)
        *intgr = NEG ((uint32_t) *intgr);
    a->frac.hi = a->frac.lo = a->sign = a->exp = 0;     /* result 0 */
    *flg = CC_V;                                        /* overflow */
    }
norm (a);                                               /* normalize */
return;
}

/* Floating divide
   Needs to develop at least one rounding bit.  Since the first
   divide step can fail, caller should specify 2 more bits than
   the precision of the fraction.
*/

static void vax_fdiv (UFP *a, UFP *b, int32_t prec, int32_t bias)
{
int32_t i;
UDP quo = { 0, 0 };

if (a->exp == 0)                                        /* divr = 0? */
    FLT_DZRO_FAULT;
if (b->exp == 0)                                        /* divd = 0? */
    return;
b->sign = b->sign ^ a->sign;                            /* result sign */
b->exp = b->exp - a->exp + bias + 1;                    /* unbiased exp */
dp_rsh (&a->frac, 1);                                   /* allow 1 bit left */
dp_rsh (&b->frac, 1);
for (i = 0; i < prec; i++) {                            /* divide loop */
    dp_lsh (&quo, 1);                                   /* shift quo */
    if (dp_cmp (&b->frac, &a->frac) >= 0) {             /* div step ok? */
        dp_sub (&b->frac, &a->frac);                    /* subtract */
        quo.lo = quo.lo + 1;                            /* quo bit = 1 */
        }
    dp_lsh (&b->frac, 1);                               /* shift divd */
    }
dp_lsh (&quo, UF_V_NM - prec + 1);                      /* put in position */
b->frac = quo;
norm (b);                                               /* normalize */
return;
}

/* Double precision integer routines */

static int32_t dp_cmp (UDP *a, UDP *b)
{
if (a->hi < b->hi)                                      /* compare hi */
    return -1;
if (a->hi > b->hi)
    return +1;
if (a->lo < b->lo)                                      /* hi =, compare lo */
    return -1;
if (a->lo > b->lo)
    return +1;
return 0;                                               /* hi, lo equal */
}

static void dp_add (UDP *a, UDP *b)
{
a->lo = (a->lo + b->lo) & LMASK;                        /* add lo */
if (a->lo < b->lo)                                      /* carry? */
    a->hi = a->hi + 1;
a->hi = (a->hi + b->hi) & LMASK;                        /* add hi */
return;
}

static void dp_inc (UDP *a)
{
a->lo = (a->lo + 1) & LMASK;                            /* inc lo */
if (a->lo == 0)                                         /* carry? inc hi */
    a->hi = (a->hi + 1) & LMASK;
return;
}

static void dp_sub (UDP *a, UDP *b)
{
if (a->lo < b->lo)                                      /* borrow? decr hi */
    a->hi = a->hi - 1;
a->lo = (a->lo - b->lo) & LMASK;                        /* sub lo */
a->hi = (a->hi - b->hi) & LMASK;                        /* sub hi */
return;
}

static void dp_lsh (UDP *r, uint32_t sc)
{
if (sc > 63)                                            /* > 63? result 0 */
    r->hi = r->lo = 0;
else if (sc > 31) {                                     /* [32,63]? */
    r->hi = (r->lo << (sc - 32)) & LMASK;
    r->lo = 0;
    }
else if (sc != 0) {
    r->hi = ((r->hi << sc) | (r->lo >> (32 - sc))) & LMASK;
    r->lo = (r->lo << sc) & LMASK;
    }
return;
}

static void dp_rsh (UDP *r, uint32_t sc)
{
if (sc > 63)                                            /* > 63? result 0 */
    r->hi = r->lo = 0;
else if (sc > 31) {                                     /* [32,63]? */
    r->lo = (r->hi >> (sc - 32)) & LMASK;
    r->hi = 0;
    }
else if (sc != 0) {
    r->lo = ((r->lo >> sc) | (r->hi << (32 - sc))) & LMASK;
    r->hi = (r->hi >> sc) & LMASK;
    }
return;
}

static void dp_rsh_s (UDP *r, uint32_t sc, uint32_t neg)
{
dp_rsh (r, sc);                                         /* do unsigned right */
if (neg && sc) {                                        /* negative? */
    if (sc > 63)                                        /* > 63? result -1 */
        r->hi = r->lo = LMASK;
    else {
        UDP ones = { LMASK, LMASK };
        dp_lsh (&ones, 64 - sc);                        /* shift ones */
        r->hi = r->hi | ones.hi;                        /* or into result */
        r->lo = r->lo | ones.lo;
        }
    }
return;
}

static void dp_imul (uint32_t a, uint32_t b, UDP *r)
{
uint32_t ah, bh, al, bl, rhi, rlo, rmid1, rmid2;

if ((a == 0) || (b == 0)) {                             /* zero argument? */
    r->hi = r->lo = 0;                                  /* result is zero */
    return;
    }
ah = (a >> 16) & WMASK;                                 /* split operands */
bh = (b >> 16) & WMASK;                                 /* into 16b chunks */
al = a & WMASK;
bl = b & WMASK;
rhi = ah * bh;                                          /* high result */
rmid1 = ah * bl;
rmid2 = al * bh;
rlo = al * bl;
rhi = rhi + ((rmid1 >> 16) & WMASK) + ((rmid2 >> 16) & WMASK);
rmid1 = (rlo + (rmid1 << 16)) & LMASK;                  /* add mid1 to lo */
if (rmid1 < rlo)                                        /* carry? incr hi */
    rhi = rhi + 1;
rmid2 = (rmid1 + (rmid2 << 16)) & LMASK;                /* add mid2 to to */
if (rmid2 < rmid1)                                      /* carry? incr hi */
    rhi = rhi + 1;
r->hi = rhi & LMASK;                                    /* mask result */
r->lo = rmid2;
return;
}

static void dp_neg (UDP *r)
{
r->lo = NEG (r->lo);
r->hi = (~r->hi + (r->lo == 0)) & LMASK;
return;
}

/* Support routines */

static void unpackf (uint32_t hi, UFP *r)
{
r->sign = hi & FPSIGN;                                  /* get sign */
r->exp = FD_GETEXP (hi);                                /* get exponent */
if (r->exp == 0) {                                      /* exp = 0? */
    if (r->sign)                                        /* if -, rsvd op */
        RSVD_OPND_FAULT;
    r->frac.hi = r->frac.lo = 0;                        /* else 0 */
    return;
    }
r->frac.hi = WORDSWAP ((hi & ~(FPSIGN | FD_EXP)) | FD_HB);
r->frac.lo = 0;
dp_lsh (&r->frac, FD_GUARD);
return;
}

static void unpackd (uint32_t hi, uint32_t lo, UFP *r)
{
r->sign = hi & FPSIGN;                                  /* get sign */
r->exp = FD_GETEXP (hi);                                /* get exponent */
if (r->exp == 0) {                                      /* exp = 0? */
    if (r->sign)                                        /* if -, rsvd op */
        RSVD_OPND_FAULT;
    r->frac.hi = r->frac.lo = 0;                        /* else 0 */
    return;
      }
r->frac.hi = WORDSWAP ((hi & ~(FPSIGN | FD_EXP)) | FD_HB);
r->frac.lo = WORDSWAP (lo);
dp_lsh (&r->frac, FD_GUARD);
return;
}

static void unpackg (uint32_t hi, uint32_t lo, UFP *r)
{
r->sign = hi & FPSIGN;                                  /* get sign */
r->exp = G_GETEXP (hi);                                 /* get exponent */
if (r->exp == 0) {                                      /* exp = 0? */
    if (r->sign)                                        /* if -, rsvd op */
        RSVD_OPND_FAULT;
    r->frac.hi = r->frac.lo = 0;                        /* else 0 */
    return;
    }
r->frac.hi = WORDSWAP ((hi & ~(FPSIGN | G_EXP)) | G_HB);
r->frac.lo = WORDSWAP (lo);
dp_lsh (&r->frac, G_GUARD);
return;
}

static void norm (UFP *r)
{
int32_t i;
static uint32_t normmask[5] = {
 0xc0000000, 0xf0000000, 0xff000000, 0xffff0000, 0xffffffff
 };
static int32_t normtab[6] = { 1, 2, 4, 8, 16, 32};

if ((r->frac.hi == 0) && (r->frac.lo == 0)) {           /* if fraction = 0 */
    r->sign = r->exp = 0;                               /* result is 0 */
    return;
    }
while ((r->frac.hi & UF_NM_H) == 0) {                   /* normalized? */
    for (i = 0; i < 5; i++) {                           /* find first 1 */
        if (r->frac.hi & normmask[i])
            break;
        }
    dp_lsh (&r->frac, normtab[i]);                      /* shift frac */
    r->exp = r->exp - normtab[i];                       /* decr exp */
    }
return;
}

static int32_t rpackfd (UFP *r, int32_t *rh)
{
static UDP f_round = { UF_FRND_L, UF_FRND_H };
static UDP d_round = { UF_DRND_L, UF_DRND_H };

if (rh)                                                 /* assume 0 */
    *rh = 0;
if ((r->frac.hi == 0) && (r->frac.lo == 0))             /* result 0? */
    return 0;
if (rh)                                                 /* round */
    dp_add (&r->frac, &d_round);
else dp_add (&r->frac, &f_round);
if ((r->frac.hi & UF_NM_H) == 0) {                      /* carry out? */
    dp_rsh (&r->frac, 1);                               /* renormalize */
    r->exp = r->exp + 1;
    }
if (r->exp > (int32_t) FD_M_EXP)                        /* ovflo? fault */
    FLT_OVFL_FAULT;
if (r->exp <= 0) {                                      /* underflow? */
    if (PSL & PSW_FU)                                   /* fault if fu */
        FLT_UNFL_FAULT;
    return 0;                                           /* else 0 */
    }
dp_rsh (&r->frac, FD_GUARD);                            /* remove guard */
if (rh)                                                 /* get low */
    *rh = WORDSWAP (r->frac.lo);
return r->sign | (r->exp << FD_V_EXP) |
    (WORDSWAP (r->frac.hi) & ~(FD_HB | FPSIGN | FD_EXP));
}

static int32_t rpackg (UFP *r, int32_t *rh)
{
static UDP g_round = { UF_GRND_L, UF_GRND_H };

*rh = 0;                                                /* assume 0 */
if ((r->frac.hi == 0) && (r->frac.lo == 0))             /* result 0? */
    return 0;
dp_add (&r->frac, &g_round);                            /* round */
if ((r->frac.hi & UF_NM_H) == 0) {                      /* carry out? */
    dp_rsh (&r->frac, 1);                               /* renormalize */
    r->exp = r->exp + 1;
    }
if (r->exp > (int32_t) G_M_EXP)                         /* ovflo? fault */
FLT_OVFL_FAULT;
if (r->exp <= 0) {                                      /* underflow? */
    if (PSL & PSW_FU)                                   /* fault if fu */
        FLT_UNFL_FAULT;
    return 0;                                           /* else 0 */
    }
dp_rsh (&r->frac, G_GUARD);                             /* remove guard */
*rh = WORDSWAP (r->frac.lo);                            /* get low */
return r->sign | (r->exp << G_V_EXP) |
    (WORDSWAP (r->frac.hi) & ~(G_HB | FPSIGN | G_EXP));
}

#endif

/* Floating point instructions */

/* Move/test/move negated floating

   Note that only the high 32b is processed.
   If the high 32b is not zero, it is unchanged.
*/

int32_t op_movfd (int32_t val)
{
if (val & FD_EXP)
    return val;
if (val & FPSIGN)
    RSVD_OPND_FAULT(op_movfd);
return 0;
}

int32_t op_mnegfd (int32_t val)
{
if (val & FD_EXP)
    return (val ^ FPSIGN);
if (val & FPSIGN)
    RSVD_OPND_FAULT(op_mnegfd);
return 0;
}

int32_t op_movg (int32_t val)
{
if (val & G_EXP)
    return val;
if (val & FPSIGN)
    RSVD_OPND_FAULT(op_movg);
return 0;
}

int32_t op_mnegg (int32_t val)
{
if (val & G_EXP)
    return (val ^ FPSIGN);
if (val & FPSIGN)
    RSVD_OPND_FAULT(op_mnegg);
return 0;
}

/* Floating to floating convert - F to D is essentially done with MOVFD */

int32_t op_cvtdf (uint32_t *opnd)
{
UFP a;

unpackd (opnd[0], opnd[1], &a);
return rpackfd (&a, NULL);
}

int32_t op_cvtfg (uint32_t *opnd, int32_t *rh)
{
UFP a;

unpackf (opnd[0], &a);
a.exp = a.exp - FD_BIAS + G_BIAS;
return rpackg (&a, rh);
}

int32_t op_cvtgf (uint32_t *opnd)
{
UFP a;

unpackg (opnd[0], opnd[1], &a);
a.exp = a.exp - G_BIAS + FD_BIAS;
return rpackfd (&a, NULL);
}

/* Floating add and subtract */

int32_t op_addf (uint32_t *opnd, bool sub)
{
UFP a, b;

unpackf (opnd[0], &a);                                  /* F format */
unpackf (opnd[1], &b);
if (sub)                                                /* sub? -s1 */
    a.sign = a.sign ^ FPSIGN;
vax_fadd (&a, &b, 0, 0);                                /* add fractions */
return rpackfd (&a, NULL);
}

int32_t op_addd (uint32_t *opnd, int32_t *rh, bool sub)
{
UFP a, b;

unpackd (opnd[0], opnd[1], &a);
unpackd (opnd[2], opnd[3], &b);
if (sub)                                                /* sub? -s1 */
    a.sign = a.sign ^ FPSIGN;
vax_fadd (&a, &b, 0, 0);                                /* add fractions */
return rpackfd (&a, rh);
}

int32_t op_addg (uint32_t *opnd, int32_t *rh, bool sub)
{
UFP a, b;

unpackg (opnd[0], opnd[1], &a);
unpackg (opnd[2], opnd[3], &b);
if (sub)                                                /* sub? -s1 */
    a.sign = a.sign ^ FPSIGN;
vax_fadd (&a, &b, 0, 0);                                /* add fractions */
return rpackg (&a, rh);                                 /* round and pack */
}

/* Floating multiply */

int32_t op_mulf (uint32_t *opnd)
{
UFP a, b;

unpackf (opnd[0], &a);                                  /* F format */
unpackf (opnd[1], &b);
vax_fmul (&a, &b, 0, FD_BIAS, 0, 0);                    /* do multiply */
return rpackfd (&a, NULL);                              /* round and pack */
}

int32_t op_muld (uint32_t *opnd, int32_t *rh)
{
UFP a, b;

unpackd (opnd[0], opnd[1], &a);                         /* D format */
unpackd (opnd[2], opnd[3], &b);
vax_fmul (&a, &b, 1, FD_BIAS, 0, 0);                    /* do multiply */
return rpackfd (&a, rh);                                /* round and pack */
}

int32_t op_mulg (uint32_t *opnd, int32_t *rh)
{
UFP a, b;

unpackg (opnd[0], opnd[1], &a);                         /* G format */
unpackg (opnd[2], opnd[3], &b);
vax_fmul (&a, &b, 1, G_BIAS, 0, 0);                     /* do multiply */
return rpackg (&a, rh);                                 /* round and pack */
}

/* Floating divide */

int32_t op_divf (uint32_t *opnd)
{
UFP a, b;

unpackf (opnd[0], &a);                                  /* F format */
unpackf (opnd[1], &b);
vax_fdiv (&a, &b, 26, FD_BIAS);                         /* do divide */
return rpackfd (&b, NULL);                              /* round and pack */
}

int32_t op_divd (uint32_t *opnd, int32_t *rh)
{
UFP a, b;

unpackd (opnd[0], opnd[1], &a);                         /* D format */
unpackd (opnd[2], opnd[3], &b);
vax_fdiv (&a, &b, 58, FD_BIAS);                         /* do divide */
return rpackfd (&b, rh);                                /* round and pack */
}

int32_t op_divg (uint32_t *opnd, int32_t *rh)
{
UFP a, b;

unpackg (opnd[0], opnd[1], &a);                         /* G format */
unpackg (opnd[2], opnd[3], &b);
vax_fdiv (&a, &b, 55, G_BIAS);                          /* do divide */
return rpackg (&b, rh);                                 /* round and pack */
}

/* Polynomial evaluation
   The most mis-implemented instruction in the VAX (probably here too).
   POLY requires a precise combination of masking versus normalizing
   to achieve the desired answer.  In particular, the multiply step
   is masked prior to normalization.  In addition, negative small
   fractions must not be treated as 0 during denorm.
*/

void op_polyf (uint32_t *opnd, int32_t acc)
{
/* Shared instruction helper signature.
   This implementation does not use every parameter. */
(void) acc;

UFP r, a, c;
int32_t deg = opnd[1];
int32_t ptr = opnd[2];
int32_t i, wd, res;

if (deg > 31)                                           /* degree > 31? fault */
    RSVD_OPND_FAULT(op_polyf);
unpackf (opnd[0], &a);                                  /* unpack arg */
wd = Read (ptr, L_LONG, RD);                            /* get C0 */
ptr = ptr + 4;
unpackf (wd, &r);                                       /* unpack C0 */
res = rpackfd (&r, NULL);                               /* first result */
for (i = 0; i < deg; i++) {                             /* loop */
    unpackf (res, &r);                                  /* unpack result */
    vax_fmul (&r, &a, 0, FD_BIAS, 1, LMASK);            /* r = r * arg, mask */
    wd = Read (ptr, L_LONG, RD);                        /* get Cnext */
    ptr = ptr + 4;
    unpackf (wd, &c);                                   /* unpack Cnext */
    vax_fadd (&r, &c, 1, LMASK);                        /* r = r + Cnext */
    res = rpackfd (&r, NULL);                           /* round and pack */
    }
R[0] = res;
R[1] = R[2] = 0;
R[3] = ptr;
return;
}

void op_polyd (uint32_t *opnd, int32_t acc)
{
/* Shared instruction helper signature.
   This implementation does not use every parameter. */
(void) acc;

UFP r, a, c;
int32_t deg = opnd[2];
int32_t ptr = opnd[3];
int32_t i, wd, wd1, res, resh;

if (deg > 31)                                           /* degree > 31? fault */
    RSVD_OPND_FAULT(op_polyd);
unpackd (opnd[0], opnd[1], &a);                         /* unpack arg */
wd = Read (ptr, L_LONG, RD);                            /* get C0 */
wd1 = Read (ptr + 4, L_LONG, RD);
ptr = ptr + 8;
unpackd (wd, wd1, &r);                                  /* unpack C0 */
res = rpackfd (&r, &resh);                              /* first result */
for (i = 0; i < deg; i++) {                             /* loop */
    unpackd (res, resh, &r);                            /* unpack result */
    vax_fmul (&r, &a, 1, FD_BIAS, 0, 1);                /* r = r * arg, mask */
    wd = Read (ptr, L_LONG, RD);                        /* get Cnext */
    wd1 = Read (ptr + 4, L_LONG, RD);
    ptr = ptr + 8;
    unpackd (wd, wd1, &c);                              /* unpack Cnext */
    vax_fadd (&r, &c, 0, 1);                            /* r = r + Cnext */
    res = rpackfd (&r, &resh);                          /* round and pack */
    }
R[0] = res;
R[1] = resh;
R[2] = 0;
R[3] = ptr;
R[4] = 0;
R[5] = 0;
return;
}

void op_polyg (uint32_t *opnd, int32_t acc)
{
/* Shared instruction helper signature.
   This implementation does not use every parameter. */
(void) acc;

UFP r, a, c;
int32_t deg = opnd[2];
int32_t ptr = opnd[3];
int32_t i, wd, wd1, res, resh;

if (deg > 31)                                           /* degree > 31? fault */
    RSVD_OPND_FAULT(op_polyg);
unpackg (opnd[0], opnd[1], &a);                         /* unpack arg */
wd = Read (ptr, L_LONG, RD);                            /* get C0 */
wd1 = Read (ptr + 4, L_LONG, RD);
ptr = ptr + 8;
unpackg (wd, wd1, &r);                                  /* unpack C0 */
res = rpackg (&r, &resh);                               /* first result */
for (i = 0; i < deg; i++) {                             /* loop */
    unpackg (res, resh, &r);                            /* unpack result */
    vax_fmul (&r, &a, 1, G_BIAS, 0, 1);                 /* r = r * arg */
    wd = Read (ptr, L_LONG, RD);                        /* get Cnext */
    wd1 = Read (ptr + 4, L_LONG, RD);
    ptr = ptr + 8;
    unpackg (wd, wd1, &c);                              /* unpack Cnext */
    vax_fadd (&r, &c, 0, 1);                            /* r = r + Cnext */
    res = rpackg (&r, &resh);                           /* round and pack */
    }
R[0] = res;
R[1] = resh;
R[2] = 0;
R[3] = ptr;
R[4] = 0;
R[5] = 0;
return;
}
