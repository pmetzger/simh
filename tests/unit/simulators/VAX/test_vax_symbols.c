#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_tempfile.h"
#include "vax_sys_internal.h"
#include "vax_syscm.h"

REG cpu_reg[1];
UNIT cpu_unit;
DEVICE cpu_dev = {
    "CPU", &cpu_unit, cpu_reg, NULL, 1, 16, 32, 1, 16, 8,
};
uint32_t PSL;
DEVICE *sim_devices[] = {&cpu_dev, NULL};
char sim_name[64] = "zimh-unit-vax-symbols";

t_stat sim_instr(void)
{
    return SCPE_OK;
}

t_stat sim_load(FILE *ptr, const char *cptr, const char *fnam, int flag)
{
    (void)ptr;
    (void)cptr;
    (void)fnam;
    (void)flag;

    return SCPE_OK;
}

t_stat fprint_sym_cm(FILE *of, t_addr addr, t_value *bytes, int32_t sw)
{
    (void)of;
    (void)addr;
    (void)bytes;
    (void)sw;

    return SCPE_ARG;
}

t_stat parse_sym_cm(const char *cptr, t_addr addr, t_value *bytes, int32_t sw)
{
    (void)cptr;
    (void)addr;
    (void)bytes;
    (void)sw;

    return SCPE_ARG;
}

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

static char *format_vax_symbol_m(t_value *bytes, uint32_t addr, t_stat *status)
{
    FILE *stream = sim_tmpfile();
    char *output;

    assert_non_null(stream);

    *status = fprint_sym_m(stream, addr, bytes);
    output = read_stream(stream);
    assert_int_equal(fclose(stream), 0);

    return output;
}

static char *format_vax_symbol(t_value *bytes, t_addr addr, int32_t sw,
                               t_stat *status)
{
    FILE *stream = sim_tmpfile();
    char *output;

    assert_non_null(stream);

    *status = fprint_sym(stream, addr, bytes, &cpu_unit, sw);
    output = read_stream(stream);
    assert_int_equal(fclose(stream), 0);

    return output;
}

static void assert_vax_symbol_m_formats(const t_value *bytes, size_t byte_count,
                                        const char *expected)
{
    t_stat status;
    char *output;
    t_value local[16] = {0};

    assert_true(byte_count <= sizeof(local) / sizeof(local[0]));

    memcpy(local, bytes, byte_count * sizeof(local[0]));
    output = format_vax_symbol_m(local, 0, &status);

    assert_int_equal(status, -(int)(byte_count - 1));
    assert_string_equal(output, expected);

    free(output);
}

static void assert_vax_symbol_m_round_trips(uint32_t addr, const char *input,
                                            const char *expected)
{
    t_stat status;
    char *output;
    t_value val[32] = {0};

    status = parse_sym_m(input, addr, val);
    assert_true(status < 0);

    output = format_vax_symbol_m(val, addr, &status);

    assert_true(status < 0);
    assert_string_equal(output, expected);

    free(output);
}

/* Verify parsing long immediates preserves the existing byte encoding. */
static void test_parse_sym_m_accepts_long_immediate_boundaries(void **state)
{
    static const struct {
        const char *input;
        t_value expected[7];
    } cases[] = {
        {
            "MOVL #7FFFFFFF,R0",
            {0xd0, 0x8f, 0xff, 0xff, 0xff, 0x7f, 0x50},
        },
        {
            "MOVL #80000000,R0",
            {0xd0, 0x8f, 0x00, 0x00, 0x00, 0x80, 0x50},
        },
        {
            "MOVL #FFFFFFFF,R0",
            {0xd0, 0x8f, 0xff, 0xff, 0xff, 0xff, 0x50},
        },
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        t_value val[16] = {0};

        assert_int_equal(parse_sym_m(cases[i].input, 0, val), -6);
        assert_memory_equal(val, cases[i].expected, sizeof(cases[i].expected));
    }
}

