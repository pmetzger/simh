/* pdp18b_cpu_internal.h: internal helpers for 18-bit PDP CPU tests */

#ifndef PDP18B_CPU_INTERNAL_H_
#define PDP18B_CPU_INTERNAL_H_ 0

#include <stdint.h>

#include "pdp18b_defs.h"
#include "sim_types.h"

enum { PDP18B_LAC_WIDTH = 19 };

/* Rotate a PDP-18B link-plus-AC bit pattern within its 19-bit field. */
static inline uint32_t
pdp18b_lac_rotate_left(uint32_t lac, uint_t count)
{
    uint32_t value = lac & LACMASK;

    count %= PDP18B_LAC_WIDTH;
    if (count == 0)
        return value;
    return ((value << count) | (value >> (PDP18B_LAC_WIDTH - count))) &
           LACMASK;
}

/* Rotate a PDP-18B link-plus-AC bit pattern within its 19-bit field. */
static inline uint32_t
pdp18b_lac_rotate_right(uint32_t lac, uint_t count)
{
    uint32_t value = lac & LACMASK;

    count %= PDP18B_LAC_WIDTH;
    if (count == 0)
        return value;
    return ((value >> count) | (value << (PDP18B_LAC_WIDTH - count))) &
           LACMASK;
}

#endif
