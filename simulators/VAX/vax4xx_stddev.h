/* vax4xx_stddev.h: KA4xx standard device interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-FileCopyrightText: 1998-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX4XX_STDDEV_H_
#define VAX4XX_STDDEV_H_ 0

#include "vax_defs.h"

/*
 * Read and write the KA4xx interval clock control/status register.  The
 * legacy VAX I/O interface pattern uses signed int32 parameters and return
 * values, but the register contents are guest-visible bit patterns.
 */
int32 iccs_rd(void);
void iccs_wr(int32 data);

/*
 * Read and write the KA4xx non-volatile RAM address space.  The address and
 * value parameters use the legacy signed int32 VAX I/O callback pattern;
 * callers should treat them as 32-bit guest address and register bit
 * patterns.
 */
int32 nvr_rd(int32 pa);
void nvr_wr(int32 pa, int32 val, int32 lnt);

/*
 * Read a KA4xx option ROM word.  The signed int32 signature follows the
 * legacy signed VAX I/O callback pattern; the implementation treats the
 * address and returned word as unsigned 32-bit machine bit patterns.
 */
int32 or_rd(int32 pa);

/* Map or unmap one KA4xx option ROM image for system option probing. */
t_stat or_map(uint32 index, uint8 *rom, t_addr size);
t_stat or_unmap(uint32 index);

/*
 * Read or byte-write the KA4xx boot ROM.  The signed int32 signature follows
 * the legacy signed VAX load and I/O callback pattern; the implementation
 * treats the address, stored byte, and returned word as unsigned guest bit
 * patterns.
 */
int32 rom_rd(int32 pa);
void rom_wr_B(int32 pa, int32 val);

#endif
