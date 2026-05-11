#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "i1401_defs.h"
#include "i1401_dat.h"

DEVICE cpu_dev;
DEVICE inq_dev;
DEVICE lpt_dev;
DEVICE cdr_dev;
DEVICE cdp_dev;
DEVICE stack_dev;
DEVICE dp_dev;
DEVICE mt_dev;
UNIT cpu_unit;
REG cpu_reg[1];
uint8_t M[MAXMEMSIZE];
bool conv_old = false;
int32_t cct[CCT_LNT];
int32_t cctlnt;
int32_t cctptr;
const int32_t op_table[64] = {
    [OP_LCA] = L1 | L4 | L7 | L8 | BREQ | MLS | IO,
};
const int32_t len_table[9] = {0, L1, L2, 0, L4, L5, 0, L7, L8};
const int32_t hun_table[64];
const int32_t ten_table[64];
const int32_t one_table[64];

int32_t store_addr_h(int32_t addr);
int32_t store_addr_t(int32_t addr);
int32_t store_addr_u(int32_t addr);

int32_t store_addr_h(int32_t addr)
{
    return addr;
}

int32_t store_addr_t(int32_t addr)
{
    return addr;
}

int32_t store_addr_u(int32_t addr)
{
    return addr;
}

t_stat sim_instr(void)
{
    return SCPE_OK;
}

#include "i1401_sys.c"

static char *read_stream(FILE *stream)
{
    long size;
    char *buffer;

    assert_int_equal(fflush(stream), 0);
    size = ftell(stream);
    assert_true(size >= 0);
    assert_int_equal(fseek(stream, 0, SEEK_SET), 0);

    buffer = malloc((size_t)size + 1);
    assert_non_null(buffer);
    assert_int_equal(fread(buffer, 1, (size_t)size, stream), (size_t)size);
    buffer[size] = '\0';

    return buffer;
}

static void
test_symbolic_io_operand_uses_only_f_switch_for_fortran_set(void **state)
{
    FILE *stream;
    char *output;
    t_value instruction[] = {
        OP_LCA | WM, BCD_PERCNT, 001, 060, WM,
    };

    (void)state;

    conv_old = false;
    stream = tmpfile();
    assert_non_null(stream);

    assert_int_equal(fprint_sym(stream, 0, instruction, &cpu_unit, SWMASK('M')),
                     -3);
    output = read_stream(stream);

    assert_string_equal(output, "LCA %1&");

    free(output);
    fclose(stream);
}

static void
test_symbolic_io_operand_uses_fortran_set_with_f_switch(void **state)
{
    FILE *stream;
    char *output;
    t_value instruction[] = {
        OP_LCA | WM, BCD_PERCNT, 001, 060, WM,
    };

    (void)state;

    conv_old = false;
    stream = tmpfile();
    assert_non_null(stream);

    assert_int_equal(fprint_sym(stream, 0, instruction, &cpu_unit,
                                SWMASK('M') | SWMASK('F')),
                     -3);
    output = read_stream(stream);

    assert_string_equal(output, "LCA %1+");

    free(output);
    fclose(stream);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_symbolic_io_operand_uses_only_f_switch_for_fortran_set),
        cmocka_unit_test(
            test_symbolic_io_operand_uses_fortran_set_with_f_switch),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
