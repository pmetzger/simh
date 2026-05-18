/* sim_host_path.c: Host path-name discovery helpers

   This module owns reusable host path-name answers that are independent
   of a specific file operation. Keep platform lookup order and API
   selection here when callers need a host path spelling rather than
   direct file I/O.
*/
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include "sim_host_path_internal.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sim_defs.h"

/* Copy a path only when caller-provided storage can hold the full string. */
static bool sim_host_path_copy(char *buf, size_t buf_size, const char *path)
{
    if (strlcpy(buf, path, buf_size) >= buf_size)
        return false;
    return true;
}

/* Strip one balanced quote layer from a host path argument.

   File path quoting groups the argument but does not make the path a
   C-style string literal. Backslashes remain path characters. */
static bool sim_strip_host_path_quotes(const char *path, char *buf,
                                       size_t buf_size)
{
    char quote;
    const char *inner;
    size_t copy_len;
    size_t len;

    if ((path == NULL) || (buf == NULL) || (buf_size == 0)) {
        errno = EINVAL;
        return false;
    }

    len = strlen(path);
    if (len == 0) {
        buf[0] = '\0';
        return true;
    }

    quote = path[0];
    if ((quote != '"') && (quote != '\'')) {
        if (!sim_host_path_copy(buf, buf_size, path)) {
            errno = ENAMETOOLONG;
            return false;
        }
        return true;
    }

    if ((len == 1) || (path[len - 1] != quote)) {
        errno = EINVAL;
        return false;
    }

    for (inner = path + 1; inner < path + len - 1; ++inner) {
        if (*inner == quote) {
            errno = EINVAL;
            return false;
        }
    }

    copy_len = len - 2;
    if (copy_len >= buf_size) {
        errno = ENAMETOOLONG;
        return false;
    }
    memcpy(buf, path + 1, copy_len);
    buf[copy_len] = '\0';
    return true;
}

/* Expand a leading ~/ in a host path after file-argument quoting has
   already been stripped. */
static bool sim_expand_home_path(const char *path, char *buf, size_t buf_size)
{
    const char *home;
    const char *drive;
    int written;
    char *slash;

    if ((path == NULL) || (buf == NULL) || (buf_size == 0)) {
        errno = EINVAL;
        return false;
    }

    if ((path[0] != '~') || (path[1] != '/')) {
        if (!sim_host_path_copy(buf, buf_size, path)) {
            errno = ENAMETOOLONG;
            return false;
        }
        return true;
    }

    home = getenv("HOME");
    if (home == NULL) {
        home = getenv("HOMEPATH");
        drive = getenv("HOMEDRIVE");
    } else {
        drive = NULL;
    }

    if (home == NULL) {
        if (!sim_host_path_copy(buf, buf_size, path)) {
            errno = ENAMETOOLONG;
            return false;
        }
        return true;
    }

    written = snprintf(buf, buf_size, "%s%s%s%s", drive != NULL ? drive : "",
                       home, strchr(home, '/') != NULL ? "/" : "\\", path + 2);
    if ((written < 0) || ((size_t)written >= buf_size)) {
        errno = ENAMETOOLONG;
        return false;
    }

    while ((strchr(buf, '\\') != NULL) && ((slash = strchr(buf, '/')) != NULL))
        *slash = '\\';
    return true;
}

/* Normalize a user-supplied host path argument in caller-provided storage. */
const char *sim_normalize_host_path(const char *path, char *buf,
                                    size_t buf_size)
{
    char unquoted[PATH_MAX + 1];

    errno = 0;
    if (!sim_strip_host_path_quotes(path, unquoted, sizeof(unquoted)))
        return NULL;
    if (!sim_expand_home_path(unquoted, buf, buf_size))
        return NULL;
    return buf;
}

#if defined(_WIN32)
typedef enum {
    SIM_WIN32_TEMP_DIR_UNAVAILABLE,
    SIM_WIN32_TEMP_DIR_TOO_LONG,
    SIM_WIN32_TEMP_DIR_OK
} sim_win32_temp_dir_result;

static sim_host_win32_temp_path_fn sim_win32_get_temp_path2_hook = NULL;
static sim_host_win32_temp_path_fn sim_win32_get_temp_path_hook = NULL;
static bool sim_win32_temp_path_hooks_installed = false;

