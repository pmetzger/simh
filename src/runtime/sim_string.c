/* sim_string.c: Fixed-buffer string helpers for ZIMH */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include "sim_string.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * Append formatted text to an existing NUL-terminated buffer.  The return
 * value follows strlcat-style truncation detection: a non-negative value at
 * least as large as buf_size means output was truncated.
 */
int strlappendf(char *buf, size_t buf_size, const char *fmt, ...)
{
    char *end;
    size_t len = 0;
    int formatted_len;
    va_list args;

    if (buf_size != 0) {
        end = memchr(buf, '\0', buf_size);
        if (end == NULL)
            len = buf_size;
        else
            len = (size_t)(end - buf);
    }

    if (len >= buf_size)
        buf_size = 0;

    va_start(args, fmt);
    formatted_len = vsnprintf(buf_size ? buf + len : NULL,
                              buf_size ? buf_size - len : 0, fmt, args);
    va_end(args);
    if (formatted_len < 0)
        return formatted_len;
    return (int)(len + (size_t)formatted_len);
}
