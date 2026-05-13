/* vax780_fload_internal.h: private VAX 780 FLOAD test seams */
// SPDX-FileCopyrightText: 2006-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX_780_FLOAD_INTERNAL_H_
#define VAX_780_FLOAD_INTERNAL_H_ 1

#include <stdbool.h>
#include <stdint.h>

bool rtfile_read(uint32_t block, uint32_t count, uint16_t *buffer);
uint32_t rtfile_find(uint32_t block, uint32_t sector);

#endif /* VAX_780_FLOAD_INTERNAL_H_ */
