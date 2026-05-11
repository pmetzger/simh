// SPDX-FileCopyrightText: 2005-2020 Rich Felker
// SPDX-License-Identifier: MIT

#include <stdlib.h>
#include <string.h>

#undef strndup

char *strndup(const char *s, size_t n)
{
    size_t l = strnlen(s, n);
    char *d = malloc(l + 1);

    if (!d)
        return NULL;
    memcpy(d, s, l);
    d[l] = 0;
    return d;
}
