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
 * Assert that malformed symbolic input is rejected before modifying the
 * caller's value buffer.
 */
static void assert_parse_sym_rejects_unchanged(const char *input)
{
    t_value val[HIST_ILNT] = {0xAA, 0xBB, 0xCC};

    assert_int_equal(parse_sym(input, 0, &i8080_unit, val, SWMASK('M')),
                     SCPE_ARG);
    assert_int_equal(val[0], 0xAA);
    assert_int_equal(val[1], 0xBB);
    assert_int_equal(val[2], 0xCC);
}

/*
 * Numeric deposit input is not an instruction mnemonic, so parse_sym() must
 * return SCPE_ARG and leave numeric parsing to the generic deposit path.
 */
static void
test_parse_sym_rejects_numeric_input_for_generic_deposit(void **state)
{
    (void)state;

    assert_parse_sym_rejects_unchanged("00");
}

/*
 * Empty symbolic input is also not an instruction mnemonic. It should be
 * rejected without disturbing the caller's value buffer.
 */
static void test_parse_sym_rejects_empty_input(void **state)
{
    (void)state;

    assert_parse_sym_rejects_unchanged("");
}

/*
 * One-byte symbolic instructions store just the opcode byte and return
 * SCPE_OK.
 */
static void test_parse_sym_accepts_one_byte_mnemonics(void **state)
{
    t_value val[HIST_ILNT] = {0};

    (void)state;

    assert_int_equal(parse_sym("NOP", 0, &i8080_unit, val, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(val[0], 0x00);
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

/*
 * Mnemonic matching is case-insensitive for the opcode text.
 */
static void test_parse_sym_accepts_lowercase_mnemonics(void **state)
{
    t_value val[HIST_ILNT] = {0};

    (void)state;

    assert_int_equal(parse_sym("nop", 0, &i8080_unit, val, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(val[0], 0x00);
}

/*
 * MOV is the one opcode family whose table entries include a comma in the
 * mnemonic itself.
 */
static void test_parse_sym_accepts_mov_mnemonic_with_comma(void **state)
{
    t_value val[HIST_ILNT] = {0};

    (void)state;

    assert_int_equal(parse_sym("MOV B,C", 0, &i8080_unit, val, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(val[0], 0x41);
}

/*
 * RST opcodes include the restart number in the mnemonic text.
 */
static void test_parse_sym_accepts_rst_mnemonic_with_number(void **state)
{
    t_value val[HIST_ILNT] = {0};

    (void)state;

    assert_int_equal(parse_sym("RST 7", 0, &i8080_unit, val, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(val[0], 0xFF);
}

/*
 * Two-byte instructions return -1 and store the parsed 8-bit operand.
 * Existing i8080 symbolic input treats reachable numeric operands as octal.
 */
static void test_parse_sym_accepts_two_byte_octal_operand(void **state)
{
    t_value val[HIST_ILNT] = {0};

    (void)state;

    assert_int_equal(parse_sym("MVI L377", 0, &i8080_unit, val, SWMASK('M')),
                     -1);
    assert_int_equal(val[0], 0x2E);
    assert_int_equal(val[1], 0xFF);
}

/*
 * Incomplete MOV mnemonics should reject cleanly instead of reading past the
 * end of the input while looking for the comma and source register.
 */
static void test_parse_sym_rejects_incomplete_mov_mnemonics(void **state)
{
    (void)state;

    assert_parse_sym_rejects_unchanged("MOV");
    assert_parse_sym_rejects_unchanged("MOV B");
    assert_parse_sym_rejects_unchanged("MOV B,");
}

/*
 * Incomplete RST mnemonics do not identify an opcode.
 */
static void test_parse_sym_rejects_incomplete_rst_mnemonics(void **state)
{
    (void)state;

    assert_parse_sym_rejects_unchanged("RST");
    assert_parse_sym_rejects_unchanged("RST ");
}

/*
 * Known opcodes that require operands should reject missing operands without
 * modifying the caller's value buffer.
 */
static void test_parse_sym_rejects_missing_operands(void **state)
{
    (void)state;

    assert_parse_sym_rejects_unchanged("MVI L");
}

/*
 * Known opcodes that require operands should reject non-octal operands
 * without modifying the caller's value buffer.
 */
static void test_parse_sym_rejects_bad_octal_operands(void **state)
{
    (void)state;

    assert_parse_sym_rejects_unchanged("MVI L8");
}

/*
 * Non-ASCII bytes are not valid i8080 mnemonics, but the parser still must
 * pass them to ctype routines safely.
 */
static void test_parse_sym_rejects_high_bit_input(void **state)
{
    char input[] = {(char)0x80, '\0'};

    (void)state;

    assert_parse_sym_rejects_unchanged(input);
}

/*
 * ASCII character input stores the host character as an unsigned simulated
 * byte, not as a sign-extended host char.
 */
static void test_parse_sym_ascii_char_uses_unsigned_byte(void **state)
{
    char input[] = {(char)0x80, '\0'};
    t_value val[HIST_ILNT] = {0};

    (void)state;

    assert_int_equal(parse_sym(input, 0, &i8080_unit, val, SWMASK('A')),
                     SCPE_OK);
    assert_int_equal(val[0], 0x80);
}

/*
 * ASCII string input stores two host characters as unsigned simulated bytes.
 */
static void test_parse_sym_ascii_string_uses_unsigned_bytes(void **state)
{
    char input[] = {(char)0x80, (char)0x81, '\0'};
    t_value val[HIST_ILNT] = {0};

    (void)state;

    assert_int_equal(parse_sym(input, 0, &i8080_unit, val, SWMASK('C')),
                     SCPE_OK);
    assert_int_equal(val[0], 0x8081);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(
            test_parse_sym_rejects_numeric_input_for_generic_deposit,
            setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_empty_input,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_accepts_one_byte_mnemonics,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_trims_trailing_opcode_spaces,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_accepts_lowercase_mnemonics,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_accepts_mov_mnemonic_with_comma,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_accepts_rst_mnemonic_with_number,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_accepts_two_byte_octal_operand,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_incomplete_mov_mnemonics,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_incomplete_rst_mnemonics,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_missing_operands,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_bad_octal_operands,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_high_bit_input,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_ascii_char_uses_unsigned_byte,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_ascii_string_uses_unsigned_bytes,
                               setup_i8080_symbols),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
