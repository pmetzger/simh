/* vax4xx_stddev_internal.h: internal KA4xx standard device helpers */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-FileCopyrightText: 1998-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX4XX_STDDEV_INTERNAL_H_
#define VAX4XX_STDDEV_INTERNAL_H_ 0

#include "vax_defs.h"

/* Shift a byte value into one byte lane of a 32-bit VAX word. */
static inline uint32 vax4xx_pack_byte_lane(uint32 val, uint32 shift)
{
    return (val & 0xFFu) << shift;
}

/*
 * Replace one byte lane in a 32-bit word while preserving the other lanes.
 * The low two address bits select the byte lane.
 */
static inline uint32 vax4xx_replace_byte_lane(uint32 word, uint32 pa,
                                              uint32 val)
{
    uint32 sc = (pa & 3u) << 3;
    uint32 mask = vax4xx_pack_byte_lane(0xFFu, sc);

    return vax4xx_pack_byte_lane(val, sc) | (word & ~mask);
}

#endif
