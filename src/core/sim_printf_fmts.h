/* sim_printf_fmts.h: printf() format specifiers for ZIMH domain types */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_PRINTF_FMTS_H
#define SIM_PRINTF_FMTS_H

#include <inttypes.h>

/*
 * Preferred printf() conversion specifiers for ZIMH domain type widths.
 * These are complete conversion suffixes, following the <inttypes.h>
 * PRI* convention.  Use them as "%" PRIuADDR, not as modifier fragments.
 * Choose the signedness that matches the actual argument type after any
 * deliberate cast.
 */
#if defined (USE_INT64) && defined (USE_ADDR64)
#  define PRIdADDR PRId64
#  define PRIiADDR PRIi64
#  define PRIuADDR PRIu64
#  define PRIoADDR PRIo64
#  define PRIxADDR PRIx64
#  define PRIXADDR PRIX64
#else
#  define PRIdADDR PRId32
#  define PRIiADDR PRIi32
#  define PRIuADDR PRIu32
#  define PRIoADDR PRIo32
#  define PRIxADDR PRIx32
#  define PRIXADDR PRIX32
#endif

#if defined (USE_INT64)
#  define PRIdVALUE  PRId64
#  define PRIiVALUE  PRIi64
#  define PRIuVALUE  PRIu64
#  define PRIoVALUE  PRIo64
#  define PRIxVALUE  PRIx64
#  define PRIXVALUE  PRIX64
#  define PRIdSVALUE PRId64
#  define PRIiSVALUE PRIi64
#  define PRIuSVALUE PRIu64
#  define PRIoSVALUE PRIo64
#  define PRIxSVALUE PRIx64
#  define PRIXSVALUE PRIX64
#else
#  define PRIdVALUE  PRId32
#  define PRIiVALUE  PRIi32
#  define PRIuVALUE  PRIu32
#  define PRIoVALUE  PRIo32
#  define PRIxVALUE  PRIx32
#  define PRIXVALUE  PRIX32
#  define PRIdSVALUE PRId32
#  define PRIiSVALUE PRIi32
#  define PRIuSVALUE PRIu32
#  define PRIoSVALUE PRIo32
#  define PRIxSVALUE PRIx32
#  define PRIXSVALUE PRIX32
#endif

#endif /* SIM_PRINTF_FMTS_H */
