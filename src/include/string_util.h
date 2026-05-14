/* string_util.h: Fixed-buffer string helpers for ZIMH */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef STRING_UTIL_H_
#define STRING_UTIL_H_ 1

#include "sim_attrs.h"

#include <stddef.h>

/*
 * Append formatted text to an existing NUL-terminated buffer.  The return
 * value is the total string length that would have been produced, or a
 * negative value if formatting fails.
 */
int strlappendf(char *buf, size_t buf_size, const char *fmt, ...)
    PRINTF_FMT(3, 4);

#endif
