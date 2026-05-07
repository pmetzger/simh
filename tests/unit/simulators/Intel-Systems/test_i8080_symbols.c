#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "system_defs.h"

#include "i8080.c"

static uint8 test_memory[MAXMEMSIZE + 1];

REG *sim_PC = &i8080_reg[0];
DEVICE *sim_devices[] = {&i8080_dev, NULL};
char sim_name[] = "Intel-MDS";
const char *sim_stop_messages[SCPE_BASE];
int32 sim_emax = 1;

struct idev dev_table[256];

/*
 * Satisfy the simulator core's loader callback for symbol-only tests.
 */
t_stat sim_load(FILE *fileref, const char *cptr, const char *fnam, int flag)
{
    (void)fileref;
    (void)cptr;
    (void)fnam;
    (void)flag;

    return SCPE_OK;
}

uint8 get_mbyte(uint16 addr)
{
    return test_memory[addr];
}

uint16 get_mword(uint16 addr)
{
    return (uint16)(test_memory[addr] |
                    (test_memory[(addr + 1) & ADDRMASK] << 8));
}

void put_mbyte(uint16 addr, uint8 val)
{
    test_memory[addr] = val;
}

void put_mword(uint16 addr, uint16 val)
{
    put_mbyte(addr, (uint8)(val & BYTEMASK));
    put_mbyte((uint16)((addr + 1) & ADDRMASK), (uint8)(val >> 8));
}

static int setup_i8080_symbols(void **state)
{
    (void)state;

    memset(test_memory, 0, sizeof(test_memory));
    memset(dev_table, 0, sizeof(dev_table));
    i8080_unit.flags = 0;
    i8080_unit.capac = MAXMEMSIZE;

    return 0;
}

/*
 * Numeric deposit input is not an instruction mnemonic, so parse_sym() must
 * return SCPE_ARG and leave numeric parsing to the generic deposit path.
 */
static void
test_parse_sym_rejects_numeric_input_for_generic_deposit(void **state)
{
    t_value val[HIST_ILNT] = {0xAA, 0xBB, 0xCC};

    (void)state;

    assert_int_equal(parse_sym("00", 0, &i8080_unit, val, SWMASK('M')),
                     SCPE_ARG);
    assert_int_equal(val[0], 0xAA);
    assert_int_equal(val[1], 0xBB);
    assert_int_equal(val[2], 0xCC);
}

/*
 * Empty symbolic input is also not an instruction mnemonic. It should be
 * rejected without disturbing the caller's value buffer.
 */
static void test_parse_sym_rejects_empty_input(void **state)
{
    t_value val[HIST_ILNT] = {0xAA, 0xBB, 0xCC};

    (void)state;

    assert_int_equal(parse_sym("", 0, &i8080_unit, val, SWMASK('M')), SCPE_ARG);
    assert_int_equal(val[0], 0xAA);
    assert_int_equal(val[1], 0xBB);
    assert_int_equal(val[2], 0xCC);
}

/*
 * Trailing spaces are ignored while matching mnemonic opcodes.
 */
static void test_parse_sym_trims_trailing_opcode_spaces(void **state)
{
    t_value val[HIST_ILNT] = {0};

    (void)state;

    assert_int_equal(parse_sym("NOP   ", 0, &i8080_unit, val, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(val[0], 0x00);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(
            test_parse_sym_rejects_numeric_input_for_generic_deposit,
            setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_empty_input,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_trims_trailing_opcode_spaces,
                               setup_i8080_symbols),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
