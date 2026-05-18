/* test_sim_host_path.c: tests for host path-name discovery helpers

   These tests cover the platform lookup order and fallback behavior for
   reusable path-name answers owned by sim_host_path.c.
*/

#include <errno.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#include "sim_host_path_internal.h"

struct saved_env {
    const char *name;
    char *value;
};

#if defined(_WIN32)
static unsigned long win32_temp_path2_calls = 0;
static unsigned long win32_temp_path_calls = 0;

static unsigned long win32_temp_path2_success(unsigned long size, char *buf)
{
    const char *path = "C:\\Windows\\SystemTemp\\";

    ++win32_temp_path2_calls;
    if (strlen(path) + 1 > size)
        return (unsigned long)(strlen(path) + 1);
    strlcpy(buf, path, size);
    return (unsigned long)strlen(path);
}

static unsigned long win32_temp_path_success(unsigned long size, char *buf)
{
    const char *path = "C:\\Users\\Test\\AppData\\Local\\Temp\\";

    ++win32_temp_path_calls;
    if (strlen(path) + 1 > size)
        return (unsigned long)(strlen(path) + 1);
    strlcpy(buf, path, size);
    return (unsigned long)strlen(path);
}

static unsigned long win32_temp_path_fail(unsigned long size, char *buf)
{
    (void)size;
    (void)buf;

    ++win32_temp_path_calls;
    return 0;
}

static unsigned long win32_temp_path2_fail(unsigned long size, char *buf)
{
    (void)size;
    (void)buf;

    ++win32_temp_path2_calls;
    return 0;
}
#endif

static struct saved_env save_env(const char *name)
{
    const char *value = getenv(name);
    struct saved_env saved = {name, value == NULL ? NULL : strdup(value)};

    return saved;
}

static void restore_env(struct saved_env *saved)
{
    if (saved->value != NULL) {
        assert_int_equal(setenv(saved->name, saved->value, 1), 0);
        free(saved->value);
        saved->value = NULL;
    } else
        unsetenv(saved->name);
}

static void test_sim_host_temp_dir_rejects_invalid_buffer(void **state)
{
    char buf[32];

    (void)state;

    assert_null(sim_host_temp_dir(NULL, sizeof(buf)));
    assert_null(sim_host_temp_dir(buf, 0));
}

static void test_sim_host_temp_dir_rejects_short_buffer(void **state)
{
    char buf[4];

    (void)state;

    assert_null(sim_host_temp_dir(buf, sizeof(buf)));
}

static void test_sim_normalize_host_path_rejects_invalid_buffer(void **state)
{
    char buf[32];

    (void)state;

    assert_null(sim_normalize_host_path(NULL, buf, sizeof(buf)));
    assert_int_equal(errno, EINVAL);
    assert_null(sim_normalize_host_path("file", NULL, sizeof(buf)));
    assert_int_equal(errno, EINVAL);
    assert_null(sim_normalize_host_path("file", buf, 0));
    assert_int_equal(errno, EINVAL);
}

static void test_sim_normalize_host_path_rejects_short_buffer(void **state)
{
    char buf[4];

    (void)state;

    assert_null(sim_normalize_host_path("too-long", buf, sizeof(buf)));
    assert_int_equal(errno, ENAMETOOLONG);
}

static void test_sim_normalize_host_path_preserves_backslashes(void **state)
{
    char buf[64];

    (void)state;

    assert_string_equal(
        sim_normalize_host_path("\"literal\\tpath.bin\"", buf, sizeof(buf)),
        "literal\\tpath.bin");
}

static void test_sim_normalize_host_path_rejects_malformed_quotes(void **state)
{
    static const char *const bad_paths[] = {
        "\"",
        "\"foo'",
        "\"foo\"bar\"",
        "'foo'bar'",
    };
    char buf[64];
    size_t i;

    (void)state;

    for (i = 0; i < sizeof(bad_paths) / sizeof(bad_paths[0]); ++i) {
        assert_null(sim_normalize_host_path(bad_paths[i], buf, sizeof(buf)));
        assert_int_equal(errno, EINVAL);
    }
}

