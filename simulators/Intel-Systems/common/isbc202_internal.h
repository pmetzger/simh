/* isbc202_internal.h: Intel iSBC 202 private controller state */
// SPDX-FileCopyrightText: 2016 William A. Beech
// SPDX-License-Identifier: X11

#ifndef ISBC202_INTERNAL_H_
#define ISBC202_INTERNAL_H_

#include <stdint.h>

#include "system_defs.h"

#define ISBC202_FDD_NUM 4

typedef struct { // FDD definition
    uint8_t sec;
    uint8_t cyl;
    uint8_t att;
} FDDDEF;

typedef struct {                 // FDC definition
    uint8_t baseport;            // FDC base port
    uint8_t intnum;              // interrupt number
    uint8_t verb;                // verbose flag
    uint16_t iopb;               // FDC IOPB
    uint8_t stat;                // FDC status
    uint8_t rdychg;              // FDC ready change
    uint8_t rtype;               // FDC result type
    uint8_t rbyte0;              // FDC result byte for type 00
    uint8_t rbyte1;              // FDC result byte for type 10
    uint8_t intff;               // fdc interrupt FF
    FDDDEF fdd[ISBC202_FDD_NUM]; // indexed by the FDD number
} FDCDEF;

extern FDCDEF fdc202;

#endif /* ISBC202_INTERNAL_H_ */
