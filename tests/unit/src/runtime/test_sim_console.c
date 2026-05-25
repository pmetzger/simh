#include <errno.h>
#include <stddef.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_console.h"
#include "sim_console_internal.h"

static void test_sim_fd_isatty_rejects_invalid_fd(void **state)
{
    (void)state;

    assert_false(sim_fd_isatty(-1));
}

/* Verify no-ready zero-byte reads remain ordinary no-input polls. */
static void test_console_read_zero_without_ready_is_no_input(void **state)
{
    (void)state;

    assert_int_equal(
        sim_console_classify_terminal_read(false, 0, 0), SCPE_OK);
}

/* Verify a successful one-byte terminal read is not a disconnect. */
static void test_console_read_one_byte_is_no_error(void **state)
{
    (void)state;

    assert_int_equal(
        sim_console_classify_terminal_read(true, 1, 0), SCPE_OK);
}

/* Verify ready zero-byte reads are treated as a disconnected console. */
static void test_console_read_zero_after_ready_exits(void **state)
{
    (void)state;

    assert_int_equal(
        sim_console_classify_terminal_read(true, 0, 0), SCPE_EXIT);
}

/* Verify transient nonblocking read errors remain ordinary no-input polls. */
static void test_console_read_transient_errors_are_no_input(void **state)
{
    (void)state;

#if defined(EAGAIN) && defined(EWOULDBLOCK) && defined(EINTR)
    assert_int_equal(
        sim_console_classify_terminal_read(true, -1, EAGAIN), SCPE_OK);
    assert_int_equal(
        sim_console_classify_terminal_read(true, -1, EWOULDBLOCK), SCPE_OK);
    assert_int_equal(
        sim_console_classify_terminal_read(true, -1, EINTR), SCPE_OK);
#endif
}

/* Verify real terminal read errors cause the simulator to exit. */
static void test_console_read_real_errors_exit(void **state)
{
    (void)state;

    assert_int_equal(
        sim_console_classify_terminal_read(true, -1, 12345), SCPE_EXIT);
    assert_int_equal(
        sim_console_classify_terminal_read(false, -1, 12345), SCPE_EXIT);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sim_fd_isatty_rejects_invalid_fd),
        cmocka_unit_test(test_console_read_zero_without_ready_is_no_input),
        cmocka_unit_test(test_console_read_one_byte_is_no_error),
        cmocka_unit_test(test_console_read_zero_after_ready_exits),
        cmocka_unit_test(test_console_read_transient_errors_are_no_input),
        cmocka_unit_test(test_console_read_real_errors_exit),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
