/* isbc201_internal.h: Intel iSBC 201 private controller state */
// SPDX-FileCopyrightText: 2010 William A. Beech
// SPDX-License-Identifier: X11

#ifndef ISBC201_INTERNAL_H_
#define ISBC201_INTERNAL_H_

#include <stdint.h>

#include "system_defs.h"

#define ISBC201_FDD_NUM 2

typedef struct { // FDD definition
    uint8_t sec;
    uint8_t cyl;
} FDDDEF;

typedef struct {                 // FDC board definition
    uint8_t baseport;            // FDC base port
    uint8_t intnum;              // interrupt number
    uint8_t verb;                // verbose flag
    uint16_t iopb;               // FDC IOPB
    uint8_t stat;                // FDC status
    uint8_t rdychg;              // FDC ready changed
    uint8_t rtype;               // FDC result type
    uint8_t rbyte0;              // FDC result byte for type 00
    uint8_t rbyte1;              // FDC result byte for type 10
    uint8_t intff;               // FDC interrupt FF
    FDDDEF fdd[ISBC201_FDD_NUM]; // indexed by the FDD number
} FDCDEF;

extern FDCDEF fdc201;

#endif /* ISBC201_INTERNAL_H_ */
