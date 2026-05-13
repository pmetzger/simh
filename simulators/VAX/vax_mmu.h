/* vax_mmu.h - VAX memory management (inlined)

   Copyright (c) 1998-2013, Robert M Supnik

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

   29-Nov-13    RMS     Reworked unaligned flows
   24-Oct-12    MB      Added support for KA620 virtual addressing
   21-Jul-08    RMS     Removed inlining support
   28-May-08    RMS     Inlined physical memory routines
   29-Apr-07    RMS     Added address masking for system page table reads
   22-Sep-05    RMS     Fixed declarations (Sterling Garwood)
   30-Sep-04    RMS     Comment and formating changes
   19-Sep-03    RMS     Fixed upper/lower case linkage problems on VMS
   01-Jun-03    RMS     Fixed compilation problem with USE_ADDR64

   This module contains the instruction simulators for

        Read            -       read virtual
        Write           -       write virtual
        ReadL(P)        -       read aligned physical longword (physical context)
        WriteL(P)       -       write aligned physical longword (physical context)
        ReadB(W)        -       read aligned physical byte (word)
        WriteB(W)       -       write aligned physical byte (word)
        Test            -       test acccess

*/

#ifndef VAX_MMU_H_
#define VAX_MMU_H_ 1

#include "vax_defs.h"
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t    tag;                                    /* tag */
    uint32_t    pte;                                    /* pte */
    } TLBENT;

extern uint32_t *M;
extern UNIT cpu_unit;
extern DEVICE cpu_dev;
extern int32_t mapen;                                   /* map enable */

extern uint32_t mchk_va, mchk_ref;                      /* for mcheck */
extern TLBENT stlb[VA_TBSIZE], ptlb[VA_TBSIZE];

static const uint32_t insert[4] = {
    0x00000000, 0x000000FF, 0x0000FFFF, 0x00FFFFFF
    };

void zap_tb (int stb);
void zap_tb_ent (uint32_t va);
bool chk_tb_ent (uint32_t va);
void set_map_reg (void);
int32_t ReadIO (uint32_t pa, int32_t lnt);
void WriteIO (uint32_t pa, int32_t val, int32_t lnt);
int32_t ReadReg (uint32_t pa, int32_t lnt);
void WriteReg (uint32_t pa, int32_t val, int32_t lnt);
TLBENT fill (uint32_t va, int32_t lnt, int32_t acc, int32_t *stat);
static inline int32_t ReadU (uint32_t pa, int32_t lnt);
static inline void WriteU (uint32_t pa, int32_t val, int32_t lnt);
static inline int32_t ReadB (uint32_t pa);
static inline int32_t ReadW (uint32_t pa);
static inline int32_t ReadL (uint32_t pa);
static inline int32_t ReadLP (uint32_t pa);
static inline void WriteB (uint32_t pa, int32_t val);
static inline void WriteW (uint32_t pa, int32_t val);
static inline void WriteL (uint32_t pa, int32_t val);

/* Read and write virtual

   These routines logically fall into three phases:

   1.   Look up the virtual address in the translation buffer, calling
        the fill routine on a tag mismatch or access mismatch (invalid
        tlb entries have access = 0 and thus always mismatch).  The
        fill routine handles all errors.  If the resulting physical
        address is aligned, do an aligned physical read or write.
   2.   Test for unaligned across page boundaries.  If cross page, look
        up the physical address of the second page.  If not cross page,
        the second physical address is the same as the first.
   3.   Using the two physical addresses, do an unaligned read or
        write, with three cases: unaligned long, unaligned word within
        a longword, unaligned word crossing a longword boundary.

   Note that these routines do not handle quad or octa references.
*/

/* Read virtual

   Inputs:
        va      =       virtual address
        lnt     =       length code (BWL)
        acc     =       access code (KESU)
   Output:
        returned data, right justified in 32b longword
*/

