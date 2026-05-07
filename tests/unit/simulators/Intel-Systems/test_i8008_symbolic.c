#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_tempfile.h"
#include "test_support.h"

static t_stat i8008_test_messagef(t_stat stat, const char *fmt, ...)
    PRINTF_FMT(2, 3);

#define sim_messagef i8008_test_messagef
#include "i8008.c"
#undef sim_messagef

struct idev dev_table[32];
char sim_name[] = "i8008-unit";
REG *sim_PC = &cpu_reg[0];
int32 sim_emax = 4;
DEVICE *sim_devices[] = {&cpu_dev, NULL};
const char *sim_stop_messages[SCPE_BASE];

/*
 * Satisfy the simulator core's loader hook.  These tests only exercise the
 * i8008 symbolic parse/print entry points, not file loading.
 */
t_stat sim_load(FILE *ptr, const char *cptr, const char *fnam, int flag)
{
    (void)ptr;
    (void)cptr;
    (void)fnam;
    (void)flag;

    return SCPE_OK;
}

static char i8008_test_message[CBUFSIZE];

struct symbolic_output {
    FILE *stream;
    char path[512];
};

/*
 * Capture parse_sym diagnostics so tests can assert both status and message
 * without depending on the whole SCP command processor.
 */
static t_stat i8008_test_messagef(t_stat stat, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsnprintf(i8008_test_message, sizeof(i8008_test_message), fmt, args);
    va_end(args);

    return stat | ((stat != SCPE_OK) ? SCPE_NOMESSAGE : 0);
}

/*
 * Open a temporary stream for symbolic-printer output using the portable
 * simulator temporary-file helper.
 */
static void open_symbolic_output(struct symbolic_output *output)
{
    output->stream =
        sim_tempfile_open_stream(output->path, sizeof(output->path),
                                 "zimh-i8008-symbolic-", ".txt", "w+b");
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
 * Read fprint_sym output into a heap buffer for exact comparisons.
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
 * The i8008 symbolic printer treats CPU and non-CPU units identically for
 * ASCII byte output.
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
 * Character-pair output prints the high and low bytes in order and does not
 * vary by UNIT pointer.
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
 * One-byte instruction output emits only the opcode mnemonic and asks SCP to
 * consume no extra words.
 */
static void test_fprint_one_byte_instruction_ignores_unit_type(void **state)
{
    struct symbolic_output stream = {0};
    char *output;
    t_value value[] = {0002};
    UNIT non_cpu_unit = {0};

    (void)state;
    open_symbolic_output(&stream);

    assert_int_equal(
        fprint_sym(stream.stream, 0, value, &non_cpu_unit, SWMASK('M')), 0);
    output = read_symbolic_output(stream.stream);
    assert_string_equal(output, "RLC");

    free(output);
    close_symbolic_output(&stream);
}

/*
 * i8008 INP opcodes encode the port number in the opcode itself, not in a
 * following operand byte.
 */
static void test_fprint_inp_instruction_ignores_unit_type(void **state)
{
    struct symbolic_output stream = {0};
    char *output;
    t_value value[] = {0117};
    UNIT non_cpu_unit = {0};

    (void)state;
    open_symbolic_output(&stream);

    assert_int_equal(
        fprint_sym(stream.stream, 0, value, &non_cpu_unit, SWMASK('M')), 0);
    output = read_symbolic_output(stream.stream);
    assert_string_equal(output, "INP 7");

    free(output);
    close_symbolic_output(&stream);
}

/*
 * i8008 OUT opcodes use the same embedded-port encoding as INP, with the
 * valid output-port range running from octal 10 through octal 37.
 */
static void test_fprint_out_instruction_ignores_unit_type(void **state)
{
    struct symbolic_output stream = {0};
    char *output;
    t_value value[] = {0177};
    UNIT non_cpu_unit = {0};

    (void)state;
    open_symbolic_output(&stream);

    assert_int_equal(
        fprint_sym(stream.stream, 0, value, &non_cpu_unit, SWMASK('M')), 0);
    output = read_symbolic_output(stream.stream);
    assert_string_equal(output, "OUT 37");

    free(output);
    close_symbolic_output(&stream);
}

/*
 * Two-byte immediate instructions emit the following byte and return -1 so
 * SCP advances past it.
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
    assert_string_equal(output, "LAI 123");

    free(output);
    close_symbolic_output(&stream);
}

/*
 * Three-byte branch instructions combine the low and high operand bytes as an
 * i8008 address and return -2.
 */
static void test_fprint_three_byte_instruction_ignores_unit_type(void **state)
{
    struct symbolic_output stream = {0};
    char *output;
    t_value value[] = {0104, 064, 022};
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
 * ASCII symbolic input stores one byte and does not depend on the UNIT.
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
 * Character-pair symbolic input stores both characters in one word.
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
 * One-byte mnemonic parsing accepts lowercase text and stores the opcode.
 */
static void test_parse_one_byte_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("rlc", 0, &non_cpu_unit, value, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(value[0], 0002);
}

/*
 * RST instructions encode the vector in the opcode mnemonic itself.
 */
static void test_parse_rst_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("rst7", 0, &non_cpu_unit, value, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(value[0], 0075);
}

/*
 * INP parses an octal input port and folds it into the opcode byte.
 */
static void test_parse_inp_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("inp 7", 0, &non_cpu_unit, value, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(value[0], 0117);
}

/*
 * OUT parses an octal output port and folds it into the opcode byte.
 */
static void test_parse_out_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("out 37", 0, &non_cpu_unit, value, SWMASK('M')),
                     SCPE_OK);
    assert_int_equal(value[0], 0177);
}

