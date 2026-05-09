#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "test_support.h"

static uint8_t register_buffer[4];
static REG test_registers[] = {
    {BRDATA(RBUF, register_buffer, 8, 8, 4)},
    {NULL},
};

/* Verify summarized array output keeps skipped entries at their own value. */
static void test_examine_array_register_summarizes_single_repeat(void **state)
{
    FILE *stream;
    char *text = NULL;
    size_t size = 0;
    t_value eval[1] = {0};
    t_value *saved_eval = sim_eval;

    (void)state;

    sim_eval = eval;
    register_buffer[0] = 0;
    register_buffer[1] = 0;
    register_buffer[2] = 1;
    register_buffer[3] = 0;

    stream = tmpfile();
    assert_non_null(stream);
    assert_int_equal(exdep_reg_loop(stream, NULL, EX_E, NULL,
                                    &test_registers[0], &test_registers[0], 0,
                                    3),
                     SCPE_OK);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_string_equal(text, "RBUF[0]:\t000\n"
                              "RBUF[1]: same as above\n"
                              "RBUF[2]:\t001\n"
                              "RBUF[3]:\t000\n");

    sim_eval = saved_eval;
    fclose(stream);
    free(text);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_examine_array_register_summarizes_single_repeat),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