static inline int32_t Read (uint32_t va, int32_t lnt, int32_t acc)
{
uint32_t vpn, off, tbi, pa;
uint32_t pa1, bo, sc, wl, wh;
TLBENT xpte;

mchk_va = va;
if (mapen) {                                            /* mapping on? */
    vpn = VA_GETVPN (va);                               /* get vpn, offset */
    off = VA_GETOFF (va);
    tbi = VA_GETTBI (vpn);
    xpte = (va & VA_S0)? stlb[tbi]: ptlb[tbi];          /* access tlb */
    if (((xpte.pte & acc) == 0) || (xpte.tag != vpn) ||
        ((acc & TLB_WACC) && ((xpte.pte & TLB_M) == 0)))
        xpte = fill (va, lnt, acc, NULL);               /* fill if needed */
    pa = (xpte.pte & TLB_PFN) | off;                    /* get phys addr */
    }
else {
    pa = va & PAMASK;
    off = 0;
    }
if ((pa & (lnt - 1)) == 0) {                            /* aligned? */
    if (lnt >= L_LONG)                                  /* long, quad? */
        return ReadL (pa);
    if (lnt == L_WORD)                                  /* word? */
        return ReadW (pa);
    return ReadB (pa);                                  /* byte */
    }
if (mapen && ((uint32_t)(off + lnt) > VA_PAGSIZE)) {    /* cross page? */
    vpn = VA_GETVPN (va + lnt);                         /* vpn 2nd page */
    tbi = VA_GETTBI (vpn);
    xpte = (va & VA_S0)? stlb[tbi]: ptlb[tbi];          /* access tlb */
    if (((xpte.pte & acc) == 0) || (xpte.tag != vpn) ||
        ((acc & TLB_WACC) && ((xpte.pte & TLB_M) == 0)))
        xpte = fill (va + lnt, lnt, acc, NULL);         /* fill if needed */
    pa1 = ((xpte.pte & TLB_PFN) | VA_GETOFF (va + 4)) & ~03;
    }
else
    pa1 = ((pa + 4) & PAMASK) & ~03;                   /* not cross page */
bo = pa & 3;
if (lnt >= L_LONG) {                                    /* lw unaligned? */
    sc = bo << 3;
    wl = ReadU (pa, L_LONG - bo);                       /* read both fragments */
    wh = ReadU (pa1, bo);                               /* extract */
    return ((wl | (wh << (32 - sc))) & LMASK);
    }
else if (bo == 1)                                       /* read within lw */
    return ReadU (pa, L_WORD);
else {
    wl = ReadU (pa, L_BYTE);                            /* word cross lw */
    wh = ReadU (pa1, L_BYTE);                           /* read, extract */
    return (wl | (wh << 8));
    }
}

/* Write virtual

   Inputs:
        va      =       virtual address
        val     =       data to be written, right justified in 32b lw
        lnt     =       length code (BWL)
        acc     =       access code (KESU)
   Output:
        none
*/

