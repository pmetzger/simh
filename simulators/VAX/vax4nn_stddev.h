/* vax4nn_stddev.h: KA4nn standard-device interfaces */
// SPDX-FileCopyrightText: 2019 Matt Burke
// SPDX-FileCopyrightText: 1998-2008 Robert M Supnik
// SPDX-License-Identifier: X11

#ifndef VAX4NN_STDDEV_H_
#define VAX4NN_STDDEV_H_ 1

#include <stdint.h>

extern int32_t tmr_int;
extern uint32_t *rom;

/*
 * Read and write the KA4nn interval clock control/status register.  The
 * legacy VAX I/O interface pattern uses signed int32_t parameters and return
 * values, but the register contents are guest-visible bit patterns.
 */
int32_t iccs_rd(void);
void iccs_wr(int32_t data);

/*
 * Read and write the KA4nn non-volatile RAM address space.  The address and
 * value parameters use the legacy signed int32_t VAX I/O callback pattern;
 * callers should treat them as 32-bit guest address and register bit
 * patterns.
 */
int32_t nvr_rd(int32_t pa);
void nvr_wr(int32_t pa, int32_t val, int32_t lnt);

/*
 * Read the KA4nn boot ROM.  The signed int32_t signature follows the legacy
 * signed VAX load and I/O callback pattern; the implementation treats the
 * address and returned word as unsigned guest bit patterns.
 */
int32_t rom_rd(int32_t pa);

int32_t ReadIOU(uint32_t pa, int32_t lnt);
void WriteIOU(uint32_t pa, int32_t val, int32_t lnt);

#endif /* VAX4NN_STDDEV_H_ */
