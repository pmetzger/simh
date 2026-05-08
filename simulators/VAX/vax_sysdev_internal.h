/* vax_sysdev_internal.h: internal VAX 3900 system device helpers */
// SPDX-FileCopyrightText: 1998-2019 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_SYSDEV_INTERNAL_H_
#define VAX_SYSDEV_INTERNAL_H_ 0

#include "vax_defs.h"

/* Return the bit shift for the byte lane selected by a VAX address. */
static inline uint32 vax_sysdev_lane_shift(uint32 pa)
{
    return (pa & 3u) << 3;
}

/*
 * Shift a right-justified register write value into the lane selected by a VAX
 * address.
 */
static inline uint32 vax_sysdev_shift_lane_value(uint32 val, uint32 pa)
{
    return val << vax_sysdev_lane_shift(pa);
}

/* Shift a byte value into one byte lane of a 32-bit VAX word. */
static inline uint32 vax_sysdev_pack_byte_lane(uint32 val, uint32 shift)
{
    return (val & 0xFFu) << shift;
}

/*
 * Replace a masked lane in a 32-bit word while preserving the other lanes.
 * The low two address bits select the lane shift.
 */
static inline uint32 vax_sysdev_replace_masked_lane(uint32 word, uint32 pa,
                                                    uint32 val, uint32 mask)
{
    uint32 sc = vax_sysdev_lane_shift(pa);
    uint32 lane_mask = mask << sc;

    return ((val & mask) << sc) | (word & ~lane_mask);
}

/*
 * Replace one byte lane in a 32-bit word while preserving the other lanes.
 * The low two address bits select the byte lane.
 */
static inline uint32 vax_sysdev_replace_byte_lane(uint32 word, uint32 pa,
                                                  uint32 val)
{
    uint32 sc = vax_sysdev_lane_shift(pa);
    uint32 mask = vax_sysdev_pack_byte_lane(0xFFu, sc);

    return vax_sysdev_pack_byte_lane(val, sc) | (word & ~mask);
}

#endif