static inline void Write (uint32_t va, int32_t val, int32_t lnt, int32_t acc)
{
uint32_t vpn, off, tbi, pa;
uint32_t pa1, bo, sc;
TLBENT xpte;

mchk_va = va;
if (mapen) {
    vpn = VA_GETVPN (va);
    off = VA_GETOFF (va);
    tbi = VA_GETTBI (vpn);
    xpte = (va & VA_S0)? stlb[tbi]: ptlb[tbi];          /* access tlb */
    if (((xpte.pte & acc) == 0) || (xpte.tag != vpn) ||
        ((xpte.pte & TLB_M) == 0))
        xpte = fill (va, lnt, acc, NULL);
    pa = (xpte.pte & TLB_PFN) | off;
    }
else {
    pa = va & PAMASK;
    off = 0;
    }
if ((pa & (lnt - 1)) == 0) {                            /* aligned? */
    if (lnt >= L_LONG)                                  /* long, quad? */
        WriteL (pa, val);
    else {
        if (lnt == L_WORD)                             /* word? */
            WriteW (pa, val);
        else
            WriteB (pa, val);                              /* byte */
        }
    return;
    }
if (mapen && ((uint32_t)(off + lnt) > VA_PAGSIZE)) {
    vpn = VA_GETVPN (va + 4);
    tbi = VA_GETTBI (vpn);
    xpte = (va & VA_S0)? stlb[tbi]: ptlb[tbi];          /* access tlb */
    if (((xpte.pte & acc) == 0) || (xpte.tag != vpn) ||
        ((xpte.pte & TLB_M) == 0))
        xpte = fill (va + lnt, lnt, acc, NULL);
    pa1 = ((xpte.pte & TLB_PFN) | VA_GETOFF (va + 4)) & ~03;
    }
else
    pa1 = ((pa + 4) & PAMASK) & ~03;
bo = pa & 3;
if (lnt >= L_LONG) {
    sc = bo << 3;
    WriteU (pa, val & insert[L_LONG - bo], L_LONG - bo);
    WriteU (pa1, (val >> (32 - sc)) & insert[bo], bo);
    }
else if (bo == 1)                                       /* read within lw */
    WriteU (pa, val & WMASK, L_WORD);
else {                                                  /* word cross lw */
    WriteU (pa, val & BMASK, L_BYTE);
    WriteU (pa1, (val >> 8) & BMASK, L_BYTE);
    }
return;
}

/* Test access to a byte (VAX PROBEx) */

static inline int32_t Test (uint32_t va, int32_t acc, int32_t *status)
{
uint32_t vpn, off, tbi;
TLBENT xpte;

*status = PR_OK;                                        /* assume ok */
if (mapen) {                                            /* mapping on? */
    vpn = VA_GETVPN (va);                               /* get vpn, off */
    off = VA_GETOFF (va);
    tbi = VA_GETTBI (vpn);
    xpte = (va & VA_S0)? stlb[tbi]: ptlb[tbi];          /* access tlb */
    if ((xpte.pte & acc) && (xpte.tag == vpn))          /* TB hit, acc ok? */
        return (xpte.pte & TLB_PFN) | off;
    xpte = fill (va, L_BYTE, acc, status);              /* fill TB */
    if (*status == PR_OK)
        return (xpte.pte & TLB_PFN) | off;
    else
        return -1;
    }
return va & PAMASK;                                     /* ret phys addr */
}

/* Read aligned physical (in virtual context, unless indicated)

   Inputs:
        pa      =       physical address, naturally aligned
   Output:
        returned data, right justified in 32b longword
*/

static inline int32_t ReadB (uint32_t pa)
{
int32_t dat;

if (ADDR_IS_MEM (pa))
    dat = M[pa >> 2];
else {
    mchk_ref = REF_V;
    if (ADDR_IS_IO (pa))
        dat = ReadIO (pa, L_BYTE);
    else
        dat = ReadReg (pa, L_BYTE);
    }
return ((dat >> ((pa & 3) << 3)) & BMASK);
}

static inline int32_t ReadW (uint32_t pa)
{
int32_t dat;

if (ADDR_IS_MEM (pa))
    dat = M[pa >> 2];
else {
    mchk_ref = REF_V;
    if (ADDR_IS_IO (pa))
        dat = ReadIO (pa, L_WORD);
    else
        dat = ReadReg (pa, L_WORD);
    }
return ((dat >> ((pa & 2)? 16: 0)) & WMASK);
}

static inline int32_t ReadL (uint32_t pa)
{
if (ADDR_IS_MEM (pa))
    return M[pa >> 2];
mchk_ref = REF_V;
if (ADDR_IS_IO (pa))
    return ReadIO (pa, L_LONG);
return ReadReg (pa, L_LONG);
}

