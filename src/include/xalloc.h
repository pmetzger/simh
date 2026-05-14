/* xalloc.h: fatal allocation wrappers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef XALLOC_H_
#define XALLOC_H_ 0

#include <stddef.h>

/*
 * Allocate memory or abort execution. A zero-byte request is treated as a
 * caller error and aborts.
 */
void *xmalloc(size_t size);

/*
 * Allocate zero-filled memory or abort execution. A zero-sized element count
 * or element size is treated as a caller error and aborts.
 */
void *xcalloc(size_t count, size_t size);

/*
 * Resize an allocation or abort execution. A zero-byte request is treated as a
 * caller error and aborts.
 */
void *xrealloc(void *ptr, size_t size);

/* Duplicate a NUL-terminated string or abort execution. */
char *xstrdup(const char *str);

/*
 * Duplicate at most max_len bytes of a NUL-terminated string or abort. A
 * max_len of zero is valid and returns an owned empty string.
 */
char *xstrndup(const char *str, size_t max_len);

#endif