/*
 * Invalid i8008 input ports are rejected rather than silently truncated.
 */
static void test_parse_rejects_invalid_inp_port(void **state)
{
    t_value value[3] = {0};

    (void)state;

    assert_int_equal(parse_sym("inp 10", 0, &cpu_unit, value, SWMASK('M')),
                     SCPE_ARG);
}

/*
 * Invalid i8008 output ports are rejected rather than silently encoded.
 */
static void test_parse_rejects_invalid_out_port(void **state)
{
    t_value value[3] = {0};

    (void)state;

    assert_int_equal(parse_sym("out 7", 0, &cpu_unit, value, SWMASK('M')),
                     SCPE_ARG);
}

/*
 * Two-byte immediate parsing stores the operand in the second value slot and
 * returns -1.
 */
static void test_parse_two_byte_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(parse_sym("lai 123", 0, &non_cpu_unit, value, SWMASK('M')),
                     -1);
    assert_int_equal(value[0], 0006);
    assert_int_equal(value[1], 0123);
}

/*
 * Three-byte branch parsing stores the low byte before the high byte, matching
 * the instruction stream consumed by fprint_sym.
 */
static void test_parse_three_byte_instruction_ignores_unit_type(void **state)
{
    t_value value[3] = {0};
    UNIT non_cpu_unit = {0};

    (void)state;

    assert_int_equal(
        parse_sym("jmp 2264", 0, &non_cpu_unit, value, SWMASK('M')), -2);
    assert_int_equal(value[0], 0104);
    assert_int_equal(value[1], 0264);
    assert_int_equal(value[2], 004);
}

/*
 * Unknown mnemonics are reported through sim_messagef.
 */
static void test_parse_unknown_opcode_reports_error(void **state)
{
    t_value value[3] = {0};

    (void)state;
    i8008_test_message[0] = '\0';

    assert_int_equal(parse_sym("bogus", 0, &cpu_unit, value, SWMASK('M')),
                     SCPE_ARG | SCPE_NOMESSAGE);
    assert_string_equal(i8008_test_message, "No such opcode: BOGUS\n");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_fprint_ascii_ignores_unit_type),
        cmocka_unit_test(test_fprint_character_pair_ignores_unit_type),
        cmocka_unit_test(test_fprint_one_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_fprint_inp_instruction_ignores_unit_type),
        cmocka_unit_test(test_fprint_out_instruction_ignores_unit_type),
        cmocka_unit_test(test_fprint_two_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_fprint_three_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_ascii_ignores_unit_type),
        cmocka_unit_test(test_parse_character_pair_ignores_unit_type),
        cmocka_unit_test(test_parse_one_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_rst_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_inp_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_out_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_rejects_invalid_inp_port),
        cmocka_unit_test(test_parse_rejects_invalid_out_port),
        cmocka_unit_test(test_parse_two_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_three_byte_instruction_ignores_unit_type),
        cmocka_unit_test(test_parse_unknown_opcode_reports_error),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
