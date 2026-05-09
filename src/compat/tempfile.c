/* tempfile.c: Windows compatibility for mkstemp and mkstemps */
// SPDX-FileCopyrightText: The ZIMH Project
// SPDX-License-Identifier: MIT

#include "sim_types.h"
#include "sim_win32_compat.h"

#if defined(_WIN32) || defined(SIMH_COMPAT_TEST)

/*
 * This compatibility shim remains for code that still calls mkstemp or
 * mkstemps on Windows. If all in-tree callers move to sim_tempfile, this
 * file may become a candidate for removal.
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static const char temp_chars[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static uint_t tempfile_process_id(void)
{
#if defined(_WIN32)
    return (uint_t)_getpid();
#else
    return (uint_t)getpid();
#endif
}

static int open_temp_file(const char *path)
{
#if defined(_WIN32)
    /*
     * _O_NOINHERIT keeps the temporary fd out of child processes, matching
     * the practical safety expectation callers have from POSIX mkstemp.
     */
    return _open(path, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY | _O_NOINHERIT,
                 _S_IREAD | _S_IWRITE);
#else
    return open(path, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
#endif
}

/* Replace six template X characters with a base-62 attempt value. */
static void fill_temp_suffix(char *first_x, uint_t value)
{
    int i;

    for (i = 0; i < 6; i++) {
        first_x[i] = temp_chars[value % (sizeof(temp_chars) - 1)];
        value /= (uint_t)(sizeof(temp_chars) - 1);
    }
}

/* Create a unique read/write temporary file from a mkstemps-style template. */
int mkstemps(char *path_template, int suffix_len)
{
    size_t len;
    char *first_x;
    uint_t seed;
    uint_t attempt;

    if (path_template == NULL || suffix_len < 0) {
        errno = EINVAL;
        return -1;
    }

    len = strlen(path_template);
    if (len < (size_t)suffix_len + 6) {
        errno = EINVAL;
        return -1;
    }

    first_x = path_template + len - (size_t)suffix_len - 6;
    if (strncmp(first_x, "XXXXXX", 6) != 0) {
        errno = EINVAL;
        return -1;
    }

    seed = tempfile_process_id();
    seed ^= (uint_t)(uintptr_t)path_template;

    for (attempt = 0; attempt < 10000; attempt++) {
        int fd;

        fill_temp_suffix(first_x, seed + attempt);
        fd = open_temp_file(path_template);
        if (fd >= 0)
            return fd;
        if (errno != EEXIST)
            return -1;
    }

    errno = EEXIST;
    return -1;
}

/* Create a unique read/write temporary file from a mkstemp-style template. */
int mkstemp(char *path_template)
{
    return mkstemps(path_template, 0);
}

#endif
