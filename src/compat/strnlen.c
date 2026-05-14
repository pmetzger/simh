// SPDX-FileCopyrightText: 2005-2020 Rich Felker
// SPDX-License-Identifier: MIT

#include <string.h>

#include "string_compat.h"

#undef strnlen

size_t strnlen(const char *s, size_t n)
{
    const char *p = memchr(s, 0, n);

    return p ? (size_t)(p - s) : n;
}
