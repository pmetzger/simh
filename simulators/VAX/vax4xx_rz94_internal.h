/* vax4xx_rz94_internal.h: NCR 53C94 SCSI controller test seams */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX4XX_RZ94_INTERNAL_H_
#define VAX4XX_RZ94_INTERNAL_H_ 1

#include <stdint.h>

#include "sim_defs.h"
#include "sim_scsi.h"

extern DEVICE rz_dev;
extern UNIT rz_unit[];
extern SCSI_BUS rz_bus;
extern uint8_t rz_cfg1;
extern uint8_t rz_cfg2;
extern uint8_t rz_cfg3;
extern uint8_t rz_int;
extern uint8_t rz_stat;
extern uint8_t *rz_buf;
extern uint8_t rz_fifo[];
extern uint32_t rz_dest;
extern uint32_t rz_fifo_b;
extern uint32_t rz_fifo_c;
extern uint32_t rz_fifo_t;
extern uint32_t rz_seq;
extern uint32_t rz_txc;

t_stat rz_reset(DEVICE *dptr);
void rz_cmd(uint32_t cmd);

#endif /* VAX4XX_RZ94_INTERNAL_H_ */
