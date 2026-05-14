/* scp_size.h: shared SCP storage sizing helpers

   SCP stores device and register values in host integer types that are
   wide enough for the simulated bit width.  These helpers centralize
   that width-to-storage policy so SCP save/restore, buffered unit I/O,
   and tests all use the same rule.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#ifndef SIM_SCP_SIZE_H_
#define SIM_SCP_SIZE_H_ 1

#include <stdint.h>

#include "sim_defs.h"

/* Return the host storage size used for a width measured in bytes. */
static inline size_t scp_storage_size_for_width_bytes(size_t width_bytes)
{
    if (width_bytes <= 1)
        return sizeof(int8_t);
    if (width_bytes == 2)
        return sizeof(int16_t);
    if (width_bytes <= 4)
        return sizeof(int32_t);
#if defined(USE_INT64)
    if (width_bytes <= sizeof(int64_t))
        return sizeof(int64_t);
#endif
    return 0;
}

/* Return the host storage size used for one device datum. */
static inline size_t scp_device_data_size_bytes(const DEVICE *dptr)
{
    size_t width_bytes = (dptr->dwidth + CHAR_BIT - 1) / CHAR_BIT;

    return scp_storage_size_for_width_bytes(width_bytes);
}

/* Return the host storage size used for one register element. */
static inline size_t scp_register_data_size_bytes(const REG *rptr)
{
    size_t width_bytes = (rptr->width + rptr->offset + CHAR_BIT - 1) / CHAR_BIT;

    return scp_storage_size_for_width_bytes(width_bytes);
}

#endif
