// SPDX-FileCopyrightText: 2005-2020 Rich Felker
// SPDX-License-Identifier: MIT

#include <ctype.h>
#include <stddef.h>

#include "string_compat.h"

#undef strncasecmp

int strncasecmp(const char *_l, const char *_r, size_t n)
{
    const unsigned char *l = (void *)_l, *r = (void *)_r;

    if (!n--)
        return 0;
    for (; *l && *r && n && (*l == *r || tolower(*l) == tolower(*r));
         l++, r++, n--)
        ;
    return tolower(*l) - tolower(*r);
}