static inline int32_t ReadLP (uint32_t pa)
{
if (ADDR_IS_MEM (pa))
    return M[pa >> 2];
mchk_va = pa;
mchk_ref = REF_P;
if (ADDR_IS_IO (pa))
    return ReadIO (pa, L_LONG);
return ReadReg (pa, L_LONG);
}

/* Read unaligned physical (in virtual context)

   Inputs:
        pa      =       physical address
        lnt     =       length in bytes (1, 2, or 3)
   Output:
        returned data
*/

static inline int32_t ReadU (uint32_t pa, int32_t lnt)
{
int32_t dat;
int32_t sc = (pa & 3) << 3;
if (ADDR_IS_MEM (pa))
    dat = M[pa >> 2];
else {
    mchk_ref = REF_V;
    if (ADDR_IS_IO (pa))
        dat = ReadIOU (pa, lnt);
    else
        dat = ReadRegU (pa, lnt);
    }
return ((dat >> sc) & insert[lnt]);
}

/* Write aligned physical (in virtual context, unless indicated)

   Inputs:
        pa      =       physical address, naturally aligned
        val     =       data to be written, right justified in 32b longword
   Output:
        none
*/

static inline void WriteB (uint32_t pa, int32_t val)
{
if (ADDR_IS_MEM (pa)) {
    uint32_t id = pa >> 2;
    uint32_t sc = (pa & 3) << 3;
    uint32_t mask = 0xFFu << sc;
    M[id] = (M[id] & ~mask) | (((uint32_t) val << sc) & mask);
    }
else {
    mchk_ref = REF_V;
    if (ADDR_IS_IO (pa))
        WriteIO (pa, val, L_BYTE);
    else
        WriteReg (pa, val, L_BYTE);
    }
return;
}

static inline void WriteW (uint32_t pa, int32_t val)
{
if (ADDR_IS_MEM (pa)) {
    uint32_t id = pa >> 2;
    uint32_t uval = (uint32_t) val;
    M[id] = (pa & 2)? (M[id] & 0xFFFFu) | ((uval & WMASK) << 16):
        (M[id] & ~0xFFFFu) | (uval & WMASK);
    }
else {
    mchk_ref = REF_V;
    if (ADDR_IS_IO (pa))
        WriteIO (pa, val, L_WORD);
    else
        WriteReg (pa, val, L_WORD);
    }
return;
}

static inline void WriteL (uint32_t pa, int32_t val)
{
if (ADDR_IS_MEM (pa))
    M[pa >> 2] = val;
else {
    mchk_ref = REF_V;
    if (ADDR_IS_IO (pa))
        WriteIO (pa, val, L_LONG);
    else
        WriteReg (pa, val, L_LONG);
    }
return;
}

static inline void WriteLP (uint32_t pa, int32_t val)
{
if (ADDR_IS_MEM (pa))
    M[pa >> 2] = val;
else {
    mchk_va = pa;
    mchk_ref = REF_P;
    if (ADDR_IS_IO (pa))
        WriteIO (pa, val, L_LONG);
    else
        WriteReg (pa, val, L_LONG);
    }
return;
}

/* Write unaligned physical (in virtual context)

   Inputs:
        pa      =       physical address
        val     =       data to be written, right justified in 32b longword
        lnt     =       length (1, 2, or 3 bytes)
   Output:
        none
*/

static inline void WriteU (uint32_t pa, int32_t val, int32_t lnt)
{
if (ADDR_IS_MEM (pa)) {
    uint32_t bo = pa & 3;
    uint32_t sc = bo << 3;
    uint32_t mask = insert[lnt] << sc;
    M[pa >> 2] = (M[pa >> 2] & ~mask) |
        ((((uint32_t) val) & insert[lnt]) << sc);
    }
else {
    mchk_ref = REF_V;
    if ADDR_IS_IO (pa)
        WriteIOU (pa, val, lnt);
    else
        WriteRegU (pa, val, lnt);
    }
return;
}

#endif /* VAX_MMU_H_ */
