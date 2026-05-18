// SPDX-FileCopyrightText: The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef H_SIM_WIN32_COMPAT
#define H_SIM_WIN32_COMPAT 1

/* Windows compatibility declarations and CRT spelling aliases. */

#if defined(_WIN32) || defined(SIMH_COMPAT_TEST)

#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>

#ifndef F_OK
#define F_OK 0
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#define access _access
#define chdir _chdir
#define close _close
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#define getcwd _getcwd
#define read _read
#define rmdir _rmdir
#define unlink _unlink
#define write _write
#endif

#if defined(_WIN32)
struct tm *localtime_r(const time_t *timer, struct tm *result);

struct tm *gmtime_r(const time_t *timer, struct tm *result);

int setenv(const char *envname, const char *envval, int overwrite);

int unsetenv(const char *envname);
#endif

#endif /* _WIN32 || SIMH_COMPAT_TEST */

#endif /* H_SIM_WIN32_COMPAT */
