#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "altairz80_defs.h"
#include "sim_tempfile.h"
#include "sim_types.h"
#include "test_support.h"

UNIT cpu_unit;
REG cpu_reg[CPU_INDEX_8080 + 1];
DEVICE cpu_dev;
DEVICE sio_dev;
DEVICE simh_device;
DEVICE ptr_dev;
DEVICE ptp_dev;
DEVICE dsk_dev;
DEVICE mhdsk_dev;
DEVICE hdsk_dev;
DEVICE net_dev;
DEVICE mfdc_dev;
DEVICE fw2_dev;
DEVICE fif_dev;
DEVICE vfdhd_dev;
DEVICE mdsa_dev;
DEVICE mdsad_dev;
DEVICE nsfpb_dev;
DEVICE disk1a_dev;
DEVICE disk2_dev;
DEVICE disk3_dev;
DEVICE selchan_dev;
DEVICE ss1_dev;
DEVICE if3_dev;
DEVICE i8272_dev;
DEVICE ibc_dev;
DEVICE ibc_hdc_dev;
DEVICE ibc_smd_dev;
DEVICE ibctimer_device;
DEVICE ibcrtctimer_device;
DEVICE mdriveh_dev;
DEVICE switchcpu_dev;
DEVICE adcs6_dev;
DEVICE hdc1001_dev;
DEVICE jade_dev;
DEVICE tarbell_dev;
DEVICE tdd_dev;
DEVICE icom_dev;
DEVICE dj2d_dev;
DEVICE djhdc_dev;
DEVICE m2sio0_dev;
DEVICE m2sio1_dev;
DEVICE pmmi_dev;
DEVICE hayes_dev;
DEVICE daz_dev;
DEVICE js1_dev;
DEVICE jair_dev;
DEVICE jairs0_dev;
DEVICE jairs1_dev;
DEVICE jairp_dev;
DEVICE mmd_dev;
DEVICE mmdm_dev;
DEVICE sol20_dev;
DEVICE sol20k_dev;
DEVICE sol20t_dev;
DEVICE sol20s_dev;
DEVICE sol20p_dev;
DEVICE vdm1_dev;
DEVICE cromfdc_dev;
DEVICE wd179x_dev;
DEVICE tuart0_dev;
DEVICE tuart1_dev;
DEVICE tuart2_dev;
DEVICE n8vem_dev;
DEVICE wdi2_dev;
DEVICE scp300f_dev;
ChipType chiptype = CHIP_TYPE_8080;

long disasm(uchar_t *data, char *output, size_t output_size, int segsize,
            long offset);
uint_t m68k_disassemble(char *str_buff, uint_t pc, uint_t cpu_type);

/*
 * Satisfy the simulator core execution hook.  These tests only exercise the
 * AltairZ80 symbolic formatting paths, not CPU execution.
 */
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

long disasm(uchar_t *data, char *output, size_t output_size, int segsize,
            long offset)
{
    (void)data;
    (void)output;
    (void)output_size;
    (void)segsize;
    (void)offset;
    fail_msg("8086 disassembler stub should not be called");
    return 0;
}

uint_t m68k_disassemble(char *str_buff, uint_t pc, uint_t cpu_type)
{
    (void)str_buff;
    (void)pc;
    (void)cpu_type;
    fail_msg("M68K disassembler stub should not be called");
    return 0;
}

t_stat parse_sym_m68k(char *c, t_addr a, UNIT *u, t_value *val, int32_t sw)
{
    (void)c;
    (void)a;
    (void)u;
    (void)val;
    (void)sw;
    fail_msg("M68K parser stub should not be called");
    return SCPE_IERR;
}

#include "altairz80_sys.c"

struct symbolic_output {
    FILE *stream;
    char path[512];
};

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
    output->stream =
        sim_tempfile_open_stream(output->path, sizeof(output->path),
                                 "zimh-altairz80-sys-", ".txt", "w+b");
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

