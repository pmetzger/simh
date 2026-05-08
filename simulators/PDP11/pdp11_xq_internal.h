/* pdp11_xq_internal.h: internal DEQNA/DELQA helpers */
// SPDX-FileCopyrightText: 1993-2008 Robert M Supnik
// SPDX-FileCopyrightText: 2001-2008 Mark Pizzolato
// SPDX-FileCopyrightText: 2001-2004 Dan Haygood
// SPDX-License-Identifier: X11

#ifndef PDP11_XQ_INTERNAL_H_
#define PDP11_XQ_INTERNAL_H_ 0

#include "pdp11_xq.h"

/* Replace the low 16 bits of the DELQA-PLUS init block address register. */
static inline uint32 xq_replace_iba_low(uint32 iba, uint16 data)
{
    uint32 data_word = data;

    return (iba & 0xFFFF0000u) | (data_word & 0xFFFFu);
}

/* Replace the high 16 bits of the DELQA-PLUS init block address register. */
static inline uint32 xq_replace_iba_high(uint32 iba, uint16 data)
{
    uint32 data_word = data;

    return (iba & 0xFFFFu) | ((data_word & 0xFFFFu) << 16);
}

#endif
