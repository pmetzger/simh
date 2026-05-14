/* xalloc.c: fatal allocation wrappers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include "xalloc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Report an allocation contract violation and abort execution. */
static void xalloc_abort(const char *func, const char *reason)
{
    fprintf(stderr, "%s: %s\n", func, reason);
    abort();
}

/* Allocate memory or abort execution. */
void *xmalloc(size_t size)
{
    void *ptr;

    if (size == 0)
        xalloc_abort("xmalloc", "zero-byte allocation request");
    ptr = malloc(size);
    if (ptr == NULL)
        xalloc_abort("xmalloc", "out of memory");
    return ptr;
}

/* Allocate zero-filled memory or abort execution. */
void *xcalloc(size_t count, size_t size)
{
    void *ptr;

    if ((count == 0) || (size == 0))
        xalloc_abort("xcalloc", "zero-byte allocation request");
    if (count > SIZE_MAX / size)
        xalloc_abort("xcalloc", "allocation size overflow");
    ptr = calloc(count, size);
    if (ptr == NULL)
        xalloc_abort("xcalloc", "out of memory");
    return ptr;
}

/* Resize an allocation or abort execution. */
void *xrealloc(void *ptr, size_t size)
{
    void *new_ptr;

    if (size == 0)
        xalloc_abort("xrealloc", "zero-byte allocation request");
    new_ptr = realloc(ptr, size);
    if (new_ptr == NULL)
        xalloc_abort("xrealloc", "out of memory");
    return new_ptr;
}

/* Duplicate a NUL-terminated string or abort execution. */
char *xstrdup(const char *str)
{
    size_t len;
    char *copy;

    if (str == NULL)
        xalloc_abort("xstrdup", "NULL string");
    len = strlen(str) + 1;
    copy = (char *)xmalloc(len);
    memcpy(copy, str, len);
    return copy;
}

/* Duplicate at most max_len bytes of a NUL-terminated string or abort. */
char *xstrndup(const char *str, size_t max_len)
{
    size_t len;
    char *copy;

    if (str == NULL)
        xalloc_abort("xstrndup", "NULL string");
    len = 0;
    while ((len < max_len) && (str[len] != '\0'))
        ++len;
    copy = (char *)xmalloc(len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}
