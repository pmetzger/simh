/* vax750_mem_internal.h: internal VAX-750 memory controller helpers */
// SPDX-FileCopyrightText: 2010-2012 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX750_MEM_INTERNAL_H_
#define VAX750_MEM_INTERNAL_H_ 1

#include <stdint.h>

#include "vax_defs.h"

#define MCSR2_OF 0x02
#define MCSR2_M_MAP 0xFFFF    /* Memory present */
#define MCSR2_INIT 0x00010000 /* Cold/warm restart flag */
#define MCSR2_V_SA 17
#define MCSR2_M_SA 0x7F /* Start address */
#define MCSR2_V_CS64 24
#define MCSR2_CS64 (1u << MCSR2_V_CS64) /* Chip size */
#define MCSR2_V_CS256 25
#define MCSR2_CS256 (1u << MCSR2_V_CS256) /* Chip size */
#define MCSR2_MBZ 0xFC000000

#define MEM_SIZE_16K (1u << 18)  /* Board size (16k chips) */
#define MEM_SIZE_64K (1u << 20)  /* Board size (64k chips) */
#define MEM_SIZE_256K (1u << 22) /* Board size (256k chips) */

/*
 * Build the MCSR2 memory-present and chip-size bits for the configured
 * VAX-750 memory size.
 */
static inline uint32_t vax750_mcsr2_reset_value(uint32_t memsize)
{
    uint32_t large_slot_size = MEM_SIZE_16K;
    uint32_t large_slots;
    uint32_t small_slot_size;
    uint32_t small_slots;
    uint32_t boards;
    uint32_t board_mask;
    uint32_t large_slot_bits;

    if (memsize > MAXMEMSIZE_Y)
        large_slot_size = MEM_SIZE_256K;
    else if (memsize > MAXMEMSIZE)
        large_slot_size = MEM_SIZE_64K;

    small_slot_size = large_slot_size >> 2;
    large_slots = memsize / large_slot_size;
    small_slots = (memsize & (large_slot_size - 1)) / small_slot_size;
    large_slot_bits = large_slots << 1;
    boards = (1u << ((large_slots + small_slots) << 1)) - 1;
    board_mask = (((large_slot_size == MEM_SIZE_16K) ? 0xFFFFu : 0x5555u) &
                  ((1u << large_slot_bits) - 1)) |
                 (((large_slot_size == MEM_SIZE_256K) ? 0xAAAAu : 0xFFFFu)
                  << large_slot_bits);

    return MCSR2_INIT | (boards & board_mask) |
           ((large_slot_size == MEM_SIZE_256K) ? MCSR2_CS256 : 0);
}

#endif
