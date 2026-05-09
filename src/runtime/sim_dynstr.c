/* sim_dynstr.c: Dynamic string support for ZIMH */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include <stdarg.h>
#include <stdbool.h>

#include "sim_dynstr.h"
#include "sim_dynstr_internal.h"

/* Tests can replace the allocator to drive allocation-failure paths. */
static sim_dynstr_realloc_fn sim_dynstr_realloc_hook = NULL;
static sim_dynstr_vsnprintf_fn sim_dynstr_vsnprintf_hook = NULL;

/* Use the host allocator for normal dynamic-string growth. */
static void *sim_dynstr_default_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

/* Resolve the realloc implementation currently in force. */
static sim_dynstr_realloc_fn sim_dynstr_realloc_impl(void)
{
    return sim_dynstr_realloc_hook ? sim_dynstr_realloc_hook
                                   : sim_dynstr_default_realloc;
}

/* Use the host formatter for normal dynamic-string formatted append. */
static int PRINTF_FMT(3, 0) sim_dynstr_default_vsnprintf(
    char *buf, size_t size, const char *fmt, va_list args)
{
    return vsnprintf(buf, size, fmt, args);
}

/* Resolve the formatting implementation currently in force. */
static sim_dynstr_vsnprintf_fn sim_dynstr_vsnprintf_impl(void)
{
    return sim_dynstr_vsnprintf_hook ? sim_dynstr_vsnprintf_hook
                                     : sim_dynstr_default_vsnprintf;
}

/* Ensure the string buffer can hold the requested byte count. */
static bool sim_dynstr_reserve(sim_dynstr_t *ds, size_t needed)
{
    sim_dynstr_realloc_fn realloc_fn;
    size_t new_cap;
    char *new_buf;

    if (needed <= ds->cap)
        return true;
    new_cap = ds->cap ? ds->cap : 16;
    while (new_cap < needed)
        new_cap *= 2;
    realloc_fn = sim_dynstr_realloc_impl();
    new_buf = (char *)realloc_fn(ds->buf, new_cap);
    if (new_buf == NULL)
        return false;
    ds->buf = new_buf;
    ds->cap = new_cap;
    return true;
}

/* Initialize one dynamic string to the empty state. */
void sim_dynstr_init(sim_dynstr_t *ds)
{
    ds->buf = NULL;
    ds->len = 0;
    ds->cap = 0;
}

/* Release any storage owned by one dynamic string. */
void sim_dynstr_free(sim_dynstr_t *ds)
{
    free(ds->buf);
    sim_dynstr_init(ds);
}

/* Append one NUL-terminated string to the current contents. */
bool sim_dynstr_append(sim_dynstr_t *ds, const char *text)
{
    size_t text_len = strlen(text);

    if (!sim_dynstr_reserve(ds, ds->len + text_len + 1))
        return false;
    memcpy(ds->buf + ds->len, text, text_len + 1);
    ds->len += text_len;
    return true;
}

/* Append one formatted string fragment. */
bool sim_dynstr_appendf(sim_dynstr_t *ds, const char *fmt, ...)
{
    sim_dynstr_vsnprintf_fn vsnprintf_fn;
    char stackbuf[CBUFSIZE];
    va_list args;
    va_list copy;
    int len;

    vsnprintf_fn = sim_dynstr_vsnprintf_impl();
    va_start(args, fmt);
    va_copy(copy, args);
    len = vsnprintf_fn(stackbuf, sizeof(stackbuf), fmt, copy);
    va_end(copy);
    if (len < 0) {
        va_end(args);
        return false;
    }
    if ((size_t)len < sizeof(stackbuf)) {
        va_end(args);
        return sim_dynstr_append(ds, stackbuf);
    }
    if (!sim_dynstr_reserve(ds, ds->len + (size_t)len + 1)) {
        va_end(args);
        return false;
    }
    (void)vsnprintf_fn(ds->buf + ds->len, (size_t)len + 1, fmt, args);
    va_end(args);
    ds->len += (size_t)len;
    return true;
}

/* Append one character and keep the buffer NUL-terminated. */
bool sim_dynstr_append_ch(sim_dynstr_t *ds, char ch)
{
    if (!sim_dynstr_reserve(ds, ds->len + 2))
        return false;
    ds->buf[ds->len++] = ch;
    ds->buf[ds->len] = '\0';
    return true;
}

/* Return the current contents as a conventional C string view. */
const char *sim_dynstr_cstr(const sim_dynstr_t *ds)
{
    return ds->buf ? ds->buf : "";
}

/* Remove any leading characters that appear in the trim set. */
void sim_dynstr_ltrim_chars(sim_dynstr_t *ds, const char *trimchars)
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
char *sim_dynstr_take(sim_dynstr_t *ds)
{
    char *buf = ds->buf;

    ds->buf = NULL;
    ds->len = 0;
    ds->cap = 0;
    return buf;
}

/* Allocate a new C string containing two concatenated C strings. */
char *sim_dynstr_concat_cstrs(const char *first, const char *second)
{
    sim_dynstr_t ds;

    if ((first == NULL) || (second == NULL))
        return NULL;

    sim_dynstr_init(&ds);
    if (!sim_dynstr_append(&ds, first) || !sim_dynstr_append(&ds, second)) {
        sim_dynstr_free(&ds);
        return NULL;
    }

    return sim_dynstr_take(&ds);
}

/* Install a test allocator hook for deterministic failure coverage. */
void sim_dynstr_set_test_realloc_hook(sim_dynstr_realloc_fn hook)
{
    sim_dynstr_realloc_hook = hook;
}

/* Install a test formatting hook for deterministic error coverage. */
void sim_dynstr_set_test_vsnprintf_hook(sim_dynstr_vsnprintf_fn hook)
{
    sim_dynstr_vsnprintf_hook = hook;
}

/* Restore the default allocator after one test case completes. */
void sim_dynstr_reset_test_hooks(void)
{
    sim_dynstr_set_test_realloc_hook(NULL);
    sim_dynstr_set_test_vsnprintf_hook(NULL);
}
