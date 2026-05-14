/* sel32_chan_internal.h: internal SEL-32 channel helpers */

#ifndef SEL32_CHAN_INTERNAL_H_
#define SEL32_CHAN_INTERNAL_H_ 1

#include <stdint.h>

#include "sel32_defs.h"

/* Return the optional per-unit IOCL queue for a device. */
static inline IOCLQ *sel32_ioclq_for_unit(DIB *dibp, int unit)
{
    if ((dibp == NULL) || (dibp->ioclq_ptr == NULL))
        return NULL;
    if ((unit < 0) || ((uint32_t)unit >= dibp->numunits))
        return NULL;
    return &dibp->ioclq_ptr[unit];
}

#endif