static void assert_fprint_sym_output(ChipType cpu_type, t_addr addr,
                                     const t_value *input, size_t input_count,
                                     t_stat expected_status,
                                     const char *expected_output)
{
    struct symbolic_output stream = {0};
    t_value values[SIM_EMAX] = {0};
    UNIT non_cpu_unit = {0};
    char *output;

    assert_true(input_count <= SIM_EMAX);
    memcpy(values, input, input_count * sizeof(values[0]));
    chiptype = cpu_type;

    open_symbolic_output(&stream);
    assert_int_equal(
        fprint_sym(stream.stream, addr, values, &non_cpu_unit, SWMASK('M')),
        expected_status);
    output = read_symbolic_output(stream.stream);
    assert_string_equal(output, expected_output);

    free(output);
    close_symbolic_output(&stream);
}

static void test_breakpoint_messages_are_formatted(void **state)
{
    (void)state;

    chiptype = CHIP_TYPE_8080;
    prepareMemoryAccessMessage(0x1234);
    assert_string_equal(sim_stop_messages[STOP_MEM],
                        "Memory access breakpoint [01234h]");

    prepareInstructionMessage(0x2345, 0x06);
    assert_string_equal(sim_stop_messages[STOP_INSTR],
                        "Instruction \"MVI B,*h\" breakpoint [02345h]");

    chiptype = CHIP_TYPE_Z80;
    prepareInstructionMessage(0x2345, 0x06);
    assert_string_equal(sim_stop_messages[STOP_INSTR],
                        "Instruction \"LD B,*h\" breakpoint [02345h]");
}

static void test_fprint_sym_formats_8080_substitutions(void **state)
{
    const t_value immediate[] = {0x06, 0x12};
    const t_value address[] = {0x01, 0x34, 0x12};

    (void)state;

    assert_fprint_sym_output(CHIP_TYPE_8080, 0, immediate, 2, -1, "MVI B,12h");
    assert_fprint_sym_output(CHIP_TYPE_8080, 0, address, 3, -2, "LXI B,1234h");
}

static void test_fprint_sym_formats_z80_substitutions(void **state)
{
    const t_value immediate[] = {0x06, 0x9a};
    const t_value high_immediate[] = {0x06, 0xa0};
    const t_value address[] = {0x01, 0x34, 0x12};
    const t_value high_address[] = {0x01, 0x00, 0xa0};
    const t_value relative[] = {0x18, 0xfe};
    const t_value index_positive[] = {0xdd, 0x34, 0x7f};
    const t_value index_negative[] = {0xfd, 0x34, 0x80};
    const t_value indexed_bit[] = {0xdd, 0xcb, 0xfc, 0x46};

    (void)state;

    assert_fprint_sym_output(CHIP_TYPE_Z80, 0, immediate, 2, -1, "LD B,9Ah");
    assert_fprint_sym_output(CHIP_TYPE_Z80, 0, high_immediate, 2, -1,
                             "LD B,0A0h");
    assert_fprint_sym_output(CHIP_TYPE_Z80, 0, address, 3, -2, "LD BC,1234h");
    assert_fprint_sym_output(CHIP_TYPE_Z80, 0, high_address, 3, -2,
                             "LD BC,0A000h");
    assert_fprint_sym_output(CHIP_TYPE_Z80, 0x1000, relative, 2, -1,
                             "JR 1000h");
    assert_fprint_sym_output(CHIP_TYPE_Z80, 0, index_positive, 3, -2,
                             "INC (IX+7Fh)");
    assert_fprint_sym_output(CHIP_TYPE_Z80, 0, index_negative, 3, -2,
                             "INC (IY+80h)");
    assert_fprint_sym_output(CHIP_TYPE_Z80, 0, indexed_bit, 4, -3,
                             "BIT 0,(IX-04h)");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_breakpoint_messages_are_formatted),
        cmocka_unit_test(test_fprint_sym_formats_8080_substitutions),
        cmocka_unit_test(test_fprint_sym_formats_z80_substitutions),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
