#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

struct expected_instruction {
    const char *input;
    t_stat status;
    t_value opcode_byte;
    t_value low_operand;
    t_value high_operand;
};

struct operand_pattern {
    t_value low;
    t_value high;
};

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
 * Read back the complete output emitted to a temporary stream.
 */
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

/*
 * Render one i8080 instruction through the simulator's symbolic output API.
 */
static char *render_i8080_symbol(t_value *val, t_stat *status)
{
    FILE *stream;
    char *output;

    stream = tmpfile();
    assert_non_null(stream);

    *status = fprint_sym(stream, 0, val, &i8080_unit, SWMASK('M'));
    output = read_stream(stream);
    assert_int_equal(fclose(stream), 0);

    return output;
}

/*
 * Assert that symbolic machine input parses to exactly the expected bytes and
 * returns the expected instruction-length status.
 */
static void
assert_parse_sym_accepts_instruction(const char *input, t_stat status,
                                     t_value opcode_byte,
                                     t_value low_operand,
                                     t_value high_operand)
{
    t_value val[HIST_ILNT] = {0};

    assert_int_equal(parse_sym(input, 0, &i8080_unit, val, SWMASK('M')),
                     status);
    assert_int_equal(val[0], opcode_byte);
    if (status <= -1)
        assert_int_equal(val[1], low_operand);
    if (status <= -2)
        assert_int_equal(val[2], high_operand);
}

/*
 * Assert a table of valid symbolic instructions.
 */
static void
assert_parse_sym_accepts_instructions(const struct expected_instruction *cases,
                                      size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        assert_parse_sym_accepts_instruction(cases[i].input, cases[i].status,
                                             cases[i].opcode_byte,
                                             cases[i].low_operand,
                                             cases[i].high_operand);
    }
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
 * Assert a table of malformed symbolic inputs.
 */
static void assert_parse_sym_rejects_all(const char *const *inputs,
                                         size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
        assert_parse_sym_rejects_unchanged(inputs[i]);
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
    const struct expected_instruction cases[] = {
        {"NOP", SCPE_OK, 0x00, 0, 0},
    };

    (void)state;

    assert_parse_sym_accepts_instructions(cases,
                                          sizeof(cases) / sizeof(*cases));
}

/*
 * Trailing spaces are ignored while matching mnemonic opcodes.
 */
static void test_parse_sym_trims_trailing_opcode_spaces(void **state)
{
    const struct expected_instruction cases[] = {
        {"NOP   ", SCPE_OK, 0x00, 0, 0},
    };

    (void)state;

    assert_parse_sym_accepts_instructions(cases,
                                          sizeof(cases) / sizeof(*cases));
}

/*
 * Mnemonic matching is case-insensitive for the opcode text.
 */
static void test_parse_sym_accepts_lowercase_mnemonics(void **state)
{
    const struct expected_instruction cases[] = {
        {"nop", SCPE_OK, 0x00, 0, 0},
        {"adi 5a", -1, 0xC6, 0x5A, 0},
        {"jmp 1234", -2, 0xC3, 0x34, 0x12},
    };

    (void)state;

    assert_parse_sym_accepts_instructions(cases,
                                          sizeof(cases) / sizeof(*cases));
}

/*
 * MOV is the one opcode family whose table entries include a comma in the
 * mnemonic itself.
 */
static void test_parse_sym_accepts_mov_mnemonic_with_comma(void **state)
{
    const struct expected_instruction cases[] = {
        {"MOV B,C", SCPE_OK, 0x41, 0, 0},
    };

    (void)state;

    assert_parse_sym_accepts_instructions(cases,
                                          sizeof(cases) / sizeof(*cases));
}

/*
 * RST opcodes include the restart number in the mnemonic text.
 */
static void test_parse_sym_accepts_rst_mnemonic_with_number(void **state)
{
    const struct expected_instruction cases[] = {
        {"RST 0", SCPE_OK, 0xC7, 0, 0},
        {"RST 7", SCPE_OK, 0xFF, 0, 0},
    };

    (void)state;

    assert_parse_sym_accepts_instructions(cases,
                                          sizeof(cases) / sizeof(*cases));
}

/*
 * Symbolic input should accept hex byte operands in the exact form emitted by
 * fprint_sym().
 */
static void test_parse_sym_accepts_hex_byte_operands(void **state)
{
    const struct expected_instruction cases[] = {
        {"MVI A,00", -1, 0x3E, 0x00, 0},
        {"MVI A,5A", -1, 0x3E, 0x5A, 0},
        {"ADI FF", -1, 0xC6, 0xFF, 0},
    };

    (void)state;

    assert_parse_sym_accepts_instructions(cases,
                                          sizeof(cases) / sizeof(*cases));
}

/*
 * Symbolic input should accept hex word operands in the exact form emitted by
 * fprint_sym().
 */
static void test_parse_sym_accepts_hex_word_operands(void **state)
{
    const struct expected_instruction cases[] = {
        {"LXI B,0000", -2, 0x01, 0x00, 0x00},
        {"LXI B,1234", -2, 0x01, 0x34, 0x12},
        {"JMP FFFF", -2, 0xC3, 0xFF, 0xFF},
    };

    (void)state;

    assert_parse_sym_accepts_instructions(cases,
                                          sizeof(cases) / sizeof(*cases));
}

/*
 * MVI L uses the same comma-delimited operand spelling as the other MVI
 * register instructions.
 */
