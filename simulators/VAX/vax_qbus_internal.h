/* vax_qbus_internal.h: internal VAX Qbus helpers */
// SPDX-FileCopyrightText: 1998-2019 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_QBUS_INTERNAL_H_
#define VAX_QBUS_INTERNAL_H_ 0

#include "vax_defs.h"

#ifdef VAX_QBUS_TEST_RECORD_READS
int32 vax_qbus_test_record_read(uint32 pa);
#endif

#ifdef VAX_QBUS_TEST_RECORD_WRITES
void vax_qbus_test_record_write(uint32 pa, int32 val, int32 mode);
#endif

#endif