static void test_sim_normalize_host_path_accepts_unquoted_quotes(void **state)
{
    char buf[64];

    (void)state;

    assert_string_equal(
        sim_normalize_host_path("literal'quote", buf, sizeof(buf)),
        "literal'quote");
    assert_string_equal(
        sim_normalize_host_path("literal\"quote", buf, sizeof(buf)),
        "literal\"quote");
}

static void test_sim_normalize_host_path_accepts_opposite_quotes(void **state)
{
    char buf[64];

    (void)state;

    assert_string_equal(
        sim_normalize_host_path("'literalquote\"'", buf, sizeof(buf)),
        "literalquote\"");
    assert_string_equal(
        sim_normalize_host_path("\"literalquote'\"", buf, sizeof(buf)),
        "literalquote'");
}

#if !defined(_WIN32)
static void test_sim_normalize_host_path_expands_posix_home(void **state)
{
    char buf[128];
    struct saved_env saved_home = save_env("HOME");

    (void)state;

    assert_int_equal(setenv("HOME", "/home/zimh", 1), 0);
    assert_string_equal(
        sim_normalize_host_path("\"~/disk images/a.dsk\"", buf, sizeof(buf)),
        "/home/zimh/disk images/a.dsk");

    restore_env(&saved_home);
}

static void test_sim_host_temp_dir_uses_posix_environment_order(void **state)
{
    char buf[64];
    struct saved_env saved_tmpdir = save_env("TMPDIR");
    struct saved_env saved_tmp = save_env("TMP");
    struct saved_env saved_temp = save_env("TEMP");

    (void)state;

    assert_int_equal(setenv("TMPDIR", "/zimh/tmpdir", 1), 0);
    assert_int_equal(setenv("TMP", "/zimh/tmp", 1), 0);
    assert_int_equal(setenv("TEMP", "/zimh/temp", 1), 0);
    assert_string_equal(sim_host_temp_dir(buf, sizeof(buf)), "/zimh/tmpdir");

    unsetenv("TMPDIR");
    assert_string_equal(sim_host_temp_dir(buf, sizeof(buf)), "/zimh/tmp");

    unsetenv("TMP");
    assert_string_equal(sim_host_temp_dir(buf, sizeof(buf)), "/zimh/temp");

    restore_env(&saved_tmpdir);
    restore_env(&saved_tmp);
    restore_env(&saved_temp);
}

static void test_sim_host_temp_dir_rejects_long_posix_environment(void **state)
{
    char buf[8];
    struct saved_env saved_tmpdir = save_env("TMPDIR");
    struct saved_env saved_tmp = save_env("TMP");
    struct saved_env saved_temp = save_env("TEMP");

    (void)state;

    assert_int_equal(setenv("TMPDIR", "/too/long/for/buffer", 1), 0);
    unsetenv("TMP");
    unsetenv("TEMP");

    assert_null(sim_host_temp_dir(buf, sizeof(buf)));

    restore_env(&saved_tmpdir);
    restore_env(&saved_tmp);
    restore_env(&saved_temp);
}

static void test_sim_host_temp_dir_uses_posix_default(void **state)
{
    char buf[64];
    struct saved_env saved_tmpdir = save_env("TMPDIR");
    struct saved_env saved_tmp = save_env("TMP");
    struct saved_env saved_temp = save_env("TEMP");

    (void)state;

    unsetenv("TMPDIR");
    unsetenv("TMP");
    unsetenv("TEMP");

    assert_string_equal(sim_host_temp_dir(buf, sizeof(buf)), "/tmp");

    restore_env(&saved_tmpdir);
    restore_env(&saved_tmp);
    restore_env(&saved_temp);
}
#else
static void test_sim_normalize_host_path_expands_windows_home(void **state)
{
    char buf[128];
    struct saved_env saved_home = save_env("HOME");
    struct saved_env saved_homedrive = save_env("HOMEDRIVE");
    struct saved_env saved_homepath = save_env("HOMEPATH");

    (void)state;

    unsetenv("HOME");
    assert_int_equal(setenv("HOMEDRIVE", "C:", 1), 0);
    assert_int_equal(setenv("HOMEPATH", "\\Users\\zimh", 1), 0);
    assert_string_equal(
        sim_normalize_host_path("\"~/disk images/a.dsk\"", buf, sizeof(buf)),
        "C:\\Users\\zimh\\disk images\\a.dsk");

    restore_env(&saved_home);
    restore_env(&saved_homedrive);
    restore_env(&saved_homepath);
}

