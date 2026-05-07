#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "system_defs.h"
#include "i8080_symbol_internal.h"

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

struct expected_symbol {
    t_value opcode_byte;
    t_value low_operand;
    t_value high_operand;
    t_stat status;
    const char *symbol;
};

REG *sim_PC = NULL;
DEVICE *sim_devices[] = {NULL};
char sim_name[] = "Intel-MDS";
const char *sim_stop_messages[SCPE_BASE];
int32 sim_emax = 1;

static UNIT symbol_unit;

/*
 * Satisfy the simulator core's execution callback for symbol-only tests.
 */
t_stat sim_instr(void)
{
    return SCPE_OK;
}

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

static int setup_i8080_symbols(void **state)
{
    (void)state;
    symbol_unit = (UNIT){0};

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

    *status = fprint_sym(stream, 0, val, &symbol_unit, SWMASK('M'));
    output = read_stream(stream);
    assert_int_equal(fclose(stream), 0);

    return output;
}

/*
 * Assert that one opcode formats to the expected machine-mode symbol.
 */
static void assert_fprint_sym_formats_instruction(
    const struct expected_symbol *expected)
{
    t_stat status;
    t_value val[I8080_SYMBOL_WORDS] = {
        expected->opcode_byte,
        expected->low_operand,
        expected->high_operand,
    };
    char *symbol;

    symbol = render_i8080_symbol(val, &status);
    assert_int_equal(status, expected->status);
    assert_string_equal(symbol, expected->symbol);

    free(symbol);
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
    t_value val[I8080_SYMBOL_WORDS] = {0};

    assert_int_equal(parse_sym(input, 0, &symbol_unit, val, SWMASK('M')),
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
    t_value val[I8080_SYMBOL_WORDS] = {0xAA, 0xBB, 0xCC};

    assert_int_equal(parse_sym(input, 0, &symbol_unit, val, SWMASK('M')),
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
 * Assert that the opcode length table still describes the documented i8080
 * opcode shape independently of the symbolic parser implementation.
 */
static void test_opcode_table_has_expected_instruction_lengths(void **state)
{
    static const uint8 expected_lengths[256] = {
        1, 3, 1, 1, 1, 1, 2, 1, 0, 1, 1, 1, 1, 1, 2, 1,
        0, 3, 1, 1, 1, 1, 2, 1, 0, 1, 1, 1, 1, 1, 2, 1,
        1, 3, 3, 1, 1, 1, 2, 1, 0, 1, 3, 1, 1, 1, 2, 1,
        1, 3, 3, 1, 1, 1, 2, 1, 0, 1, 3, 1, 1, 1, 2, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 3, 3, 3, 1, 2, 1, 1, 1, 3, 0, 3, 3, 2, 1,
        1, 1, 3, 2, 3, 1, 2, 1, 1, 0, 3, 2, 3, 0, 2, 1,
        1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 0, 2, 1,
        1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 0, 2, 1,
    };
    size_t op;

    (void)state;

    for (op = 0; op < sizeof(expected_lengths) / sizeof(*expected_lengths);
         op++)
        assert_int_equal(i8080_symbol_instruction_length((uint8)op),
                         expected_lengths[op]);
}

/*
 * Representative symbolic output forms should not depend on parser details.
 */
static void test_fprint_sym_formats_representative_forms(void **state)
{
    const struct expected_symbol cases[] = {
        {0x00, 0x00, 0x00, SCPE_OK, "NOP"},
        {0x01, 0x34, 0x12, -2, "LXI B,1234"},
        {0x22, 0x34, 0x12, -2, "SHLD 1234"},
        {0x3E, 0x5A, 0x00, -1, "MVI A,5A"},
        {0x41, 0x00, 0x00, SCPE_OK, "MOV B,C"},
        {0xC3, 0x34, 0x12, -2, "JMP 1234"},
        {0xC6, 0x5A, 0x00, -1, "ADI 5A"},
        {0xFF, 0x00, 0x00, SCPE_OK, "RST 7"},
        {0x08, 0x00, 0x00, 1, "???"},
    };
    size_t i;

    (void)state;

    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++)
        assert_fprint_sym_formats_instruction(&cases[i]);
}

/*
 * Every opcode should format to the documented symbolic display text. This
 * table is intentionally independent of the parser's opcode table so it can
 * catch transcription errors in mnemonic text and operand delimiters.
 */
static void test_fprint_sym_formats_all_opcode_text(void **state)
{
    static const char *const expected_symbols[256] = {
        "NOP",
        "LXI B,1234",
        "STAX B",
        "INX B",
        "INR B",
        "DCR B",
        "MVI B,34",
        "RLC",
        "???",
        "DAD B",
        "LDAX B",
        "DCX B",
        "INR C",
        "DCR C",
        "MVI C,34",
        "RRC",
        "???",
        "LXI D,1234",
        "STAX D",
        "INX D",
        "INR D",
        "DCR D",
        "MVI D,34",
        "RAL",
        "???",
        "DAD D",
        "LDAX D",
        "DCX D",
        "INR E",
        "DCR E",
        "MVI E,34",
        "RAR",
        "RIM",
        "LXI H,1234",
        "SHLD 1234",
        "INX H",
        "INR H",
        "DCR H",
        "MVI H,34",
        "DAA",
        "???",
        "DAD H",
        "LHLD 1234",
        "DCX H",
        "INR L",
        "DCR L",
        "MVI L,34",
        "CMA",
        "SIM",
        "LXI SP,1234",
        "STA 1234",
        "INX SP",
        "INR M",
        "DCR M",
        "MVI M,34",
        "STC",
        "???",
        "DAD SP",
        "LDA 1234",
        "DCX SP",
        "INR A",
        "DCR A",
        "MVI A,34",
        "CMC",
        "MOV B,B",
        "MOV B,C",
        "MOV B,D",
        "MOV B,E",
        "MOV B,H",
        "MOV B,L",
        "MOV B,M",
        "MOV B,A",
        "MOV C,B",
        "MOV C,C",
        "MOV C,D",
        "MOV C,E",
        "MOV C,H",
        "MOV C,L",
        "MOV C,M",
        "MOV C,A",
        "MOV D,B",
        "MOV D,C",
        "MOV D,D",
        "MOV D,E",
        "MOV D,H",
        "MOV D,L",
        "MOV D,M",
        "MOV D,A",
        "MOV E,B",
        "MOV E,C",
        "MOV E,D",
        "MOV E,E",
        "MOV E,H",
        "MOV E,L",
        "MOV E,M",
        "MOV E,A",
        "MOV H,B",
        "MOV H,C",
        "MOV H,D",
        "MOV H,E",
        "MOV H,H",
        "MOV H,L",
        "MOV H,M",
        "MOV H,A",
        "MOV L,B",
        "MOV L,C",
        "MOV L,D",
        "MOV L,E",
        "MOV L,H",
        "MOV L,L",
        "MOV L,M",
        "MOV L,A",
        "MOV M,B",
        "MOV M,C",
        "MOV M,D",
        "MOV M,E",
        "MOV M,H",
        "MOV M,L",
        "HLT",
        "MOV M,A",
        "MOV A,B",
        "MOV A,C",
        "MOV A,D",
        "MOV A,E",
        "MOV A,H",
        "MOV A,L",
        "MOV A,M",
        "MOV A,A",
        "ADD B",
        "ADD C",
        "ADD D",
        "ADD E",
        "ADD H",
        "ADD L",
        "ADD M",
        "ADD A",
        "ADC B",
        "ADC C",
        "ADC D",
        "ADC E",
        "ADC H",
        "ADC L",
        "ADC M",
        "ADC A",
        "SUB B",
        "SUB C",
        "SUB D",
        "SUB E",
        "SUB H",
        "SUB L",
        "SUB M",
        "SUB A",
        "SBB B",
        "SBB C",
        "SBB D",
        "SBB E",
        "SBB H",
        "SBB L",
        "SBB M",
        "SBB A",
        "ANA B",
        "ANA C",
        "ANA D",
        "ANA E",
        "ANA H",
        "ANA L",
        "ANA M",
        "ANA A",
        "XRA B",
        "XRA C",
        "XRA D",
        "XRA E",
        "XRA H",
        "XRA L",
        "XRA M",
        "XRA A",
        "ORA B",
        "ORA C",
        "ORA D",
        "ORA E",
        "ORA H",
        "ORA L",
        "ORA M",
        "ORA A",
        "CMP B",
        "CMP C",
        "CMP D",
        "CMP E",
        "CMP H",
        "CMP L",
        "CMP M",
        "CMP A",
        "RNZ",
        "POP B",
        "JNZ 1234",
        "JMP 1234",
        "CNZ 1234",
        "PUSH B",
        "ADI 34",
        "RST 0",
        "RZ",
        "RET",
        "JZ 1234",
        "???",
        "CZ 1234",
        "CALL 1234",
        "ACI 34",
        "RST 1",
        "RNC",
        "POP D",
        "JNC 1234",
        "OUT 34",
        "CNC 1234",
        "PUSH D",
        "SUI 34",
        "RST 2",
        "RC",
        "???",
        "JC 1234",
        "IN 34",
        "CC 1234",
        "???",
        "SBI 34",
        "RST 3",
        "RPO",
        "POP H",
        "JPO 1234",
        "XTHL",
        "CPO 1234",
        "PUSH H",
        "ANI 34",
        "RST 4",
        "RPE",
        "PCHL",
        "JPE 1234",
        "XCHG",
        "CPE 1234",
        "???",
        "XRI 34",
        "RST 5",
        "RP",
        "POP PSW",
        "JP 1234",
        "DI",
        "CP 1234",
        "PUSH PSW",
        "ORI 34",
        "RST 6",
        "RM",
        "SPHL",
        "JM 1234",
        "EI",
        "CM 1234",
        "???",
        "CPI 34",
        "RST 7",
    };
    size_t op;

    (void)state;

    for (op = 0; op < sizeof(expected_symbols) / sizeof(*expected_symbols);
         op++) {
        t_stat status;
        t_value val[I8080_SYMBOL_WORDS] = {op, 0x34, 0x12};
        char *symbol;

        symbol = render_i8080_symbol(val, &status);
        assert_string_equal(symbol, expected_symbols[op]);

        free(symbol);
    }
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
 * Mnemonics that are prefixes of other mnemonics should only match when their
 * operand delimiter is present.
 */
static void test_parse_sym_accepts_prefix_mnemonics(void **state)
{
    const struct expected_instruction cases[] = {
        {"CP 1234", -2, 0xF4, 0x34, 0x12},
        {"CPI 5A", -1, 0xFE, 0x5A, 0},
    };

    (void)state;

    assert_parse_sym_accepts_instructions(cases,
                                          sizeof(cases) / sizeof(*cases));
}

/*
 * Comma-delimited immediate operands should allow normal human spacing around
 * the comma.
 */
static void
test_parse_sym_accepts_immediate_operands_with_spaced_comma(void **state)
{
    const struct expected_instruction cases[] = {
        {"MVI A, 5A", -1, 0x3E, 0x5A, 0},
        {"MVI A ,5A", -1, 0x3E, 0x5A, 0},
        {"MVI A , 5A", -1, 0x3E, 0x5A, 0},
        {"LXI B, 1234", -2, 0x01, 0x34, 0x12},
        {"LXI B ,1234", -2, 0x01, 0x34, 0x12},
        {"LXI B , 1234", -2, 0x01, 0x34, 0x12},
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
    t_value val[I8080_SYMBOL_WORDS] = {0x2E, 0x5A, 0};
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
 * MOV encodes its destination and source registers directly in the opcode
 * byte; every valid register pair except M,M should round-trip.
 */
static void test_parse_sym_accepts_all_mov_register_pairs(void **state)
{
    static const char registers[] = "BCDEHLMA";
    size_t dst;

    (void)state;

    for (dst = 0; dst < sizeof(registers) - 1; dst++) {
        size_t src;

        for (src = 0; src < sizeof(registers) - 1; src++) {
            char input[] = "MOV x,y";
            t_value expected_opcode;

            if (registers[dst] == 'M' && registers[src] == 'M')
                continue;

            input[4] = registers[dst];
            input[6] = registers[src];
            expected_opcode = 0x40 + (t_value)(dst * 8 + src);
            assert_parse_sym_accepts_instruction(input, SCPE_OK,
                                                 expected_opcode, 0, 0);
        }
    }
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

        if (i8080_symbol_instruction_length((uint8)op) == 0)
            continue;

        for (pattern = 0; pattern < sizeof(patterns) / sizeof(*patterns);
             pattern++) {
            t_value input[I8080_SYMBOL_WORDS] = {0};
            t_value output[I8080_SYMBOL_WORDS] = {0};
            t_stat expected_status;
            char *symbol;

            input[0] = (t_value)op;
            input[1] = patterns[pattern].low;
            input[2] = patterns[pattern].high;

            symbol = render_i8080_symbol(input, &expected_status);
            assert_int_equal(parse_sym(symbol, 0, &symbol_unit, output,
                                       SWMASK('M')),
                             expected_status);
            assert_int_equal(output[0], input[0]);
            if (i8080_symbol_instruction_length((uint8)op) > 1)
                assert_int_equal(output[1], input[1]);
            if (i8080_symbol_instruction_length((uint8)op) > 2)
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
        "MOVB,C",
        "MOV B",
        "MOV B,",
        "MOV B,CX",
    };

    (void)state;

    assert_parse_sym_rejects_all(cases, sizeof(cases) / sizeof(*cases));
}

/*
 * MOV only accepts i8080 register names, and M,M is encoded as HLT rather
 * than a MOV instruction.
 */
static void test_parse_sym_rejects_bad_mov_operands(void **state)
{
    const char *const cases[] = {
        "MOV X,B",
        "MOV B,X",
        "MOV M,M",
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
 * Operand delimiter rules should reject missing required commas and
 * unexpected commas before space-delimited operands.
 */
static void test_parse_sym_rejects_bad_operand_delimiters(void **state)
{
    const char *const cases[] = {
        "MVI L 5A",
        "LXI B 1234",
        "ADI,5A",
        "JMP,1234",
        "MOV B C",
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
    t_value val[I8080_SYMBOL_WORDS] = {0};

    (void)state;

    assert_int_equal(parse_sym(input, 0, &symbol_unit, val, SWMASK('A')),
                     SCPE_OK);
    assert_int_equal(val[0], 0x80);
}

/*
 * ASCII string input stores two host characters as unsigned simulated bytes.
 */
static void test_parse_sym_ascii_string_uses_unsigned_bytes(void **state)
{
    char input[] = {(char)0x80, (char)0x81, '\0'};
    t_value val[I8080_SYMBOL_WORDS] = {0};

    (void)state;

    assert_int_equal(parse_sym(input, 0, &symbol_unit, val, SWMASK('C')),
                     SCPE_OK);
    assert_int_equal(val[0], 0x8081);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(
            test_opcode_table_has_expected_instruction_lengths,
            setup_i8080_symbols),
        cmocka_unit_test_setup(test_fprint_sym_formats_representative_forms,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_fprint_sym_formats_all_opcode_text,
                               setup_i8080_symbols),
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
        cmocka_unit_test_setup(test_parse_sym_accepts_prefix_mnemonics,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(
            test_parse_sym_accepts_immediate_operands_with_spaced_comma,
            setup_i8080_symbols),
        cmocka_unit_test_setup(
            test_fprint_sym_mvi_l_uses_comma_operand_delimiter,
            setup_i8080_symbols),
        cmocka_unit_test_setup(
            test_parse_sym_accepts_mov_mnemonic_with_spaced_comma,
            setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_accepts_all_mov_register_pairs,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_accepts_all_fprint_sym_outputs,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(
            test_parse_sym_rejects_legacy_octal_operand_spelling,
            setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_incomplete_mov_mnemonics,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_bad_mov_operands,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_bad_rst_mnemonics,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_trailing_opcode_garbage,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_missing_operands,
                               setup_i8080_symbols),
        cmocka_unit_test_setup(test_parse_sym_rejects_bad_operand_delimiters,
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
