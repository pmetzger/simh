#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_cmocka.h"

#include "cdc1700_defs.h"

uint16_t Areg;
uint16_t Mreg;
uint16_t Preg;
uint16_t Qreg;
uint16_t RelBase;
uint8_t P[MAXMEMSIZE];
DEVICE cpu_dev;
UNIT cpu_unit;

static uint16_t test_memory[MAXMEMSIZE];

uint16_t LoadFromMem(uint16_t addr);
t_stat disEffectiveAddr(uint16_t addr, uint16_t instr, uint16_t *base,
                        uint16_t *target);
uint16_t doADDinternal(uint16_t a, uint16_t b);

uint16_t LoadFromMem(uint16_t addr)
{
    return test_memory[MEMADDR(addr)];
}

t_stat disEffectiveAddr(uint16_t addr, uint16_t instr, uint16_t *base,
                        uint16_t *target)
{
    (void)addr;
    (void)instr;
    (void)base;
    (void)target;

    return SCPE_IERR;
}

uint16_t doADDinternal(uint16_t a, uint16_t b)
{
    return a + b;
}

#include "cdc1700_dis.c"

static void reset_disassembler_state(void)
{
    memset(test_memory, 0, sizeof(test_memory));
    memset(P, 0, sizeof(P));
    memset(&cpu_dev, 0, sizeof(cpu_dev));
    memset(&cpu_unit, 0, sizeof(cpu_unit));
    cpu_unit.capac = MAXMEMSIZE;
    cpu_unit.u3 = INSTR_ORIGINAL;
}

static void test_zero_delta_sls_disassembles_without_operand(void **state)
{
    char output[128];

    (void)state;

    reset_disassembler_state();
    test_memory[0] = OPC_SLS;

    assert_int_equal(disassem(output, 0, false, false, false), 1);
    assert_non_null(strstr(output, "SLS"));
    assert_null(strstr(output, "$00"));
    assert_null(strstr(output, "UNDEF"));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_zero_delta_sls_disassembles_without_operand),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
