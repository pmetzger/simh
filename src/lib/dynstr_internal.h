/* dynstr_internal.h: Internal dynamic string test hooks */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef DYNSTR_INTERNAL_H_
#define DYNSTR_INTERNAL_H_ 1

#include <stdarg.h>
#include <stddef.h>

#include "c_attrs.h"

/* Test hook for formatted output sizing and rendering. */
typedef int (*dynstr_vsnprintf_fn)(char *buf, size_t size, const char *fmt,
                                   va_list args) PRINTF_FMT(3, 0);

/* Install a test formatting hook for deterministic error coverage. */
void dynstr_set_test_vsnprintf_hook(dynstr_vsnprintf_fn hook);

/* Restore the default formatter hook. */
void dynstr_reset_test_hooks(void);

#endif
