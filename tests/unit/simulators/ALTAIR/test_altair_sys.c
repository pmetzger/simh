#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "altair_defs.h"
#include "sim_tempfile.h"
#include "test_support.h"

DEVICE cpu_dev;
DEVICE dsk_dev;
UNIT cpu_unit;
REG cpu_reg[1];
DEVICE sio_dev;
DEVICE ptr_dev;
DEVICE ptp_dev;
DEVICE lpt_dev;
unsigned char M[MAXMEMSIZE];
int32 saved_PC;

/*
 * Satisfy the simulator core's execution hook.  These tests only exercise the
 * Altair symbolic parse/print entry points, not CPU execution.
 */
t_stat sim_instr(void)
{
    return SCPE_OK;
}

static t_stat altair_test_messagef(t_stat stat, const char *fmt, ...)
    PRINTF_FMT(2, 3);

#define sim_messagef altair_test_messagef
#include "altair_sys.c"
#undef sim_messagef

static char altair_test_message[CBUFSIZE];

struct symbolic_output {
    FILE *stream;
    char path[512];
};

/*
 * Capture parse_sym error messages without pulling in the whole SCP command
 * processor.  The tests only need to verify parse_sym's status and output.
 */
static t_stat altair_test_messagef(t_stat stat, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsnprintf(altair_test_message, sizeof(altair_test_message), fmt, args);
    va_end(args);

    return stat | ((stat != SCPE_OK) ? SCPE_NOMESSAGE : 0);
}

/*
 * Read the text emitted by fprint_sym into a heap buffer for exact comparison.
 */
static char *read_symbolic_output(FILE *stream)
{
    char *output;
    size_t output_size;

    assert_int_equal(simh_test_read_stream(stream, &output, &output_size), 0);
    (void)output_size;
    return output;
}

/*
 * Open a temporary stream for fprint_sym output using the repository's
 * portable temporary-file helper instead of tmpfile().
 */
static void open_symbolic_output(struct symbolic_output *output)
{
    output->stream = sim_tempfile_open_stream(
        output->path, sizeof(output->path), "zimh-altair-sys-", ".txt", "w+b");
    assert_non_null(output->stream);
}

/*
 * Close and remove a symbolic-output stream created by open_symbolic_output().
 */
static void close_symbolic_output(struct symbolic_output *output)
{
    if (output->stream != NULL) {
        fclose(output->stream);
        output->stream = NULL;
    }
    if (output->path[0] != '\0') {
        remove(output->path);
        output->path[0] = '\0';
    }
}

/*
 * The Altair symbolic printer treats CPU and non-CPU units the same for byte
 * output.  This protects the behavior while removing stale CPU-unit detection.
 */
static void test_fprint_ascii_ignores_unit_type(void **state)
{
    struct symbolic_output stream = {0};
    char *output;
    t_value value[] = {'A'};
    UNIT non_cpu_unit = {0};

    (void)state;
    open_symbolic_output(&stream);

    assert_int_equal(
        fprint_sym(stream.stream, 0, value, &non_cpu_unit, SWMASK('A')),
        SCPE_OK);
    output = read_symbolic_output(stream.stream);
    assert_string_equal(output, "A");

    free(output);
    close_symbolic_output(&stream);
}

/*
 * Character-pair output also does not vary by unit.  The high and low bytes of
 * the value are printed in order under the C switch.
 */
static void test_fprint_character_pair_ignores_unit_type(void **state)
{
    struct symbolic_output stream = {0};
    char *output;
    t_value value[] = {('H' << 8) | 'I'};
    UNIT non_cpu_unit = {0};

    (void)state;
    open_symbolic_output(&stream);

    assert_int_equal(
        fprint_sym(stream.stream, 0, value, &non_cpu_unit, SWMASK('C')),
        SCPE_OK);
    output = read_symbolic_output(stream.stream);
    assert_string_equal(output, "HI");

    free(output);
    close_symbolic_output(&stream);
}

/*
 * Instruction output decodes one-byte opcodes without consulting the UNIT
 * pointer; the return value asks SCP to consume no additional words.
 */
static void test_fprint_one_byte_instruction_ignores_unit_type(void **state)
{
    struct symbolic_output stream = {0};
    char *output;
    t_value value[] = {0000};
    UNIT non_cpu_unit = {0};

    (void)state;
    open_symbolic_output(&stream);

    assert_int_equal(
        fprint_sym(stream.stream, 0, value, &non_cpu_unit, SWMASK('M')), 0);
    output = read_symbolic_output(stream.stream);
    assert_string_equal(output, "NOP");

    free(output);
    close_symbolic_output(&stream);
}

