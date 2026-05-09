/* isbc206_internal.h: Intel iSBC 206 private controller state */
// SPDX-FileCopyrightText: 2017 William A. Beech
// SPDX-License-Identifier: X11

#ifndef ISBC206_INTERNAL_H_
#define ISBC206_INTERNAL_H_

#include <stdint.h>

#include "system_defs.h"

#define ISBC206_HDD_NUM 2

typedef struct { // HDD definition
    int t0;
    int rdy;
    uint8_t sec;
    uint8_t cyl;
} HDDDEF;

typedef struct {                // HDC definition
    uint8_t baseport;           // HDC base port
    uint8_t intnum;             // interrupt number
    uint8_t verb;               // verbose flag
    uint16_t iopb;              // HDC IOPB
    uint8_t stat;               // HDC status
    uint8_t rdychg;             // HDC ready change
    uint8_t rtype;              // HDC result type
    uint8_t rbyte0;             // HDC result byte for type 00
    uint8_t rbyte1;             // HDC result byte for type 10
    uint8_t intff;              // HDC interrupt FF
    HDDDEF hd[ISBC206_HDD_NUM]; // indexed by the HDD number
} HDCDEF;

extern HDCDEF hdc206;

#endif /* ISBC206_INTERNAL_H_ */
