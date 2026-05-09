#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "swtp_defs.h"

#include "m6800.c"

/*
 * sim_instr() accesses memory through the SWTP6800 board callbacks. These
 * fakes provide a flat byte-addressed memory image for CPU-only tests.
 */
static uint8_t test_memory[MAXMEMSIZE];
REG *sim_PC = &m6800_reg[0];
DEVICE *sim_devices[] = {&m6800_dev, NULL};
char sim_name[] = "SWTP6800";
const char *sim_stop_messages[SCPE_BASE];
int32_t sim_emax = 1;

int32_t CPU_BD_get_mbyte(int32_t addr)
{
    return test_memory[addr & ADDRMASK];
}

int32_t CPU_BD_get_mword(int32_t addr)
{
    return ((CPU_BD_get_mbyte(addr) << 8) |
            CPU_BD_get_mbyte((addr + 1) & ADDRMASK)) &
           ADDRMASK;
}

void CPU_BD_put_mbyte(int32_t addr, int32_t val)
{
    test_memory[addr & ADDRMASK] = val & BYTEMASK;
}

void CPU_BD_put_mword(int32_t addr, int32_t val)
{
    CPU_BD_put_mbyte(addr, val >> 8);
    CPU_BD_put_mbyte((addr + 1) & ADDRMASK, val);
}

static int setup_m6800(void **state)
{
    (void)state;

    memset(test_memory, 0, sizeof(test_memory));
    A = 0;
    B = 0;
    IX = 0;
    SP = 0;
    CC = CC_ALWAYS_ON | IF;
    saved_PC = 0;
    PC = 0;
    NMI = 0;
    IRQ = 0;
    hst_p = 0;
    hst_lnt = 0;
    reason = 0;
    m6800_unit.flags = 0;
    sim_brk_summ = 0;
    sim_interval = 100;

    return 0;
}

/*
 * WAI stops the simulator when interrupts are masked. Before stopping it
 * pushes the return PC, index register, accumulators, and condition code to
 * the current stack, so this protects ordinary sim_instr execution and stack
 * side effects.
 */
static void test_sim_instr_runs_until_masked_wait_and_pushes_state(void **state)
{
    (void)state;

    saved_PC = 0x0100;
    SP = 0x0200;
    IX = 0x3456;
    A = 0x12;
    B = 0x34;
    CC = CC_ALWAYS_ON | IF | CF;
    test_memory[0x0100] = 0x01; /* NOP */
    test_memory[0x0101] = 0x3E; /* WAI */

    assert_int_equal(sim_instr(), STOP_HALT);

    assert_int_equal(saved_PC, 0x0102);
    assert_int_equal(SP, 0x01F9);
    assert_int_equal(test_memory[0x0200], 0x02);
    assert_int_equal(test_memory[0x01FF], 0x01);
    assert_int_equal(test_memory[0x01FE], 0x56);
    assert_int_equal(test_memory[0x01FD], 0x34);
    assert_int_equal(test_memory[0x01FC], 0x12);
    assert_int_equal(test_memory[0x01FB], 0x34);
    assert_int_equal(test_memory[0x01FA], CC_ALWAYS_ON | IF | CF);
}

/*
 * With ITRAP enabled, an unassigned opcode stops immediately and backs PC up
 * to the faulting opcode. This is the invalid-opcode behavior adjacent to the
 * stale local declaration in sim_instr.
 */
static void test_sim_instr_invalid_opcode_stops_at_faulting_pc(void **state)
{
    (void)state;

    saved_PC = 0x0100;
    SP = 0x0200;
    m6800_unit.flags = UNIT_OPSTOP;
    test_memory[0x0100] = 0x00;

    assert_int_equal(sim_instr(), STOP_OPCODE);

    assert_int_equal(saved_PC, 0x0100);
    assert_int_equal(SP, 0x0200);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(
            test_sim_instr_runs_until_masked_wait_and_pushes_state,
            setup_m6800),
        cmocka_unit_test_setup(
            test_sim_instr_invalid_opcode_stops_at_faulting_pc, setup_m6800),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
