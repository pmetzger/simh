#include "test_support.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <process.h>

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

#define lstat stat
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "sim_tempfile.h"

/* Build one filesystem path from a base directory and relative leaf. */
int simh_test_join_path(char *buffer, size_t buffer_size, const char *base,
                        const char *relative_path)
{
    int written;

    written = snprintf(buffer, buffer_size, "%s/%s", base, relative_path);
    if (written < 0 || (size_t)written >= buffer_size) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

static int remove_directory_contents(const char *path)
{
#if defined(_WIN32)
    intptr_t handle;
    struct _finddata_t entry;
    char pattern[PATH_MAX];

    if (snprintf(pattern, sizeof(pattern), "%s/*", path) >=
        (int)sizeof(pattern)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    handle = _findfirst(pattern, &entry);
    if (handle == -1) {
        return errno == ENOENT ? 0 : -1;
    }

    do {
        char child[PATH_MAX];

        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }

        if (snprintf(child, sizeof(child), "%s/%s", path, entry.name) >=
            (int)sizeof(child)) {
            _findclose(handle);
            errno = ENAMETOOLONG;
            return -1;
        }

        if (simh_test_remove_path(child) != 0) {
            _findclose(handle);
            return -1;
        }
    } while (_findnext(handle, &entry) == 0);

    if (errno != ENOENT) {
        _findclose(handle);
        return -1;
    }

    return _findclose(handle) == 0 ? 0 : -1;
#else
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);
    if (dir == NULL) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child[PATH_MAX];

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >=
            (int)sizeof(child)) {
            closedir(dir);
            errno = ENAMETOOLONG;
            return -1;
        }

        if (simh_test_remove_path(child) != 0) {
            closedir(dir);
            return -1;
        }
    }

    if (closedir(dir) != 0) {
        return -1;
    }

    return 0;
#endif
}

const char *simh_test_source_root(void)
{
    return SIMH_TEST_SOURCE_ROOT;
}

const char *simh_test_binary_root(void)
{
    return SIMH_TEST_BINARY_ROOT;
}

/* Initialize a minimal one-unit device for host-side common-code tests. */
void simh_test_init_device_unit(DEVICE *device, UNIT *unit,
                                const char *dev_name, const char *unit_name,
                                uint32_t dev_flags, uint32_t unit_flags,
                                uint32_t dwidth, uint32_t aincr)
{
    memset(device, 0, sizeof(*device));
    memset(unit, 0, sizeof(*unit));

    unit->flags = unit_flags;
    unit->uname = (char *)unit_name;
    unit->dptr = device;

    device->name = dev_name;
    device->units = unit;
    device->numunits = 1;
    device->flags = dev_flags;
    device->dwidth = dwidth;
    device->aincr = aincr;
}

int simh_test_fixture_path(char *buffer, size_t buffer_size,
                           const char *relative_path)
{
    char base[PATH_MAX];

    if (simh_test_join_path(base, sizeof(base), simh_test_source_root(),
                            "tests/unit/fixtures") != 0) {
        return -1;
    }

    return simh_test_join_path(buffer, buffer_size, base, relative_path);
}

int simh_test_make_temp_dir(char *buffer, size_t buffer_size,
                            const char *prefix)
{
#if defined(_WIN32)
    int pid;
    int counter;

    pid = _getpid();
    for (counter = 0; counter < 1000; counter++) {
        int written;

        written = snprintf(buffer, buffer_size, "%s/%s-%d-%d",
                           simh_test_binary_root(), prefix, pid, counter);
        if (written < 0 || (size_t)written >= buffer_size) {
            errno = ENAMETOOLONG;
            return -1;
        }

        if (_mkdir(buffer) == 0) {
            return 0;
        }
        if (errno != EEXIST) {
            return -1;
        }
    }

    errno = EEXIST;
    return -1;
#else
    int written;

    written = snprintf(buffer, buffer_size, "%s/%s-XXXXXX",
                       simh_test_binary_root(), prefix);
    if (written < 0 || (size_t)written >= buffer_size) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return mkdtemp(buffer) == NULL ? -1 : 0;
#endif
}

