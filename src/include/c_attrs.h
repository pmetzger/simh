/* c_attrs.h: C attribute compatibility macros */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef C_ATTRS_H_
#define C_ATTRS_H_ 1

/*
 * Allow the compiler to validate printf-style format arguments. The
 * arguments are the one-based format string and first variadic argument
 * positions used by GCC and Clang's format attribute.
 */
#if !defined(PRINTF_FMT)
#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_FMT(n, m) __attribute__((format(__printf__, n, m)))
#else
#define PRINTF_FMT(n, m)
#endif
#endif

/*
 * Mark an intentional switch fallthrough. Prefer the C23 spelling when
 * available, and fall back to compiler-specific spellings for older C modes.
 */
#if !defined(FALLTHROUGH)
#if defined(__has_c_attribute)
#if __has_c_attribute(fallthrough)
#define FALLTHROUGH [[fallthrough]]
#endif
#endif

#if !defined(FALLTHROUGH) && defined(__has_attribute)
#if __has_attribute(fallthrough)
#define FALLTHROUGH __attribute__((fallthrough))
#endif
#endif

#if !defined(FALLTHROUGH) && defined(__GNUC__) && __GNUC__ >= 7
#define FALLTHROUGH __attribute__((fallthrough))
#endif

#if !defined(FALLTHROUGH)
#define FALLTHROUGH ((void)0)
#endif
#endif

#endif
