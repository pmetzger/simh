/* vax_qbus_internal.h: internal VAX Qbus helpers */
// SPDX-FileCopyrightText: 1998-2019 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_QBUS_INTERNAL_H_
#define VAX_QBUS_INTERNAL_H_ 1

#include <stdint.h>

#include "vax_defs.h"

/* Internal mapped Qbus memory test seams. */
extern int32_t qb_map[];
extern int32_t cq_mbr;
t_stat qbmem_wr(int32_t dat, int32_t pa, int32_t md);
t_stat cqm_wr(int32_t dat, int32_t pa, int32_t md);

#ifdef VAX_QBUS_TEST_RECORD_READS
int32_t vax_qbus_test_record_read(uint32_t pa);
#endif

#ifdef VAX_QBUS_TEST_RECORD_WRITES
void vax_qbus_test_record_write(uint32_t pa, int32_t val, int32_t mode);
#endif

#endif