int simh_test_read_file(const char *path, void **data_out, size_t *size_out)
{
    FILE *file;
    long size;
    void *data;
    size_t bytes_read;

    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return -1;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    data = malloc((size_t)size + 1);
    if (data == NULL) {
        fclose(file);
        errno = ENOMEM;
        return -1;
    }

    bytes_read = fread(data, 1, (size_t)size, file);
    if (bytes_read != (size_t)size) {
        free(data);
        fclose(file);
        errno = EIO;
        return -1;
    }

    ((char *)data)[size] = '\0';

    if (fclose(file) != 0) {
        free(data);
        return -1;
    }

    *data_out = data;
    *size_out = (size_t)size;

    return 0;
}

int simh_test_read_fixture(const char *relative_path, void **data_out,
                           size_t *size_out)
{
    char path[PATH_MAX];

    if (simh_test_fixture_path(path, sizeof(path), relative_path) != 0) {
        return -1;
    }

    return simh_test_read_file(path, data_out, size_out);
}

int simh_test_write_file(const char *path, const void *data, size_t size)
{
    FILE *file;
    size_t bytes_written;

    file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }

    bytes_written = fwrite(data, 1, size, file);
    if (bytes_written != size) {
        fclose(file);
        errno = EIO;
        return -1;
    }

    return fclose(file) == 0 ? 0 : -1;
}

/* Read the full contents of a seekable stream into a NUL-terminated buffer. */
int simh_test_read_stream(FILE *stream, char **text_out, size_t *size_out)
{
    long size;
    char *data;
    size_t bytes_read;

    if (fflush(stream) != 0) {
        return -1;
    }

    if (fseek(stream, 0, SEEK_END) != 0) {
        return -1;
    }

    size = ftell(stream);
    if (size < 0) {
        return -1;
    }

    if (fseek(stream, 0, SEEK_SET) != 0) {
        return -1;
    }

    data = malloc((size_t)size + 1);
    if (data == NULL) {
        errno = ENOMEM;
        return -1;
    }

    bytes_read = fread(data, 1, (size_t)size, stream);
    if (bytes_read != (size_t)size) {
        free(data);
        errno = EIO;
        return -1;
    }

    data[size] = '\0';
    *text_out = data;
    *size_out = (size_t)size;
    return 0;
}

/* Capture text written to stdout by a callback into a heap buffer. */
int simh_test_capture_stdout(simh_test_stream_writer writer, void *context,
                             char **text_out, size_t *size_out)
{
    const int stdout_fd = fileno(stdout);
    int saved_stdout;
    FILE *capture;
    int status;

    if (fflush(stdout) != 0) {
        return -1;
    }

    saved_stdout = dup(stdout_fd);
    if (saved_stdout < 0) {
        return -1;
    }

    capture = sim_tmpfile();
    if (capture == NULL) {
        close(saved_stdout);
        return -1;
    }

    if (dup2(fileno(capture), stdout_fd) < 0) {
        fclose(capture);
        close(saved_stdout);
        return -1;
    }

    writer(context);

    status = 0;
    if (fflush(stdout) != 0) {
        status = -1;
    }
    if (dup2(saved_stdout, stdout_fd) < 0) {
        status = -1;
    }
    close(saved_stdout);

    if (status == 0 &&
        simh_test_read_stream(capture, text_out, size_out) != 0) {
        status = -1;
    }

    if (fclose(capture) != 0) {
        status = -1;
    }

    return status;
}

int simh_test_files_equal(const char *left_path, const char *right_path)
{
    void *left_data;
    void *right_data;
    size_t left_size;
    size_t right_size;
    int result;

    if (simh_test_read_file(left_path, &left_data, &left_size) != 0) {
        return -1;
    }

    if (simh_test_read_file(right_path, &right_data, &right_size) != 0) {
        free(left_data);
        return -1;
    }

    result = left_size == right_size &&
             memcmp(left_data, right_data, left_size) == 0;

    free(left_data);
    free(right_data);

    return result ? 1 : 0;
}

int simh_test_remove_path(const char *path)
{
    struct stat st;

    if (lstat(path, &st) != 0) {
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (remove_directory_contents(path) != 0) {
            return -1;
        }

        return rmdir(path);
    }

    return unlink(path);
}