/* Return the Windows GetTempPath2A result, or zero if it is unavailable. */
static DWORD sim_win32_default_get_temp_path2(DWORD buf_size, LPSTR buf)
{
    typedef DWORD(WINAPI * get_temp_path2_fn)(DWORD, LPSTR);
    HMODULE kernel32;
    get_temp_path2_fn get_temp_path2 = NULL;

    kernel32 = GetModuleHandleA("kernel32.dll");
    if (kernel32 != NULL)
        get_temp_path2 =
            (get_temp_path2_fn)GetProcAddress(kernel32, "GetTempPath2A");
    return get_temp_path2 == NULL ? 0 : get_temp_path2(buf_size, buf);
}

/* Return the Windows GetTempPathA result. */
static DWORD sim_win32_default_get_temp_path(DWORD buf_size, LPSTR buf)
{
    return GetTempPathA(buf_size, buf);
}

/* Install Windows temporary-directory API test hooks. */
void sim_host_path_set_test_win32_temp_hooks(
    sim_host_win32_temp_path_fn get_temp_path2,
    sim_host_win32_temp_path_fn get_temp_path)
{
    sim_win32_get_temp_path2_hook = get_temp_path2;
    sim_win32_get_temp_path_hook = get_temp_path;
    sim_win32_temp_path_hooks_installed = true;
}

/* Restore default host-backed path hooks. */
void sim_host_path_reset_test_hooks(void)
{
    sim_win32_get_temp_path2_hook = NULL;
    sim_win32_get_temp_path_hook = NULL;
    sim_win32_temp_path_hooks_installed = false;
}

/* Return the Windows temporary-directory path from the best available API. */
static sim_win32_temp_dir_result sim_win32_temp_dir(char *buf, size_t buf_size)
{
    sim_host_win32_temp_path_fn get_temp_path2;
    sim_host_win32_temp_path_fn get_temp_path;
    DWORD length;

    if (sim_win32_temp_path_hooks_installed) {
        get_temp_path2 = sim_win32_get_temp_path2_hook;
        get_temp_path = sim_win32_get_temp_path_hook;
    } else {
        get_temp_path2 =
            (sim_host_win32_temp_path_fn)sim_win32_default_get_temp_path2;
        get_temp_path =
            (sim_host_win32_temp_path_fn)sim_win32_default_get_temp_path;
    }

    length = get_temp_path2 == NULL ? 0 : get_temp_path2((DWORD)buf_size, buf);
    if (length > 0)
        return (size_t)length < buf_size ? SIM_WIN32_TEMP_DIR_OK
                                         : SIM_WIN32_TEMP_DIR_TOO_LONG;

    if (length == 0)
        length =
            get_temp_path == NULL ? 0 : get_temp_path((DWORD)buf_size, buf);
    if (length == 0)
        return SIM_WIN32_TEMP_DIR_UNAVAILABLE;

    return (size_t)length < buf_size ? SIM_WIN32_TEMP_DIR_OK
                                     : SIM_WIN32_TEMP_DIR_TOO_LONG;
}
#endif

/* Return the host temporary-file directory path in caller-provided storage. */
const char *sim_host_temp_dir(char *buf, size_t buf_size)
{
    static const char *const env_names[] = {
#if defined(_WIN32)
        "TMP",
        "TEMP",
        "USERPROFILE",
#else
        "TMPDIR",
        "TMP",
        "TEMP",
#endif
        NULL,
    };
    size_t i;

    if ((buf == NULL) || (buf_size == 0))
        return NULL;

#if defined(_WIN32)
    switch (sim_win32_temp_dir(buf, buf_size)) {
    case SIM_WIN32_TEMP_DIR_OK:
        return buf;
    case SIM_WIN32_TEMP_DIR_TOO_LONG:
        return NULL;
    case SIM_WIN32_TEMP_DIR_UNAVAILABLE:
        break;
    }
#endif

    for (i = 0; env_names[i] != NULL; ++i) {
        const char *value = getenv(env_names[i]);

        if ((value != NULL) && (value[0] != '\0')) {
            if (!sim_host_path_copy(buf, buf_size, value))
                return NULL;
            return buf;
        }
    }
#if defined(_WIN32)
    if (!sim_host_path_copy(buf, buf_size, "."))
        return NULL;
#else
    /* Use the conventional command-file fallback, not C's P_tmpdir. */
    if (!sim_host_path_copy(buf, buf_size, "/tmp"))
        return NULL;
#endif
    return buf;
}