/* Verify formatting long immediates preserves current symbolic output. */
static void test_fprint_sym_m_formats_long_immediate_boundaries(void **state)
{
    static const struct {
        t_value bytes[7];
        const char *expected;
    } cases[] = {
        {
            {0xd0, 0x8f, 0xff, 0xff, 0xff, 0x7f, 0x50},
            "MOVL #7FFFFFFF,R0",
        },
        {
            {0xd0, 0x8f, 0x00, 0x00, 0x00, 0x80, 0x50},
            "MOVL #80000000,R0",
        },
        {
            {0xd0, 0x8f, 0xff, 0xff, 0xff, 0xff, 0x50},
            "MOVL #FFFFFFFF,R0",
        },
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_vax_symbol_m_formats(
            cases[i].bytes, sizeof(cases[i].bytes) / sizeof(cases[i].bytes[0]),
            cases[i].expected);
}

/* Verify long displacements preserve signed boundary encodings. */
static void test_parse_sym_m_accepts_long_displacement_boundaries(void **state)
{
    static const struct {
        const char *input;
        t_value expected[7];
    } cases[] = {
        {
            "MOVL L^7FFFFFFF(R0),R1",
            {0xd0, 0xe0, 0xff, 0xff, 0xff, 0x7f, 0x51},
        },
        {
            "MOVL L^-80000000(R0),R1",
            {0xd0, 0xe0, 0x00, 0x00, 0x00, 0x80, 0x51},
        },
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        t_value val[16] = {0};

        assert_int_equal(parse_sym_m(cases[i].input, 0, val), -6);
        assert_memory_equal(val, cases[i].expected, sizeof(cases[i].expected));
    }
}

/* Verify branch parsing preserves signed PC-relative boundary encodings. */
static void
test_parse_sym_m_accepts_branch_displacement_boundaries(void **state)
{
    static const struct {
        const char *input;
        t_value expected[3];
        size_t byte_count;
    } cases[] = {
        {"BRB 1081", {0x11, 0x7f, 0x00}, 2},
        {"BRB F82", {0x11, 0x80, 0x00}, 2},
        {"BRW 9002", {0x31, 0xff, 0x7f}, 3},
        {"BRW FFFF9003", {0x31, 0x00, 0x80}, 3},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        t_value val[16] = {0};

        assert_int_equal(parse_sym_m(cases[i].input, 0x1000, val),
                         -((int)cases[i].byte_count - 1));
        assert_memory_equal(val, cases[i].expected,
                            cases[i].byte_count * sizeof(val[0]));
    }
}

/* Verify the public symbolic formatter reaches machine-mode formatting. */
static void test_fprint_sym_formats_machine_mode_through_wrapper(void **state)
{
    t_stat status;
    char *output;
    t_value bytes[] = {
        0xd0, 0x8f, 0xff, 0xff, 0xff, 0xff, 0x50,
    };

    (void)state;

    output = format_vax_symbol(bytes, 0, SWMASK('M'), &status);

    assert_int_equal(status, -6);
    assert_string_equal(output, "MOVL #FFFFFFFF,R0");

    free(output);
}

/* Verify the public numeric formatter reads byte buffers as unsigned values. */
static void test_fprint_sym_formats_high_bit_numeric_values(void **state)
{
    t_stat status;
    char *output;
    t_value bytes[] = {0xff, 0xff, 0xff, 0xff};

    (void)state;

    output = format_vax_symbol(bytes, 0, SWMASK('L') | SWMASK('H'), &status);

    assert_int_equal(status, -3);
    assert_string_equal(output, "FFFFFFFF");

    free(output);
}

/* Verify public symbolic input reaches machine-mode parsing. */
static void test_parse_sym_accepts_machine_mode_through_wrapper(void **state)
{
    t_value val[16] = {0};
    const t_value expected[] = {
        0xd0, 0x8f, 0xff, 0xff, 0xff, 0xff, 0x50,
    };

    (void)state;

    assert_int_equal(parse_sym("MOVL #FFFFFFFF,R0", 0, &cpu_unit, val,
                               SWMASK('M')),
                     -6);
    assert_memory_equal(val, expected, sizeof(expected));
}

/* Verify public numeric input stores high-bit unsigned byte buffers. */
static void test_parse_sym_accepts_high_bit_numeric_values(void **state)
{
    t_value val[4] = {0};
    const t_value expected[] = {0xff, 0xff, 0xff, 0xff};

    (void)state;

    assert_int_equal(parse_sym("FFFFFFFF", 0, &cpu_unit, val,
                               SWMASK('L') | SWMASK('H')),
                     -3);
    assert_memory_equal(val, expected, sizeof(expected));
}

/* Verify character parsing through the public symbolic input wrapper. */
static void test_parse_sym_accepts_character_input(void **state)
{
    t_value val[4] = {0};
    const t_value expected[] = {'A', 'B'};

    (void)state;

    assert_int_equal(parse_sym("'AB", 0, &cpu_unit, val, SWMASK('W')), -1);
    assert_memory_equal(val, expected, sizeof(expected));
}

/* Verify representative addressing modes round-trip through parse and print. */
static void test_vax_symbol_round_trips_addressing_modes(void **state)
{
    static const struct {
        const char *input;
        const char *expected;
    } cases[] = {
        {"MOVL #3F,R0", "MOVL #3F,R0"},
        {"MOVL R0,R1", "MOVL R0,R1"},
        {"MOVL (R0),R1", "MOVL (R0),R1"},
        {"MOVL -(R0),R1", "MOVL -(R0),R1"},
        {"MOVL (R0)+,R1", "MOVL (R0)+,R1"},
        {"MOVL @(R0)+,R1", "MOVL @(R0)+,R1"},
        {"MOVL @#80000000,R1", "MOVL @#80000000,R1"},
        {"MOVL 7F(R0),R1", "MOVL 7F(R0),R1"},
        {"MOVL 8000(R0),R1", "MOVL -8000(R0),R1"},
        {"MOVL B^7F(R0),R1", "MOVL 7F(R0),R1"},
        {"MOVL B^-80(R0),R1", "MOVL -80(R0),R1"},
        {"MOVL W^7FFF(R0),R1", "MOVL 7FFF(R0),R1"},
        {"MOVL W^-8000(R0),R1", "MOVL -8000(R0),R1"},
        {"MOVL L^7FFFFFFF(R0),R1", "MOVL 7FFFFFFF(R0),R1"},
        {"MOVL 80000000(R0),R1", "MOVL -80000000(R0),R1"},
        {"MOVL L^-80000000(R0),R1", "MOVL -80000000(R0),R1"},
        {"MOVL @B^7F(R0),R1", "MOVL @7F(R0),R1"},
        {"MOVL @W^7FFF(R0),R1", "MOVL @7FFF(R0),R1"},
        {"MOVL @L^7FFFFFFF(R0),R1", "MOVL @7FFFFFFF(R0),R1"},
        {"MOVL @80000000(R0),R1", "MOVL @-80000000(R0),R1"},
        {"MOVL @L^80000000(R0),R1", "MOVL @-80000000(R0),R1"},
        {"MOVL 1081,R1", "MOVL 1081,R1"},
        {"MOVL FFFF9003,R1", "MOVL FFFF9003,R1"},
        {"MOVL L^FFFF9003,R1", "MOVL FFFF9003,R1"},
        {"MOVL 7F(R0)[R2],R1", "MOVL 7F(R0)[R2],R1"},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_vax_symbol_m_round_trips(0x1000, cases[i].input,
                                        cases[i].expected);
}

/* Verify branch displacements round-trip through parse and print. */
static void test_vax_symbol_round_trips_branch_displacements(void **state)
{
    static const struct {
        const char *input;
        const char *expected;
    } cases[] = {
        {"BRB 1081", "BRB 1081"},
        {"BRB F82", "BRB F82"},
        {"BRW 9002", "BRW 9002"},
        {"BRW FFFF9003", "BRW FFFF9003"},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_vax_symbol_m_round_trips(0x1000, cases[i].input,
                                        cases[i].expected);
}

/* Verify quad and octa immediates round-trip through their helper path. */
static void test_vax_symbol_round_trips_quad_and_octa_immediates(void **state)
{
    static const struct {
        const char *input;
        const char *expected;
    } cases[] = {
        {"MOVQ #8000000000000000,R0", "MOVQ #8000000000000000,R0"},
        {"MOVO #80000000000000000000000000000000,R0",
         "MOVO #80000000000000000000000000000000,R0"},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_vax_symbol_m_round_trips(0, cases[i].input, cases[i].expected);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_parse_sym_m_accepts_long_immediate_boundaries),
        cmocka_unit_test(test_fprint_sym_m_formats_long_immediate_boundaries),
        cmocka_unit_test(test_parse_sym_m_accepts_long_displacement_boundaries),
        cmocka_unit_test(
            test_parse_sym_m_accepts_branch_displacement_boundaries),
        cmocka_unit_test(test_fprint_sym_formats_machine_mode_through_wrapper),
        cmocka_unit_test(test_fprint_sym_formats_high_bit_numeric_values),
        cmocka_unit_test(test_parse_sym_accepts_machine_mode_through_wrapper),
        cmocka_unit_test(test_parse_sym_accepts_high_bit_numeric_values),
        cmocka_unit_test(test_parse_sym_accepts_character_input),
        cmocka_unit_test(test_vax_symbol_round_trips_addressing_modes),
        cmocka_unit_test(test_vax_symbol_round_trips_branch_displacements),
        cmocka_unit_test(test_vax_symbol_round_trips_quad_and_octa_immediates),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
