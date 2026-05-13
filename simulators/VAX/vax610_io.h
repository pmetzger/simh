/* vax610_io.h: MicroVAX I Qbus I/O interfaces */
// SPDX-FileCopyrightText: 2011-2012 Matt Burke
// SPDX-License-Identifier: X11

#ifndef VAX610_IO_H_
#define VAX610_IO_H_ 1

#include <stdint.h>

int32_t Map_ReadB(uint32_t ba, int32_t bc, uint8_t *buf);
int32_t Map_ReadW(uint32_t ba, int32_t bc, uint16_t *buf);
int32_t Map_WriteB(uint32_t ba, int32_t bc, const uint8_t *buf);
int32_t Map_WriteW(uint32_t ba, int32_t bc, const uint16_t *buf);
int32_t ReadIOU(uint32_t pa, int32_t lnt);
void WriteIOU(uint32_t pa, int32_t val, int32_t lnt);
void ioreset_wr(int32_t data);

#endif /* VAX610_IO_H_ */
