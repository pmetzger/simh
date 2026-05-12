/* hp2100_format.c: HP 2100 trace formatting helpers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include "hp2100_defs.h"

#include <stdio.h>
#include <string.h>

/*
 * Format a character value into caller-provided storage and return that
 * storage so callers can pass the result directly to trace-formatting calls.
 */
const char *fmt_char_buf(char *buffer, size_t buffer_size, uint32_t charval)
{
    static const char *const control[] = {
        "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
        "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
        "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
        "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US"};

    if (buffer_size == 0)
        return "";

    if (charval <= 0037u)
        strlcpy(buffer, control[charval], buffer_size);
    else if (charval == 0177u)
        strlcpy(buffer, "DEL", buffer_size);
    else if (charval > D8_UMAX)
        snprintf(buffer, buffer_size, "%03o", LOWER_BYTE(charval));
    else if (charval > 0177u)
        snprintf(buffer, buffer_size, "\\%03o", charval);
    else
        snprintf(buffer, buffer_size, "'%c'", (char)charval);

    return buffer;
}

/*
 * Format a bitset into caller-provided storage, preserving the historical
 * string spelling and overflow sentinel used by HP trace output.
 */
const char *fmt_bitset_buf(char *buffer, size_t buffer_size, uint32_t bitset,
                           const BITSET_FORMAT bitfmt)
{
    static const char separator[] = " | ";
    const char *bnptr;
    uint32_t test_bit, index, bitmask;

    if (buffer_size == 0)
        return "(buffer overflow)";

    if (bitfmt.name_count < D32_WIDTH)
        bitmask = (1u << bitfmt.name_count) - 1;
    else
        bitmask = D32_MASK;

    bitmask = bitmask << bitfmt.offset;
    bitset = bitset & bitmask;

    if (bitfmt.name_count == 0) {
        if (bitfmt.bar == append_bar)
            return "";
        else
            return "(none)";
    }

    if (bitfmt.direction == msb_first)
        test_bit = 1u << (bitfmt.name_count + bitfmt.offset - 1);
    else
        test_bit = 1u << bitfmt.offset;

    buffer[0] = '\0';
    index = 0;

    while ((bitfmt.alternate || bitset) && index < bitfmt.name_count) {
        bnptr = bitfmt.names[index];

        if (bnptr) {
            if (*bnptr == '\1' && bitfmt.alternate) {
                if (bitset & test_bit)
                    bnptr++;
                else
                    bnptr = bnptr + strlen(bnptr) + 1;
            } else if ((bitset & test_bit) == 0) {
                bnptr = NULL;
            }
        }

        if (bnptr) {
            if (*buffer != '\0' &&
                strlcat(buffer, separator, buffer_size) >= buffer_size)
                return "(buffer overflow)";

            if (strlcat(buffer, bnptr, buffer_size) >= buffer_size)
                return "(buffer overflow)";
        }

        if (bitfmt.direction == msb_first)
            bitset = (bitset << 1) & bitmask;
        else
            bitset = (bitset >> 1) & bitmask;

        index = index + 1;
    }

    if (*buffer == '\0') {
        if (bitfmt.bar == append_bar)
            return "";
        else
            return "(none)";
    } else if (bitfmt.bar == append_bar) {
        if (strlcat(buffer, separator, buffer_size) >= buffer_size)
            return "(buffer overflow)";
    }

    return buffer;
}