static void test_sim_host_temp_dir_prefers_get_temp_path2(void **state)
{
    char buf[128];

    (void)state;

    win32_temp_path2_calls = 0;
    win32_temp_path_calls = 0;
    sim_host_path_set_test_win32_temp_hooks(win32_temp_path2_success,
                                            win32_temp_path_success);

    assert_string_equal(sim_host_temp_dir(buf, sizeof(buf)),
                        "C:\\Windows\\SystemTemp\\");
    assert_int_equal(win32_temp_path2_calls, 1);
    assert_int_equal(win32_temp_path_calls, 0);

    sim_host_path_reset_test_hooks();
}

static void test_sim_host_temp_dir_falls_back_to_get_temp_path(void **state)
{
    char buf[128];

    (void)state;

    win32_temp_path2_calls = 0;
    win32_temp_path_calls = 0;
    sim_host_path_set_test_win32_temp_hooks(win32_temp_path2_fail,
                                            win32_temp_path_success);

    assert_string_equal(sim_host_temp_dir(buf, sizeof(buf)),
                        "C:\\Users\\Test\\AppData\\Local\\Temp\\");
    assert_int_equal(win32_temp_path2_calls, 1);
    assert_int_equal(win32_temp_path_calls, 1);

    sim_host_path_reset_test_hooks();
}

static void test_sim_host_temp_dir_falls_back_to_windows_env(void **state)
{
    char buf[128];
    struct saved_env saved_tmp = save_env("TMP");
    struct saved_env saved_temp = save_env("TEMP");
    struct saved_env saved_userprofile = save_env("USERPROFILE");

    (void)state;

    sim_host_path_set_test_win32_temp_hooks(win32_temp_path2_fail,
                                            win32_temp_path_fail);
    assert_int_equal(setenv("TMP", "C:\\zimh\\tmp", 1), 0);
    assert_int_equal(setenv("TEMP", "C:\\zimh\\temp", 1), 0);
    assert_int_equal(setenv("USERPROFILE", "C:\\Users\\zimh", 1), 0);

    assert_string_equal(sim_host_temp_dir(buf, sizeof(buf)), "C:\\zimh\\tmp");

    sim_host_path_reset_test_hooks();
    restore_env(&saved_tmp);
    restore_env(&saved_temp);
    restore_env(&saved_userprofile);
}
#endif

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sim_host_temp_dir_rejects_invalid_buffer),
        cmocka_unit_test(test_sim_host_temp_dir_rejects_short_buffer),
        cmocka_unit_test(test_sim_normalize_host_path_rejects_invalid_buffer),
        cmocka_unit_test(test_sim_normalize_host_path_rejects_short_buffer),
        cmocka_unit_test(test_sim_normalize_host_path_preserves_backslashes),
        cmocka_unit_test(test_sim_normalize_host_path_rejects_malformed_quotes),
        cmocka_unit_test(test_sim_normalize_host_path_accepts_unquoted_quotes),
        cmocka_unit_test(test_sim_normalize_host_path_accepts_opposite_quotes),
#if !defined(_WIN32)
        cmocka_unit_test(test_sim_normalize_host_path_expands_posix_home),
        cmocka_unit_test(test_sim_host_temp_dir_uses_posix_environment_order),
        cmocka_unit_test(
            test_sim_host_temp_dir_rejects_long_posix_environment),
        cmocka_unit_test(test_sim_host_temp_dir_uses_posix_default),
#else
        cmocka_unit_test(test_sim_normalize_host_path_expands_windows_home),
        cmocka_unit_test(test_sim_host_temp_dir_prefers_get_temp_path2),
        cmocka_unit_test(test_sim_host_temp_dir_falls_back_to_get_temp_path),
        cmocka_unit_test(test_sim_host_temp_dir_falls_back_to_windows_env),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
