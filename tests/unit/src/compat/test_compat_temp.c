#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <io.h>
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include "test_cmocka.h"

#include "sim_types.h"
#include "sim_win32_compat.h"
#include "string_compat.h"

static const char test_temp_chars[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static uint_t test_process_id(void)
{
#if defined(_WIN32)
    return (uint_t)_getpid();
#else
    return (uint_t)getpid();
#endif
}

static int test_open_exclusive(const char *path)
{
#if defined(_WIN32)
    return _open(path, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY | _O_NOINHERIT,
                 _S_IREAD | _S_IWRITE);
#else
    return open(path, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
#endif
}

static void test_fill_temp_suffix(char *first_x, uint_t value)
{
    int i;

    for (i = 0; i < 6; i++) {
        first_x[i] = test_temp_chars[value % (sizeof(test_temp_chars) - 1)];
        value /= (uint_t)(sizeof(test_temp_chars) - 1);
    }
}

static void test_expected_attempt(char *path, size_t size, char *template_ptr,
                                  uint_t attempt)
{
    char *first_x;
    uint_t seed;

    assert_true(size >= strlen(template_ptr) + 1);
    strlcpy(path, template_ptr, size);
    first_x = strstr(path, "XXXXXX");
    assert_non_null(first_x);

    seed = test_process_id();
    seed ^= (uint_t)(uintptr_t)template_ptr;
    test_fill_temp_suffix(first_x, seed + attempt);
}

/* Verify mkstemp creates a unique file and updates the template in place. */
static void test_mkstemp_creates_unique_file(void **state)
{
    char path[] = "zimh-compat-temp-XXXXXX";
    int fd;

    (void)state;

    fd = mkstemp(path);
    assert_true(fd >= 0);
    close(fd);
    assert_null(strstr(path, "XXXXXX"));
    assert_int_equal(access(path, F_OK), 0);
    assert_int_equal(unlink(path), 0);
}

/* Verify mkstemps keeps the requested suffix unchanged. */
static void test_mkstemps_preserves_suffix(void **state)
{
    char path[] = "zimh-compat-temp-XXXXXX.txt";
    int fd;

    (void)state;

    fd = mkstemps(path, 4);
    assert_true(fd >= 0);
    close(fd);
    assert_true(strlen(path) > 4);
    assert_string_equal(path + strlen(path) - 4, ".txt");
    assert_int_equal(access(path, F_OK), 0);
    assert_int_equal(unlink(path), 0);
}

/* Verify malformed templates fail without creating a file. */
static void test_mkstemps_rejects_bad_template(void **state)
{
    char path[] = "zimh-compat-temp-no-template.txt";

    (void)state;

    assert_int_equal(mkstemps(path, 4), -1);
}

/* Verify invalid arguments fail with EINVAL. */
static void test_mkstemps_rejects_invalid_arguments(void **state)
{
    char short_path[] = "XXX.txt";
    char path[] = "zimh-compat-temp-XXXXXX.txt";

    (void)state;

    errno = 0;
    assert_int_equal(mkstemps(NULL, 0), -1);
    assert_int_equal(errno, EINVAL);

    errno = 0;
    assert_int_equal(mkstemps(path, -1), -1);
    assert_int_equal(errno, EINVAL);

    errno = 0;
    assert_int_equal(mkstemps(short_path, 4), -1);
    assert_int_equal(errno, EINVAL);
}

/* Verify an existing first candidate is skipped rather than overwritten. */
static void test_mkstemp_retries_after_collision(void **state)
{
    char path[] = "zimh-compat-collision-XXXXXX";
    char first_path[sizeof(path)];
    int fd;

    (void)state;

    test_expected_attempt(first_path, sizeof(first_path), path, 0);
    unlink(first_path);

    fd = test_open_exclusive(first_path);
    assert_true(fd >= 0);
    close(fd);

    fd = mkstemp(path);
    assert_true(fd >= 0);
    close(fd);

    assert_string_not_equal(path, first_path);
    assert_int_equal(access(first_path, F_OK), 0);
    assert_int_equal(access(path, F_OK), 0);

    assert_int_equal(unlink(first_path), 0);
    assert_int_equal(unlink(path), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mkstemp_creates_unique_file),
        cmocka_unit_test(test_mkstemps_preserves_suffix),
        cmocka_unit_test(test_mkstemps_rejects_bad_template),
        cmocka_unit_test(test_mkstemps_rejects_invalid_arguments),
        cmocka_unit_test(test_mkstemp_retries_after_collision),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