static void test_fprint_sym_mvi_l_uses_comma_operand_delimiter(void **state)
{
    t_stat status;
    t_value val[HIST_ILNT] = {0x2E, 0x5A, 0};
    char *symbol;

    (void)state;

    symbol = render_i8080_symbol(val, &status);
    assert_int_equal(status, -1);
    assert_string_equal(symbol, "MVI L,5A");

    free(symbol);
}

/*
 * MOV operand delimiters should allow normal human spacing around the comma.
 */
static void test_parse_sym_accepts_mov_mnemonic_with_spaced_comma(void **state)
{
    const struct expected_instruction cases[] = {
        {"MOV B, C", SCPE_OK, 0x41, 0, 0},
        {"MOV B ,C", SCPE_OK, 0x41, 0, 0},
        {"MOV B , C", SCPE_OK, 0x41, 0, 0},
    };

    (void)state;

    assert_parse_sym_accepts_instructions(cases,
                                          sizeof(cases) / sizeof(*cases));
}

/*
 * Every valid machine-mode disassembly should parse back to the same opcode
 * and operand bytes across a small boundary-value operand matrix.
 */
static void test_parse_sym_accepts_all_fprint_sym_outputs(void **state)
{
    const struct operand_pattern patterns[] = {
        {0x00, 0x00},
        {0xFF, 0xFF},
        {0x34, 0x12},
        {0x00, 0x80},
        {0xA5, 0x5A},
    };
    int op;

    (void)state;

    for (op = 0; op < 256; op++) {
        size_t pattern;

        if (oplen[op] == 0)
            continue;

        for (pattern = 0; pattern < sizeof(patterns) / sizeof(*patterns);
             pattern++) {
            t_value input[HIST_ILNT] = {0};
            t_value output[HIST_ILNT] = {0};
            t_stat expected_status;
            char *symbol;

            input[0] = (t_value)op;
            input[1] = patterns[pattern].low;
            input[2] = patterns[pattern].high;

            symbol = render_i8080_symbol(input, &expected_status);
            assert_int_equal(parse_sym(symbol, 0, &i8080_unit, output,
                                       SWMASK('M')),
                             expected_status);
            assert_int_equal(output[0], input[0]);
            if (oplen[op] > 1)
                assert_int_equal(output[1], input[1]);
            if (oplen[op] > 2)
                assert_int_equal(output[2], input[2]);
            free(symbol);
        }
    }
}

/*
 * The old parser accidentally accepted some undelimited octal-looking input.
 * Symbolic input now requires the hex form emitted by fprint_sym().
 */
static void test_parse_sym_rejects_legacy_octal_operand_spelling(void **state)
{
    const char *const cases[] = {
        "MVI L377",
    };

    (void)state;

    assert_parse_sym_rejects_all(cases, sizeof(cases) / sizeof(*cases));
}

/*
 * Incomplete MOV mnemonics should reject cleanly instead of reading past the
 * end of the input while looking for the comma and source register.
 */
static void test_parse_sym_rejects_incomplete_mov_mnemonics(void **state)
{
    const char *const cases[] = {
        "MOV",
        "MOV B",
        "MOV B,",
        "MOV B,CX",
    };

    (void)state;

    assert_parse_sym_rejects_all(cases, sizeof(cases) / sizeof(*cases));
}

/*
 * Incomplete or out-of-range RST mnemonics do not identify an opcode.
 */
static void test_parse_sym_rejects_bad_rst_mnemonics(void **state)
{
    const char *const cases[] = {
        "RST",
        "RST ",
        "RST 8",
        "RST 0X",
    };

    (void)state;

    assert_parse_sym_rejects_all(cases, sizeof(cases) / sizeof(*cases));
}

/*
 * Complete one-byte mnemonics should not accept trailing garbage.
 */
static void test_parse_sym_rejects_trailing_opcode_garbage(void **state)
{
    const char *const cases[] = {
        "NOP X",
        "ADD BX",
    };

    (void)state;

    assert_parse_sym_rejects_all(cases, sizeof(cases) / sizeof(*cases));
}

/*
 * Known opcodes that require operands should reject missing operands without
 * modifying the caller's value buffer.
 */
static void test_parse_sym_rejects_missing_operands(void **state)
{
    const char *const cases[] = {
        "MVI L",
        "MVI L,",
    };

    (void)state;

    assert_parse_sym_rejects_all(cases, sizeof(cases) / sizeof(*cases));
}

/*
 * Known opcodes that require operands should reject invalid hex operands
 * without modifying the caller's value buffer.
 */
static void test_parse_sym_rejects_bad_hex_operands(void **state)
{
    const char *const cases[] = {
        "MVI L,GG",
        "MVI L,100",
        "JMP 10000",
        "ADI 5AG",
    };

    (void)state;

    assert_parse_sym_rejects_all(cases, sizeof(cases) / sizeof(*cases));
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
        cmocka_unit_test_setup(test_parse_sym_accepts_hex_byte_operands,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_accepts_hex_word_operands,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(
            test_fprint_sym_mvi_l_uses_comma_operand_delimiter,
            setup_i8080_symbols),
        cmocka_unit_test_setup(
            test_parse_sym_accepts_mov_mnemonic_with_spaced_comma,
            setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_accepts_all_fprint_sym_outputs,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(
            test_parse_sym_rejects_legacy_octal_operand_spelling,
            setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_incomplete_mov_mnemonics,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_bad_rst_mnemonics,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_trailing_opcode_garbage,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_missing_operands,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_bad_hex_operands,
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
