/* 3b2_scsi_internal.h: Private helpers for the 3B2 SCSI controller */

#ifndef _3B2_SCSI_INTERNAL_H_
#define _3B2_SCSI_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>

#include "3b2_scsi.h"

/*
 * Translate a 3B2 SCSI host-adapter subdevice number into a SCSI target
 * number. The host adapter keeps a compact table of attached devices; table
 * entries set to -1 mean that the requested subdevice does not exist.
 */
static inline bool ha_subdev_target(const int8_t subdev_tab[8],
                                      uint8_t subdev, uint8_t *target)
{
    int8_t mapped = subdev_tab[subdev & 7];

    if (mapped < 0) {
        return false;
    }

    *target = (uint8_t)mapped;
    return true;
}

/*
 * Resolve the target used by old-style host-adapter commands whose command
 * code names a host-adapter subdevice rather than a raw SCSI target. These
 * commands must reject missing subdevices before the dispatcher indexes the
 * per-target state table.
 */
static inline bool ha_subdev_command_target(const int8_t subdev_tab[8],
                                              uint8_t op, uint8_t subdev,
                                              uint8_t *target)
{
    switch (op) {
    case HA_BOOT:
    case HA_READ_BLK:
    case HA_WRITE_BLK:
        return ha_subdev_target(subdev_tab, subdev, target);
    default:
        return false;
    }
}

#endif /* _3B2_SCSI_INTERNAL_H_ */
