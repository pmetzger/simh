/* vax_qbus_internal.h: internal VAX Qbus helpers */
// SPDX-FileCopyrightText: 1998-2019 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_QBUS_INTERNAL_H_
#define VAX_QBUS_INTERNAL_H_ 0

#include "vax_defs.h"

/* Return the bit shift for the byte lane selected by a VAX address. */
static inline uint32 vax_qbus_byte_shift(uint32 pa)
{
    return (pa & 3u) << 3;
}

/* Return the bit shift for the word lane selected by a VAX address. */
static inline uint32 vax_qbus_word_shift(uint32 pa)
{
    return (pa & 2u) << 3;
}

/* Replace one byte lane in a 32-bit VAX memory word. */
static inline uint32 vax_qbus_replace_byte(uint32 word, uint32 pa, uint32 val)
{
    uint32 sc = vax_qbus_byte_shift(pa);
    uint32 byte_mask = (uint32)BMASK;
    uint32 lane_mask = byte_mask << sc;

    return ((val & byte_mask) << sc) | (word & ~lane_mask);
}

/* Replace one 16-bit lane in a 32-bit VAX memory word. */
static inline uint32 vax_qbus_replace_word(uint32 word, uint32 pa, uint32 val)
{
    uint32 sc = vax_qbus_word_shift(pa);
    uint32 word_mask = (uint32)WMASK;
    uint32 lane_mask = word_mask << sc;

    return ((val & word_mask) << sc) | (word & ~lane_mask);
}

/* Position a 16-bit Qbus read result in the selected VAX longword lane. */
static inline uint32 vax_qbus_position_read_word(uint32 pa, uint32 val)
{
    return (val & (uint32)WMASK) << vax_qbus_word_shift(pa);
}

/* Combine two 16-bit Qbus read results into one VAX longword result. */
static inline uint32 vax_qbus_combine_read_words(uint32 low, uint32 high)
{
    uint32 word_mask = (uint32)WMASK;

    return ((high & word_mask) << 16) | (low & word_mask);
}

#endif
