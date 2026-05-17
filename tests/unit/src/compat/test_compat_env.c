#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_win32_compat.h"

static const char *const simh_test_env_name = "ZIMH_TEST_COMPAT_ENV";

static int setup_compat_env(void **state)
{
    (void)state;
    unsetenv(simh_test_env_name);
    return 0;
}

static int teardown_compat_env(void **state)
{
    (void)state;
    unsetenv(simh_test_env_name);
    return 0;
}

/* Verify setenv creates a missing environment variable. */
static void test_setenv_creates_variable(void **state)
{
    (void)state;

    assert_int_equal(setenv(simh_test_env_name, "alpha", 1), 0);
    assert_string_equal(getenv(simh_test_env_name), "alpha");
}

/* Verify overwrite=0 preserves an existing environment value. */
static void test_setenv_respects_no_overwrite(void **state)
{
    (void)state;

    assert_int_equal(setenv(simh_test_env_name, "alpha", 1), 0);
    assert_int_equal(setenv(simh_test_env_name, "beta", 0), 0);
    assert_string_equal(getenv(simh_test_env_name), "alpha");
}

/* Verify setenv preserves an empty value as a present variable. */
static void test_setenv_preserves_empty_value(void **state)
{
    const char *value;

    (void)state;

    assert_int_equal(setenv(simh_test_env_name, "", 1), 0);
    value = getenv(simh_test_env_name);
    assert_non_null(value);
    assert_string_equal(value, "");
}

/* Verify unsetenv removes a value that was previously installed. */
static void test_unsetenv_removes_variable(void **state)
{
    (void)state;

    assert_int_equal(setenv(simh_test_env_name, "alpha", 1), 0);
    assert_non_null(getenv(simh_test_env_name));
    assert_int_equal(unsetenv(simh_test_env_name), 0);
    assert_null(getenv(simh_test_env_name));
}

/* Verify unsetenv removes a present empty value. */
static void test_unsetenv_removes_empty_variable(void **state)
{
    (void)state;

    assert_int_equal(setenv(simh_test_env_name, "", 1), 0);
    assert_non_null(getenv(simh_test_env_name));
    assert_int_equal(unsetenv(simh_test_env_name), 0);
    assert_null(getenv(simh_test_env_name));
}

/* Verify invalid names are rejected instead of mutating the environment. */
static void test_env_shims_reject_invalid_names(void **state)
{
    (void)state;

    errno = 0;
    assert_int_not_equal(setenv(NULL, "alpha", 1), 0);
    assert_int_equal(errno, EINVAL);
    errno = 0;
    assert_int_not_equal(setenv(simh_test_env_name, NULL, 1), 0);
    assert_int_equal(errno, EINVAL);
    errno = 0;
    assert_int_not_equal(setenv("", "alpha", 1), 0);
    assert_int_equal(errno, EINVAL);
    errno = 0;
    assert_int_not_equal(setenv("BAD=NAME", "alpha", 1), 0);
    assert_int_equal(errno, EINVAL);
    errno = 0;
    assert_int_not_equal(unsetenv(NULL), 0);
    assert_int_equal(errno, EINVAL);
    errno = 0;
    assert_int_not_equal(unsetenv(""), 0);
    assert_int_equal(errno, EINVAL);
    errno = 0;
    assert_int_not_equal(unsetenv("BAD=NAME"), 0);
    assert_int_equal(errno, EINVAL);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_setenv_creates_variable,
                                        setup_compat_env, teardown_compat_env),
        cmocka_unit_test_setup_teardown(test_setenv_respects_no_overwrite,
                                        setup_compat_env, teardown_compat_env),
        cmocka_unit_test_setup_teardown(test_setenv_preserves_empty_value,
                                        setup_compat_env, teardown_compat_env),
        cmocka_unit_test_setup_teardown(test_unsetenv_removes_variable,
                                        setup_compat_env, teardown_compat_env),
        cmocka_unit_test_setup_teardown(test_unsetenv_removes_empty_variable,
                                        setup_compat_env, teardown_compat_env),
        cmocka_unit_test_setup_teardown(test_env_shims_reject_invalid_names,
                                        setup_compat_env, teardown_compat_env),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
