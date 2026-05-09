/* ibm1130_bool_internal.h: IBM 1130 boolean conversion helpers */

#ifndef IBM1130_BOOL_INTERNAL_H_
#define IBM1130_BOOL_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>

#include "sim_defs.h"

static inline bool ibm1130_mask_is_set(uint32_t value, uint32_t mask)
{
    return (value & mask) != 0;
}

static inline bool ibm1130_switch_requested(int32_t switches, int32_t mask)
{
    return ibm1130_mask_is_set((uint32_t)switches, (uint32_t)mask);
}

static inline bool ibm1130_any_state_mask_is_set(uint32_t state0, uint32_t state1,
                                                 uint32_t state2, uint32_t mask)
{
    return ibm1130_mask_is_set(state0 | state1 | state2, mask);
}

#endif /* IBM1130_BOOL_INTERNAL_H_ */
