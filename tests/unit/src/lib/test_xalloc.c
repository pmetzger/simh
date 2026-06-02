#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "test_cmocka.h"

#include "xalloc.h"

static const char *test_program_path;

/* Verify xmalloc returns usable storage for nonzero requests. */
static void test_xmalloc_allocates_nonzero_size(void **state)
{
    char *ptr;

    (void)state;

    ptr = (char *)xmalloc(8);
    memset(ptr, 'a', 8);
    free(ptr);
}

/* Verify xcalloc zero-fills the requested element array. */
static void test_xcalloc_zero_fills_all_elements(void **state)
{
    uint32_t *values;

    (void)state;

    values = (uint32_t *)xcalloc(4, sizeof(*values));
    assert_int_equal(values[0], 0);
    assert_int_equal(values[1], 0);
    assert_int_equal(values[2], 0);
    assert_int_equal(values[3], 0);
    free(values);
}

/* Verify xrealloc preserves existing data while growing storage. */
static void test_xrealloc_grows_and_preserves_data(void **state)
{
    char *ptr;

    (void)state;

    ptr = (char *)xmalloc(6);
    memcpy(ptr, "alpha", 6);
    ptr = (char *)xrealloc(ptr, 32);
    assert_string_equal(ptr, "alpha");
    free(ptr);
}

/* Verify xstrdup returns an owned copy of the source string. */
static void test_xstrdup_duplicates_full_string(void **state)
{
    char *copy;

    (void)state;

    copy = xstrdup("frontpanel");
    assert_string_equal(copy, "frontpanel");
    assert_ptr_not_equal(copy, "frontpanel");
    free(copy);
}

/* Verify xstrndup truncates long input and accepts a zero bound. */
static void test_xstrndup_respects_bound(void **state)
{
    char *copy;
    char *empty;

    (void)state;

    copy = xstrndup("frontpanel", 5);
    assert_string_equal(copy, "front");
    free(copy);

    empty = xstrndup("frontpanel", 0);
    assert_string_equal(empty, "");
    free(empty);
}

static void abort_xmalloc_zero(void)
{
    (void)xmalloc(0);
}

static void abort_xcalloc_zero_count(void)
{
    (void)xcalloc(0, 1);
}

static void abort_xcalloc_overflow(void)
{
    (void)xcalloc(SIZE_MAX, 2);
}

static void abort_xrealloc_zero(void)
{
    void *ptr = malloc(1);

    assert_non_null(ptr);
    (void)xrealloc(ptr, 0);
}

static void abort_xstrdup_null(void)
{
    (void)xstrdup(NULL);
}

static void abort_xstrndup_null(void)
{
    (void)xstrndup(NULL, 1);
}

static int run_abort_case(const char *name)
{
    if (strcmp(name, "xmalloc-zero") == 0)
        abort_xmalloc_zero();
    if (strcmp(name, "xcalloc-zero-count") == 0)
        abort_xcalloc_zero_count();
    if (strcmp(name, "xcalloc-overflow") == 0)
        abort_xcalloc_overflow();
    if (strcmp(name, "xrealloc-zero") == 0)
        abort_xrealloc_zero();
    if (strcmp(name, "xstrdup-null") == 0)
        abort_xstrdup_null();
    if (strcmp(name, "xstrndup-null") == 0)
        abort_xstrndup_null();
    return 2;
}

#if !defined(_WIN32)
static void exit_on_sigabrt(int signo)
{
    _exit(128 + signo);
}
#endif

/* Verify one allocation contract violation aborts in a child process. */
static void assert_abort_case(const char *name)
{
#if defined(_WIN32)
    char *const args[] = {(char *)test_program_path, "--expect-abort",
                          (char *)name, NULL};
    intptr_t status    = _spawnv(_P_WAIT, test_program_path, args);

    assert_int_not_equal(status, -1);
    assert_int_not_equal(status, 0);
#else
    pid_t pid = fork();
    int status;

    assert_true(pid >= 0);
    if (pid == 0) {
        signal(SIGABRT, exit_on_sigabrt);
        (void)run_abort_case(name);
        _exit(0);
    }
    assert_int_equal(waitpid(pid, &status, 0), pid);
    assert_true(WIFEXITED(status));
    assert_int_equal(WEXITSTATUS(status), 128 + SIGABRT);
#endif
}

/* Verify contract violations abort instead of returning ambiguous pointers. */
static void test_xalloc_contract_violations_abort(void **state)
{
    (void)state;

    assert_abort_case("xmalloc-zero");
    assert_abort_case("xcalloc-zero-count");
    assert_abort_case("xcalloc-overflow");
    assert_abort_case("xrealloc-zero");
    assert_abort_case("xstrdup-null");
    assert_abort_case("xstrndup-null");
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_xmalloc_allocates_nonzero_size),
        cmocka_unit_test(test_xcalloc_zero_fills_all_elements),
        cmocka_unit_test(test_xrealloc_grows_and_preserves_data),
        cmocka_unit_test(test_xstrdup_duplicates_full_string),
        cmocka_unit_test(test_xstrndup_respects_bound),
        cmocka_unit_test(test_xalloc_contract_violations_abort),
    };

    test_program_path = argv[0];
    if ((argc == 3) && (strcmp(argv[1], "--expect-abort") == 0))
        return run_abort_case(argv[2]);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
