/* setenv.c: Windows compatibility for setenv and unsetenv */
// SPDX-FileCopyrightText: The ZIMH Project
// SPDX-License-Identifier: MIT

#include "sim_win32_compat.h"

#if defined(_WIN32)

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* Return the CRT environment entry for name, or NULL when it is absent. */
static char *find_environ_entry(const char *name)
{
    size_t name_len = strlen(name);
    char **envp;

    for (envp = _environ; (envp != NULL) && (*envp != NULL); ++envp) {
        if ((_strnicmp(*envp, name, name_len) == 0) &&
            ((*envp)[name_len] == '='))
            return *envp;
    }
    return NULL;
}

/* Install a present empty value in both the CRT and process environments. */
static int set_empty_env(const char *envname)
{
    char *entry;
    int status;

    status = _putenv_s(envname, " ");
    if (status != 0) {
        errno = status;
        return -1;
    }
    entry = find_environ_entry(envname);
    if (entry == NULL) {
        errno = EINVAL;
        return -1;
    }
    entry[strlen(envname) + 1] = '\0';
    if (!SetEnvironmentVariableA(envname, "")) {
        _putenv_s(envname, "");
        errno = EINVAL;
        return -1;
    }
    return 0;
}

/* Install or update one environment variable using the Windows CRT. */
int setenv(const char *envname, const char *envval, int overwrite)
{
    int status;

    if ((envname == NULL) || (envval == NULL) ||
        (strchr(envname, '=') != NULL) || (*envname == '\0')) {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite && (getenv(envname) != NULL))
        return 0;
    if (*envval == '\0')
        return set_empty_env(envname);
    status = _putenv_s(envname, envval);
    if (status != 0) {
        errno = status;
        return -1;
    }
    return 0;
}

/* Remove one environment variable using the Windows CRT. */
int unsetenv(const char *envname)
{
    int status;

    if ((envname == NULL) || (strchr(envname, '=') != NULL) ||
        (*envname == '\0')) {
        errno = EINVAL;
        return -1;
    }
    status = _putenv_s(envname, "");
    if (status != 0) {
        errno = status;
        return -1;
    }
    return 0;
}

#endif
