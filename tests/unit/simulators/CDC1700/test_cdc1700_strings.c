#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_cmocka.h"

#include "cdc1700_defs.h"

uint16_t M[MAXMEMSIZE];
uint16_t Preg;
uint16_t Areg;
uint16_t Qreg;
uint16_t Mreg;
uint16_t RelBase;
uint8_t P[MAXMEMSIZE];
DEVICE cpu_dev;
UNIT cpu_unit;
char INTprefix[16];

uint16_t LoadFromMem(uint16_t addr)
{
    return M[MEMADDR(addr)];
}

uint16_t doADDinternal(uint16_t a, uint16_t b)
{
    return (uint16_t)(a + b);
}

t_stat disEffectiveAddr(uint16_t addr, uint16_t instr, uint16_t *base,
                        uint16_t *taddr)
{
    (void)addr;
    (void)instr;

    *base = 0x0010;
    *taddr = 0x0020;
    return SCPE_OK;
}

#include "cdc1700_msos5.c"

static void reset_cdc1700_fixture(void)
{
    memset(M, 0, sizeof(M));
    memset(P, 0, sizeof(P));
    memset(&cpu_dev, 0, sizeof(cpu_dev));
    memset(&cpu_unit, 0, sizeof(cpu_unit));
    cpu_unit.capac = MAXMEMSIZE;
    cpu_unit.u3 = INSTR_ORIGINAL;
    sim_switches = 0;
    RelBase = 0;
}

static void test_disassem_appends_target_information(void **state)
{
    char buffer[128];

    (void)state;
    reset_cdc1700_fixture();

    M[0x0100] = OPC_ADD | 1;
    M[0x0020] = 0xABCD;

    assert_int_equal(
        disassem(buffer, sizeof(buffer), 0x0100, false, true, false), 1);
    assert_non_null(strstr(buffer, "[ => 0020  {ABCD}"));
}

static void test_disassem_respects_small_output_buffer(void **state)
{
    char buffer[12];

    (void)state;
    reset_cdc1700_fixture();

    M[0x0100] = OPC_ADD | 1;
    M[0x0020] = 0xABCD;

    assert_int_equal(
        disassem(buffer, sizeof(buffer), 0x0100, false, true, false), 1);
    assert_int_equal(buffer[sizeof(buffer) - 1], '\0');
}

static void test_msos_text_rep_preserves_current_spelling(void **state)
{
    (void)state;
    reset_cdc1700_fixture();

    M[0] = ('A' << 8) | 'B';
    M[1] = ('\n' << 8) | 'C';

    assert_string_equal(textRep(0, 2), "AB<0A>C");
}

static void test_msos_text_rep_keeps_fifty_printable_characters(void **state)
{
    int i;

    (void)state;
    reset_cdc1700_fixture();

    for (i = 0; i < 25; i++)
        M[i] = ('A' << 8) | 'B';

    assert_string_equal(textRep(0, 25),
                        "ABABABABABABABABABABABABABABABABABABABABABABABABAB");
}

static void test_msos_text_rep_truncates_after_fifty_characters(void **state)
{
    int i;

    (void)state;
    reset_cdc1700_fixture();

    for (i = 0; i < 26; i++)
        M[i] = ('A' << 8) | 'B';

    assert_string_equal(textRep(0, 26),
                        "ABABABABABABABABABABABABABABABABABABABABABABABABAB");
}

static void test_msos_text_rep_does_not_split_control_names(void **state)
{
    int i;

    (void)state;
    reset_cdc1700_fixture();

    for (i = 0; i < 23; i++)
        M[i] = ('A' << 8) | 'B';
    M[23] = ('C' << 8) | '\n';

    assert_string_equal(textRep(0, 24),
                        "ABABABABABABABABABABABABABABABABABABABABABABABC");
}

static void test_msos_motion_appends_density_and_actions(void **state)
{
    char details[128];

    (void)state;
    reset_cdc1700_fixture();

    details[0] = '\0';
    M[4] = 0x1234;

    motion(0, details, sizeof(details));

    assert_string_equal(details, "    Density   = 1600 BPI\r\n"
                                 "    Actions   = BSR,EOF,REW\r\n");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_disassem_appends_target_information),
        cmocka_unit_test(test_disassem_respects_small_output_buffer),
        cmocka_unit_test(test_msos_text_rep_preserves_current_spelling),
        cmocka_unit_test(test_msos_text_rep_keeps_fifty_printable_characters),
        cmocka_unit_test(test_msos_text_rep_truncates_after_fifty_characters),
        cmocka_unit_test(test_msos_text_rep_does_not_split_control_names),
        cmocka_unit_test(test_msos_motion_appends_density_and_actions),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
