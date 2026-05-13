/* vax_cpu1_internal.h: internal VAX complex-instruction helpers */
// SPDX-FileCopyrightText: 1998-2017 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_CPU1_INTERNAL_H_
#define VAX_CPU1_INTERNAL_H_ 1

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"

#define CALL_DV 0x8000   /* DV set */
#define CALL_IV 0x4000   /* IV set */
#define CALL_MBZ 0x3000  /* MBZ */
#define CALL_MASK 0x0FFF /* mask */
#define CALL_V_SPA 30    /* SPA */
#define CALL_M_SPA 03
#define CALL_V_S 29 /* S flag */
#define CALL_S (1 << CALL_V_S)
#define CALL_V_MASK 16
#define CALL_GETSPA(x) (((x) >> CALL_V_SPA) & CALL_M_SPA)

/* Build the saved stack-frame word for CALLG/CALLS. */
static inline int32_t vax_call_frame_word(uint32_t sp, bool gs, int32_t mask,
                                        uint32_t psl)
{
    uint32_t word;

    word =
        ((sp & (uint32_t)CALL_M_SPA) << CALL_V_SPA) | (gs ? (uint32_t)CALL_S : 0u) |
        (((uint32_t)mask & (uint32_t)CALL_MASK) << CALL_V_MASK) | (psl & 0xFFE0u);
    return (int32_t)word;
}

#endif
