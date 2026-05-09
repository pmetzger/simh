/* zx200a_internal.h: Zendex ZX-200A private controller state */
// SPDX-FileCopyrightText: 2010 William A. Beech
// SPDX-License-Identifier: X11

#ifndef ZX200A_INTERNAL_H_
#define ZX200A_INTERNAL_H_

#include <stdint.h>

#include "system_defs.h"

#define ZX200A_FDD_NUM 6

typedef struct { // FDD definition
    uint8_t sec;
    uint8_t cyl;
    uint8_t dd;
} FDDDEF;

typedef struct {                // FDC definition
    uint8_t baseport;           // FDC base port
    uint8_t intnum;             // interrupt number
    uint8_t verb;               // verbose flag
    uint16_t iopb;              // FDC IOPB
    uint8_t DDstat;             // FDC DD status
    uint8_t SDstat;             // FDC SD status
    uint8_t rdychg;             // FDC ready change
    uint8_t rtype;              // FDC result type
    uint8_t rbyte0;             // FDC result byte for type 00
    uint8_t rbyte1;             // FDC result byte for type 10
    uint8_t intff;              // fdc interrupt FF
    FDDDEF fdd[ZX200A_FDD_NUM]; // indexed by the FDD number
} FDCDEF;

extern FDCDEF zx200a;

#endif /* ZX200A_INTERNAL_H_ */
