/* sim_tempfile.c: simulator temporary file helpers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include "sim_tempfile.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sim_defs.h"
#include "sim_types.h"

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "sim_host_path.h"

static const char sim_tempfile_chars[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/* Return the current host process ID for temporary-name seeding. */
static uint_t sim_tempfile_process_id(void)
{
#if defined(_WIN32)
    return (uint_t)_getpid();
#else
    return (uint_t)getpid();
#endif
}

#if defined(_WIN32)
/* Map selected Windows file-creation failures to errno values. */
static void sim_tempfile_set_errno(DWORD error)
{
    switch (error) {
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        errno = EEXIST;
        break;

    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        errno = ENOENT;
        break;

    case ERROR_ACCESS_DENIED:
        errno = EACCES;
        break;

    case ERROR_FILENAME_EXCED_RANGE:
        errno = ENAMETOOLONG;
        break;

    default:
        errno = EINVAL;
        break;
    }
}
#endif

/* Copy the centralized host temporary directory into caller storage. */
static int sim_tempfile_get_temp_dir(char *buffer, size_t buffer_size)
{
    if (sim_host_temp_dir(buffer, buffer_size) == NULL) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

/* Return whether a directory path needs a trailing path separator. */
static int sim_tempfile_needs_separator(const char *dir)
{
    size_t len;
    char last;

    len = strlen(dir);
    if (len == 0)
        return 0;

    last = dir[len - 1];
    return last != '/' && last != '\\';
}

/* Build a unique-name template in the selected temporary directory. */
static int sim_tempfile_build_template(char *path, size_t path_size,
                                       char **first_x, const char *temp_dir,
                                       const char *prefix, const char *suffix)
{
    size_t temp_dir_len;
    size_t prefix_len;
    size_t suffix_len;
    size_t separator_len;
    size_t need;
    char *cursor;

    temp_dir_len = strlen(temp_dir);
    prefix_len = strlen(prefix);
    suffix_len = strlen(suffix);
    separator_len = sim_tempfile_needs_separator(temp_dir) ? 1 : 0;

    if (temp_dir_len > SIZE_MAX - separator_len ||
        temp_dir_len + separator_len > SIZE_MAX - prefix_len ||
        temp_dir_len + separator_len + prefix_len > SIZE_MAX - 6 ||
        temp_dir_len + separator_len + prefix_len + 6 > SIZE_MAX - suffix_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    need = temp_dir_len + separator_len + prefix_len + 6 + suffix_len + 1;
    if (need > path_size) {
        errno = ENAMETOOLONG;
        return -1;
    }

    cursor = path;
    memcpy(cursor, temp_dir, temp_dir_len);
    cursor += temp_dir_len;
    if (separator_len != 0)
        *cursor++ =
#if defined(_WIN32)
            '\\';
#else
            '/';
#endif
    memcpy(cursor, prefix, prefix_len);
    cursor += prefix_len;
    *first_x = cursor;
    memcpy(cursor, "XXXXXX", 6);
    cursor += 6;
    memcpy(cursor, suffix, suffix_len);
    cursor += suffix_len;
    *cursor = '\0';

    return 0;
}

/* Replace the six template X characters with one base-62 attempt value. */
static void sim_tempfile_fill_suffix(char *first_x, uint_t value)
{
    int i;

    for (i = 0; i < 6; i++) {
        first_x[i] =
            sim_tempfile_chars[value % (sizeof(sim_tempfile_chars) - 1)];
        value /= (uint_t)(sizeof(sim_tempfile_chars) - 1);
    }
}

/* Atomically create the requested path for binary read/write access. */
static int sim_tempfile_open_path(const char *path, bool delete_on_close)
{
#if defined(_WIN32)
    HANDLE handle;
    DWORD attributes;
    int fd;

    attributes = FILE_ATTRIBUTE_TEMPORARY;
    if (delete_on_close)
        attributes |= FILE_FLAG_DELETE_ON_CLOSE;

    handle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                         CREATE_NEW, attributes, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        sim_tempfile_set_errno(GetLastError());
        return -1;
    }

    fd = _open_osfhandle((intptr_t)handle, _O_RDWR | _O_BINARY);
    if (fd < 0) {
        CloseHandle(handle);
        return -1;
    }

    return fd;
#else
    (void)delete_on_close;

    int flags = O_CREAT | O_EXCL | O_RDWR;

#if defined(O_CLOEXEC)
    flags |= O_CLOEXEC;
#endif
    return open(path, flags, S_IRUSR | S_IWUSR);
#endif
}

/* Create an atomically unique temporary path with selected close behavior. */
static int sim_tempfile_create(char *path, size_t path_size,
                               const char *prefix, const char *suffix,
                               bool delete_on_close)
{
    char temp_dir[PATH_MAX + 1];
    char *first_x;
    uint_t seed;
    uint_t attempt;

    if (path == NULL || path_size == 0) {
        errno = EINVAL;
        return -1;
    }

    if (prefix == NULL)
        prefix = "";
    if (suffix == NULL)
        suffix = "";

    if (sim_tempfile_get_temp_dir(temp_dir, sizeof(temp_dir)) != 0)
        return -1;
    if (sim_tempfile_build_template(path, path_size, &first_x, temp_dir, prefix,
                                    suffix) != 0)
        return -1;

    seed = sim_tempfile_process_id();
    seed ^= (uint_t)(uintptr_t)path;
    seed ^= (uint_t)time(NULL);

    for (attempt = 0; attempt < 10000; attempt++) {
        int fd;

        sim_tempfile_fill_suffix(first_x, seed + attempt);
        fd = sim_tempfile_open_path(path, delete_on_close);
        if (fd >= 0)
            return fd;
        if (errno != EEXIST)
            return -1;
    }

    errno = EEXIST;
    return -1;
}

/* Create an atomically unique temporary file and return its file descriptor. */
int sim_tempfile_open(char *path, size_t path_size, const char *prefix,
                      const char *suffix)
{
    return sim_tempfile_create(path, path_size, prefix, suffix, false);
}

/* Create an atomically unique temporary file as a stdio stream. */
FILE *sim_tempfile_open_stream(char *path, size_t path_size, const char *prefix,
                               const char *suffix, const char *mode)
{
    FILE *stream;
    int fd;

    if (mode == NULL) {
        errno = EINVAL;
        return NULL;
    }

    fd = sim_tempfile_open(path, path_size, prefix, suffix);
    if (fd < 0)
        return NULL;

#if defined(_WIN32)
    stream = _fdopen(fd, mode);
#else
    stream = fdopen(fd, mode);
#endif
    if (stream == NULL) {
        int saved_errno = errno;

#if defined(_WIN32)
        _close(fd);
#else
        close(fd);
#endif
        remove(path);
        errno = saved_errno;
    }

    return stream;
}

/* Create a temporary stream that is automatically removed on close. */
FILE *sim_tmpfile(void)
{
    char path[PATH_MAX + 1];
    FILE *stream;
    int fd;

    fd = sim_tempfile_create(path, sizeof(path), "zimh-tmp-", ".tmp", true);
    if (fd < 0)
        return NULL;

#if defined(_WIN32)
    stream = _fdopen(fd, "w+b");
#else
    stream = fdopen(fd, "w+b");
#endif
    if (stream == NULL) {
        int saved_errno = errno;

#if defined(_WIN32)
        _close(fd);
#else
        close(fd);
#endif
        remove(path);
        errno = saved_errno;
        return NULL;
    }

#if !defined(_WIN32)
    if (unlink(path) != 0) {
        int saved_errno = errno;

        fclose(stream);
        errno = saved_errno;
        return NULL;
    }
#endif

    return stream;
}
