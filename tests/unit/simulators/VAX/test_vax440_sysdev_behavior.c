#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "scp.h"
#include "sim_types.h"
#include "vax_defs.h"
#include "vax_cpu.h"
#include "vax_cpu1.h"
#include "vax4xx_rom_patch.h"
#include "vax440_sysdev_internal.h"
#include "vax_xs.h"

uint32_t *rom;
uint32_t *M;
uint32_t R[16];
uint32_t STK[5];
uint32_t PSL;
uint32_t SISR;
uint32_t fault_PC;
uint32_t p1;
uint32_t p2;
uint32_t pcq[PCQ_SIZE];
uint32_t mchk_va;
uint32_t mchk_ref;
uint32_t trpirq;
int32_t pcq_p;
int32_t in_ie;
int32_t ibcnt;
int32_t ppc;
int32_t mapen;
TLBENT stlb[VA_TBSIZE];
TLBENT ptlb[VA_TBSIZE];
int32_t hlt_pin;
int32_t tmr_int;
int32_t tmr_poll;
DEVICE cpu_dev;
UNIT cpu_unit;
UNIT clk_unit;
DEVICE lk_dev;
DEVICE vs_dev;
jmp_buf save_env;

TLBENT fill(uint32_t va, int32_t lnt, int32_t acc, int32_t *stat)
{
    /* Stubbed MMU fill for uncalled memory access paths. */
    (void)va;
    (void)lnt;
    (void)acc;
    (void)stat;

    return (TLBENT){0, 0};
}

void WriteIO(uint32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed I/O write for uncalled memory access paths. */
    (void)pa;
    (void)val;
    (void)lnt;
}

void WriteIOU(uint32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed unaligned I/O write for uncalled memory access paths. */
    (void)pa;
    (void)val;
    (void)lnt;
}

t_stat cpu_load_bootcode(const char *filename, const uchar_t *builtin_code,
                         size_t size, bool load_rom, t_addr offset)
{
    /* Stubbed boot-code loader for uncalled boot paths. */
    (void)filename;
    (void)builtin_code;
    (void)size;
    (void)load_rom;
    (void)offset;

    return SCPE_OK;
}

t_stat rom_apply_patches(void)
{
    /* Stubbed ROM patch helper for uncalled boot paths. */
    return SCPE_OK;
}

int32_t intexc(int32_t vec, int32_t cc, int32_t ipl, int ei)
{
    /* Stubbed interrupt helper for uncalled legacy paths. */
    (void)vec;
    (void)cc;
    (void)ipl;
    (void)ei;

    return 0;
}

int32_t iccs_rd(void)
{
    return 0;
}

void iccs_wr(int32_t data)
{
    /* Stubbed interval clock write for uncalled legacy paths. */
    (void)data;
}

t_stat or_map(uint32_t index, uint8_t *rom_buf, t_addr size)
{
    /* Stubbed option ROM map helper for uncalled boot paths. */
    (void)index;
    (void)rom_buf;
    (void)size;

    return SCPE_OK;
}

t_stat or_unmap(uint32_t index)
{
    /* Stubbed option ROM unmap helper for uncalled boot paths. */
    (void)index;

    return SCPE_OK;
}

void cpu_idle(void) {}

int32_t nar_rd(int32_t pa)
{
    /* Stubbed register read for uncalled dispatch entries. */
    (void)pa;

    return 0;
}

int32_t dz_rd(int32_t pa)
{
    /* Stubbed register read for uncalled dispatch entries. */
    (void)pa;

    return 0;
}

int32_t xs_rd(int32_t pa)
{
    /* Stubbed register read for uncalled dispatch entries. */
    (void)pa;

    return 0;
}

int32_t nvr_rd(int32_t pa)
{
    /* Stubbed register read for uncalled dispatch entries. */
    (void)pa;

    return 0;
}

int32_t or_rd(int32_t pa)
{
    /* Stubbed register read for uncalled dispatch entries. */
    (void)pa;

    return 0;
}

int32_t rom_rd(int32_t pa)
{
    /* Stubbed register read for uncalled dispatch entries. */
    (void)pa;

    return 0;
}

int32_t rz_rd(int32_t pa)
{
    /* Stubbed register read for uncalled dispatch entries. */
    (void)pa;

    return 0;
}

void dz_wr(int32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed register write for uncalled dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
}

void xs_wr(int32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed register write for uncalled dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
}

void nvr_wr(int32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed register write for uncalled dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
}

void rz_wr(int32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed register write for uncalled dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
}

/* Install non-reset values in state that should only clear on power-up. */
static void dirty_power_up_state(void)
{
    conisp = 0x11111111;
    conpc = 0x22222222;
    conpsl = 0x33333333;
    ka_hltcod = 0x44444444;
    mem_cnfg = 0x55555555;
    CADR = 0x66666666;
    SCCR = 0x77777777;
    int_req[0] = 0x88;
    int_mask = 0x99;
}

/* Verify ordinary reset does not discard power-cycle-only KA48 state. */
static void test_ordinary_reset_preserves_power_up_state(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    dirty_power_up_state();
    sim_switches = 0;

    assert_int_equal(sysd_reset(&sysd_dev), SCPE_OK);

    assert_int_equal(conisp, 0x11111111);
    assert_int_equal(conpc, 0x22222222);
    assert_int_equal(conpsl, 0x33333333);
    assert_int_equal(ka_hltcod, 0x44444444);
    assert_int_equal(mem_cnfg, 0x55555555);
    assert_int_equal(CADR, 0x66666666);
    assert_int_equal(SCCR, 0x77777777);
    assert_int_equal(int_req[0], 0x88);
    assert_int_equal(int_mask, 0x99);
}

/* Verify RESET -P restores KA48 state that starts zero in a fresh process. */
static void test_power_up_reset_restores_power_up_state(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    dirty_power_up_state();
    sim_switches = SWMASK('P');

    assert_int_equal(sysd_reset(&sysd_dev), SCPE_OK);

    assert_int_equal(conisp, 0);
    assert_int_equal(conpc, 0);
    assert_int_equal(conpsl, 0);
    assert_int_equal(ka_hltcod, 0);
    assert_int_equal(mem_cnfg, 0);
    assert_int_equal(CADR, 0);
    assert_int_equal(SCCR, 0);
    assert_int_equal(int_req[0], 0);
    assert_int_equal(int_mask, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ordinary_reset_preserves_power_up_state),
        cmocka_unit_test(test_power_up_reset_restores_power_up_state),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
