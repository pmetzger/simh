// SPDX-FileCopyrightText: 2005-2020 Rich Felker
// SPDX-License-Identifier: MIT

#include <ctype.h>

#include "sim_string_compat.h"

#undef strcasecmp

int strcasecmp(const char *_l, const char *_r)
{
    const unsigned char *l = (void *)_l, *r = (void *)_r;

    for (; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++)
        ;
    return tolower(*l) - tolower(*r);
}
