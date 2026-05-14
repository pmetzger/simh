// SPDX-FileCopyrightText: The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef STRING_COMPAT_H_
#define STRING_COMPAT_H_ 1

/*
 * String compatibility routines that may be missing on several hosts.
 * Do not override fortified libc macros here; tests that need to call the
 * shim directly must undefine those macros locally.
 */

#include <stddef.h>

#if defined(SIMH_NEED_STRLCPY) && !defined(strlcpy)
size_t strlcpy(char *dst, const char *src, size_t dsize);
#endif

#if defined(SIMH_NEED_STRLCAT) && !defined(strlcat)
size_t strlcat(char *dst, const char *src, size_t dsize);
#endif

#if defined(SIMH_NEED_STRNLEN) && !defined(strnlen)
size_t strnlen(const char *s, size_t n);
#endif

#if defined(SIMH_NEED_STRDUP) && !defined(strdup)
char *strdup(const char *s);
#endif

#if defined(SIMH_NEED_STRNDUP) && !defined(strndup)
char *strndup(const char *s, size_t n);
#endif

#if defined(SIMH_NEED_STRCASECMP) && !defined(strcasecmp)
int strcasecmp(const char *l, const char *r);
#endif

#if defined(SIMH_NEED_STRNCASECMP) && !defined(strncasecmp)
int strncasecmp(const char *l, const char *r, size_t n);
#endif

#endif /* STRING_COMPAT_H_ */
