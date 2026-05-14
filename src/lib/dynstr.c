/* dynstr.c: Dynamic string support */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dynstr.h"
#include "dynstr_internal.h"
#include "xalloc.h"

#define DYNSTR_STACK_SIZE 4096

/* Tests can replace the formatter to drive encoding-failure paths. */
static dynstr_vsnprintf_fn dynstr_vsnprintf_hook = NULL;

/* Use the host formatter for normal dynamic-string formatted append. */
static int PRINTF_FMT(3, 0)
    dynstr_default_vsnprintf(char *buf, size_t size, const char *fmt,
                             va_list args)
{
    return vsnprintf(buf, size, fmt, args);
}

/* Resolve the formatting implementation currently in force. */
static dynstr_vsnprintf_fn dynstr_vsnprintf_impl(void)
{
    return dynstr_vsnprintf_hook ? dynstr_vsnprintf_hook
                                 : dynstr_default_vsnprintf;
}

/* Ensure the string buffer can hold the requested byte count. */
static void dynstr_reserve(dynstr_t *ds, size_t needed)
{
    size_t new_cap;

    if (needed <= ds->cap)
        return;
    new_cap = ds->cap ? ds->cap : 16;
    /*
     * On ordinary 64-bit hosts, size_t overflow here would require a
     * request near 16 exabytes. dynstr is for small internal string
     * construction, so we intentionally do not carry defensive overflow
     * machinery for that case.
     */
    while (new_cap < needed)
        new_cap *= 2;
    ds->buf = (char *)xrealloc(ds->buf, new_cap);
    ds->cap = new_cap;
}

/* Initialize one dynamic string to the empty state. */
void dynstr_init(dynstr_t *ds)
{
    ds->buf = NULL;
    ds->len = 0;
    ds->cap = 0;
}

/* Release any storage owned by one dynamic string. */
void dynstr_free(dynstr_t *ds)
{
    free(ds->buf);
    dynstr_init(ds);
}

/* Append one NUL-terminated string to the current contents. */
void dynstr_append(dynstr_t *ds, const char *text)
{
    size_t text_len = strlen(text);

    dynstr_reserve(ds, ds->len + text_len + 1);
    memcpy(ds->buf + ds->len, text, text_len + 1);
    ds->len += text_len;
}

/* Append one formatted string fragment from a va_list. */
bool dynstr_vappendf(dynstr_t *ds, const char *fmt, va_list args)
{
    dynstr_vsnprintf_fn vsnprintf_fn;
    char stackbuf[DYNSTR_STACK_SIZE];
    va_list copy;
    int len;
    int rendered;

    vsnprintf_fn = dynstr_vsnprintf_impl();
    va_copy(copy, args);
    len = vsnprintf_fn(stackbuf, sizeof(stackbuf), fmt, copy);
    va_end(copy);
    if (len < 0)
        return false;
    if ((size_t)len < sizeof(stackbuf)) {
        dynstr_append(ds, stackbuf);
        return true;
    }
    dynstr_reserve(ds, ds->len + (size_t)len + 1);
    va_copy(copy, args);
    rendered = vsnprintf_fn(ds->buf + ds->len, (size_t)len + 1, fmt, copy);
    va_end(copy);
    if (rendered != len) {
        ds->buf[ds->len] = '\0';
        return false;
    }
    ds->len += (size_t)len;
    return true;
}

/* Append one formatted string fragment. */
bool dynstr_appendf(dynstr_t *ds, const char *fmt, ...)
{
    va_list args;
    bool ok;

    va_start(args, fmt);
    ok = dynstr_vappendf(ds, fmt, args);
    va_end(args);
    return ok;
}

/* Append one character and keep the buffer NUL-terminated. */
void dynstr_append_ch(dynstr_t *ds, char ch)
{
    dynstr_reserve(ds, ds->len + 2);
    ds->buf[ds->len++] = ch;
    ds->buf[ds->len] = '\0';
}

/* Return the current contents as a conventional C string view. */
const char *dynstr_cstr(const dynstr_t *ds)
{
    return ds->buf ? ds->buf : "";
}

/* Remove any leading characters that appear in the trim set. */
void dynstr_ltrim_chars(dynstr_t *ds, const char *trimchars)
{
    size_t skip = 0;

    if (ds->buf == NULL)
        return;
    while ((skip < ds->len) && strchr(trimchars, ds->buf[skip]))
        ++skip;
    if (skip == 0)
        return;
    memmove(ds->buf, ds->buf + skip, ds->len - skip + 1);
    ds->len -= skip;
}

/* Transfer ownership of the backing buffer and reset the dynamic string. */
char *dynstr_take(dynstr_t *ds)
{
    char *buf = ds->buf;

    ds->buf = NULL;
    ds->len = 0;
    ds->cap = 0;
    return buf;
}

/* Allocate a new C string containing two concatenated C strings. */
char *dynstr_concat_cstrs(const char *first, const char *second)
{
    dynstr_t ds;

    if ((first == NULL) || (second == NULL))
        return NULL;

    dynstr_init(&ds);
    dynstr_append(&ds, first);
    dynstr_append(&ds, second);

    return dynstr_take(&ds);
}

/* Install a test formatting hook for deterministic error coverage. */
void dynstr_set_test_vsnprintf_hook(dynstr_vsnprintf_fn hook)
{
    dynstr_vsnprintf_hook = hook;
}

/* Restore the default formatter after one test case completes. */
void dynstr_reset_test_hooks(void)
{
    dynstr_set_test_vsnprintf_hook(NULL);
}
