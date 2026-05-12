/* hp3000_format.h: HP 3000 trace formatting helper declarations */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef HP3000_FORMAT_H_
#define HP3000_FORMAT_H_ 0

#include <stddef.h>
#include <stdint.h>

/*
 * This header is included from hp3000_defs.h after BITSET_FORMAT is defined.
 * The fmt_char and fmt_bitset macros preserve the historical call syntax by
 * providing automatic caller-side buffers.  Their returned pointers are valid
 * until the end of the enclosing block; callers that need longer lifetimes
 * should call the _buf forms with their own storage.
 *
 * fmt_bitset_buf formats the active names from a BITSET_FORMAT descriptor.
 * NULL names are skipped, "\1set\0clear" names select one of two alternate
 * spellings when has_alt is used, and append_bar requests a trailing " | "
 * separator for callers that concatenate decoded multi-field values.
 */

#define FMT_CHAR_SIZE 8
#define FMT_BITSET_SIZE 1024

extern const char *fmt_char_buf(char *buffer, size_t buffer_size,
                                uint32_t charval);
extern const char *fmt_bitset_buf(char *buffer, size_t buffer_size,
                                  uint32_t bitset, const BITSET_FORMAT bitfmt);

#define fmt_char(charval)                                                      \
    fmt_char_buf((char[FMT_CHAR_SIZE]){0}, FMT_CHAR_SIZE, (charval))
#define fmt_bitset(bitset, bitfmt)                                             \
    fmt_bitset_buf((char[FMT_BITSET_SIZE]){0}, FMT_BITSET_SIZE, (bitset),      \
                   (bitfmt))

#endif
