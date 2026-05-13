#include <string.h>

#include "test_cmocka.h"

#include "ibm1130_fmt.h"

static void test_fortran_edit_limits_argument_field(void **state)
{
    char source[160];
    const char *formatted;

    (void)state;

    memset(source, 'A', sizeof(source));
    memcpy(source, "12345\t", 6);
    source[sizeof(source) - 1] = '\0';

    formatted = EditToFortran(source, 0);

    assert_int_equal(strlen(formatted), 80);
    assert_memory_equal(formatted, "12345 ", 6);
    assert_int_equal(formatted[79], 'A');
}

static void test_whitespace_edit_truncates_long_input_safely(void **state)
{
    char source[160];
    const char *formatted;

    (void)state;

    memset(source, 'B', sizeof(source));
    source[sizeof(source) - 1] = '\0';

    formatted = EditToWhitespace(source, 0);

    assert_int_equal(strlen(formatted), 80);
    assert_int_equal(formatted[79], 'B');
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_fortran_edit_limits_argument_field),
        cmocka_unit_test(test_whitespace_edit_truncates_long_input_safely),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