/*
 * Two-byte instruction output emits the immediate operand and returns -1 so
 * SCP advances over the operand word.
 */
static void test_fprint_two_byte_instruction_ignores_unit_type(void **state)
{
    struct symbolic_output stream = {0};
    char *output;
    t_value value[] = {0006, 0123};
    UNIT non_cpu_unit = {0};

    (void)state;
    open_symbolic_output(&stream);

    assert_int_equal(
        fprint_sym(stream.stream, 0, value, &non_cpu_unit, SWMASK('M')), -1);
    output = read_symbolic_output(stream.stream);
    assert_string_equal(output, "MVI B,123");

    free(output);
    close_symbolic_output(&stream);
}

/*
 * Three-byte instruction output combines the two operand bytes little-endian,
 * matching 8080 instruction encoding.
 */
static void test_fprint_three_byte_instruction_ignores_unit_type(void **state)
{
    struct symbolic_output stream = {0};
    char *output;
    t_value value[] = {0303, 064, 022};
    UNIT non_cpu_unit = {0};

    (void)state;
    open_symbolic_output(&stream);

    assert_int_equal(
        fprint_sym(stream.stream, 0, value, &non_cpu_unit, SWMASK('M')), -2);
    output = read_symbolic_output(stream.stream);
    assert_string_equal(output, "JMP 11064");

    free(output);
    close_symbolic_output(&stream);
}

/*
 * ASCII symbolic input stores the byte directly and is independent of the
 * supplied unit.
 */
static void test_parse_ascii_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("'Z", 0, &non_cpu_unit, value, 0), SCPE_OK);
    assert_int_equal(value[0], 'Z');
}

/*
 * Character-pair symbolic input stores both bytes in the same word.
 */
static void test_parse_character_pair_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("\"AZ", 0, &non_cpu_unit, value, 0), SCPE_OK);
    assert_int_equal(value[0], ('A' << 8) | 'Z');
}

/*
 * One-byte opcode input accepts lowercase mnemonics and stores the opcode.
 */
static void test_parse_one_byte_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("nop", 0, &non_cpu_unit, value, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(value[0], 0000);
}

/*
 * RST mnemonics include a numeric suffix as part of the opcode name, not as an
 * operand.  The parser has special handling for that form.
 */
static void test_parse_rst_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("rst 7", 0, &non_cpu_unit, value, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(value[0], 0377);
}

/*
 * MOV mnemonics keep their comma-separated register pair in the opcode text.
 */
static void test_parse_mov_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("mov a,m", 0, &non_cpu_unit, value, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(value[0], 0176);
}

/*
 * Two-byte instruction input returns -1 and stores the immediate byte in the
 * second value slot.
 */
static void test_parse_two_byte_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(
        parse_sym("mvi b 123", 0, &non_cpu_unit, value, SWMASK('M')), -1);
    assert_int_equal(value[0], 0006);
    assert_int_equal(value[1], 0123);
}

/*
 * Three-byte instruction input splits an octal address into low and high
 * operand bytes.
 */
static void test_parse_three_byte_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(
        parse_sym("jmp 2264", 0, &non_cpu_unit, value, SWMASK('M')), -2);
    assert_int_equal(value[0], 0303);
    assert_int_equal(value[1], 0264);
    assert_int_equal(value[2], 004);
}

/*
 * Unknown opcodes report an argument error through sim_messagef.
 */
static void test_parse_unknown_opcode_reports_error(void **state)
{
    t_value value[3] = {0};

    (void)state;
    altair_test_message[0] = '\0';

    assert_int_equal(parse_sym("bogus", 0, &cpu_unit, value, SWMASK('M')),
                     SCPE_ARG | SCPE_NOMESSAGE);
    assert_string_equal(altair_test_message, "No such opcode: BOGUS\n");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_fprint_ascii_ignores_unit_type),
        cmocka_unit_test(test_fprint_character_pair_ignores_unit_type),
        cmocka_unit_test(test_fprint_one_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_fprint_two_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_fprint_three_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_ascii_ignores_unit_type),
        cmocka_unit_test(test_parse_character_pair_ignores_unit_type),
        cmocka_unit_test(test_parse_one_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_rst_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_mov_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_two_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_three_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_unknown_opcode_reports_error),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
