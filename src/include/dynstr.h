/* dynstr.h: Dynamic string support */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef DYNSTR_H_
#define DYNSTR_H_ 1

#include <stdbool.h>
#include <stddef.h>

#include "c_attrs.h"

typedef struct dynstr {
    char *buf;
    size_t len;
    size_t cap;
} dynstr_t;

/* Initialize one empty dynamic string. */
void dynstr_init(dynstr_t *ds);

/* Release storage owned by one dynamic string and reset it. */
void dynstr_free(dynstr_t *ds);

/* Append one NUL-terminated string, aborting on allocation failure. */
void dynstr_append(dynstr_t *ds, const char *text);

/*
 * Append one formatted string fragment, aborting on allocation failure.
 * Returns false only if the formatter reports an encoding error.
 */
bool dynstr_appendf(dynstr_t *ds, const char *fmt, ...) PRINTF_FMT(2, 3);

/* Append one single character, aborting on allocation failure. */
void dynstr_append_ch(dynstr_t *ds, char ch);

/* Return the current contents as a conventional C string. */
const char *dynstr_cstr(const dynstr_t *ds);

/* Remove any leading characters that appear in the trim set. */
void dynstr_ltrim_chars(dynstr_t *ds, const char *trimchars);

/* Transfer ownership of the backing buffer and reset the string. */
char *dynstr_take(dynstr_t *ds);

/*
 * Allocate a new C string containing two concatenated C strings. Returns NULL
 * for NULL inputs and aborts on allocation failure.
 */
char *dynstr_concat_cstrs(const char *first, const char *second);

#endif
