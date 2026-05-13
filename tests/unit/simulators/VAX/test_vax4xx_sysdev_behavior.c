#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_types.h"
#include "vax_defs.h"
#include "vax_cpu.h"
#include "vax_cpu1.h"
#include "vax4xx_defs.h"
#include "vax4xx_rom_patch.h"
#include "vax_xs.h"
#if defined(VAX_410)
#include "vax410_sysdev_internal.h"
#else
#include "vax420_sysdev_internal.h"
#endif

/*
 * These tests preserve legacy KA410/KA420 system-device unaligned register
 * read behavior at the real disk data buffer entry point.
 */

#if defined(VAX_410)
#define TEST_DDB_BASE D16BASE
#define TEST_DDB_WORDS (D16SIZE >> 2)
#else
#define TEST_DDB_BASE D128BASE
#define TEST_DDB_WORDS (D128SIZE >> 2)
#endif

static uint32_t test_ddb[TEST_DDB_WORDS];

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
int32_t crd_err;
int32_t tmr_int;
int32_t tmr_poll;
uint32_t vc_sel;
uint32_t vc_org;
DEVICE cpu_dev;
UNIT cpu_unit;
UNIT clk_unit;
DEVICE rd_dev;
DEVICE va_dev;
DEVICE vc_dev;
DEVICE ve_dev;
DEVICE lk_dev;
DEVICE vs_dev;
DEVICE xs_dev;
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

t_stat cpu_load_bootcode(const char *filename,
                         const uchar_t *builtin_code, size_t size,
                         bool load_rom, t_addr offset)
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

void cpu_idle(void) {}

void rom_wr_B(int32_t pa, int32_t val)
{
    /* Stubbed ROM byte write for uncalled boot-code load paths. */
    (void)pa;
    (void)val;
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

#define STUB_READ(name)                                                        \
    int32_t name(int32_t pa)                                                   \
    {                                                                          \
        /* Stubbed register read for uncalled dispatch entries. */             \
        (void)pa;                                                              \
                                                                               \
        return 0;                                                              \
    }

#define STUB_WRITE(name)                                                       \
    void name(int32_t pa, int32_t val, int32_t lnt)                            \
    {                                                                          \
        /* Stubbed register write for uncalled dispatch entries. */            \
        (void)pa;                                                              \
        (void)val;                                                             \
        (void)lnt;                                                             \
    }

STUB_READ(nar_rd)
STUB_READ(dz_rd)
STUB_READ(rd_rd)
STUB_READ(xs_rd)
STUB_READ(vc_mem_rd)
STUB_READ(ve_rd)
STUB_READ(nvr_rd)
STUB_READ(or_rd)
STUB_READ(rom_rd)
STUB_READ(rz_rd)
STUB_WRITE(dz_wr)
STUB_WRITE(rd_wr)
STUB_WRITE(xs_wr)
STUB_WRITE(vc_wr)
STUB_WRITE(vc_mem_wr)
STUB_WRITE(va_wr)
STUB_WRITE(ve_wr)
STUB_WRITE(nvr_wr)
STUB_WRITE(rz_wr)

int32_t va_rd(int32_t pa)
{
    /* Return a low or high 16-bit register value for word-width read tests. */
    return (pa & 2) ? 0x8001 : 0x5678;
}

static void reset_sysdev_behavior_state(void)
{
    memset(test_ddb, 0, sizeof(test_ddb));
    ddb = test_ddb;
    int_req[0] = 0;
    int_mask = 0;
    vc_sel = 0;
    vc_org = 0;
    fault_PC = 0;

#if !defined(VAX_410)
    assert_int_equal(sim_cancel(&sysd_unit), SCPE_OK);
    tmr_inst = false;
    tmr_tir = 0;
#endif
}

/*
 * Verify unaligned long-width register reads preserve the full low and high
 * halves of the backing register image.
 */
static void test_unaligned_register_read_preserves_high_half(void **state)
{
    static const struct {
        uint32_t stored;
        uint32_t expected;
    } cases[] = {
        {0x12345678u, 0x12345678u},
        {0x80015678u, 0x80015678u},
        {0xffff5678u, 0xffff5678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sysdev_behavior_state();
        test_ddb[0] = cases[i].stored;
        assert_int_equal((uint32_t)ReadRegU(TEST_DDB_BASE + 1, L_WORD),
                         cases[i].expected);
        assert_int_equal((uint32_t)ReadRegU(TEST_DDB_BASE + 1, 3),
                         cases[i].expected);
    }
}

/*
 * Verify word-width registers preserve high-bit halves when read as long or
 * unaligned word values.
 */
static void test_word_width_register_reads_preserve_high_half(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    assert_int_equal((uint32_t)ReadRegU(VABASE, L_LONG), 0x80015678u);
    assert_int_equal((uint32_t)ReadRegU(VABASE + 1, L_WORD), 0x80015678u);
    assert_int_equal((uint32_t)ReadRegU(VABASE + 2, L_WORD), 0x80010000u);
    assert_int_equal((uint32_t)ReadReg(VABASE, L_LONG), 0x80015678u);
    assert_int_equal((uint32_t)ReadReg(VABASE + 2, L_WORD), 0x80010000u);
}

/* Verify DDB partial writes preserve high-bit byte and word values. */
static void test_ddb_register_writes_preserve_high_lanes(void **state)
{
    static const struct {
        uint32_t addr;
        int32_t val;
        int32_t lnt;
        uint32_t expected;
    } cases[] = {
        {TEST_DDB_BASE, 0xa5, L_BYTE, 0x123456a5u},
        {TEST_DDB_BASE + 3, 0xa5, L_BYTE, 0xa5345678u},
        {TEST_DDB_BASE, 0xd617, L_WORD, 0x1234d617u},
        {TEST_DDB_BASE + 2, 0xd617, L_WORD, 0xd6175678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sysdev_behavior_state();
        test_ddb[0] = 0x12345678u;
        WriteReg(cases[i].addr, cases[i].val, cases[i].lnt);
        assert_int_equal(test_ddb[0], cases[i].expected);
        assert_int_equal((uint32_t)ReadReg(TEST_DDB_BASE, L_LONG),
                         cases[i].expected);
    }
}

/* Verify unaligned DDB register writes preserve high-bit field groups. */
static void test_ddb_unaligned_register_writes_preserve_high_lanes(
    void **state)
{
    static const struct {
        uint32_t addr;
        int32_t val;
        int32_t lnt;
        uint32_t expected;
    } cases[] = {
        {TEST_DDB_BASE + 3, 0x80, L_BYTE, 0x80345678u},
        {TEST_DDB_BASE + 2, 0x8034, L_WORD, 0x80345678u},
        {TEST_DDB_BASE + 1, 0x803456, 3, 0x80345678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sysdev_behavior_state();
        test_ddb[0] = 0x12345678u;
        WriteRegU(cases[i].addr, cases[i].val, cases[i].lnt);
        assert_int_equal(test_ddb[0], cases[i].expected);
        assert_int_equal((uint32_t)ReadReg(TEST_DDB_BASE, L_LONG),
                         cases[i].expected);
    }
}

/* Verify direct DDB byte and word writes preserve high-bit lanes. */
static void test_ddb_direct_writes_preserve_high_lanes(void **state)
{
    uint8_t bytes[] = {0x78, 0x56, 0x34, 0x80};
    uint8_t high_byte = 0x80;
    uint16_t high_word = 0x8034;

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_sysdev_behavior_state();
    test_ddb[0] = 0x12345678u;
    ddb_WriteB(3, 1, &high_byte);
    assert_int_equal(test_ddb[0], 0x80345678u);

    reset_sysdev_behavior_state();
    ddb_WriteB(0, sizeof(bytes), bytes);
    assert_int_equal(test_ddb[0], 0x80345678u);

    reset_sysdev_behavior_state();
    test_ddb[0] = 0x12345678u;
    ddb_WriteW(2, sizeof(high_word), &high_word);
    assert_int_equal(test_ddb[0], 0x80345678u);
}

/* Verify KA register reads preserve high-lane bit composition. */
static void test_ka_register_read_preserves_high_lanes(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_sysdev_behavior_state();
    int_req[0] = 0x91;
    vc_sel = 1;
    vc_org = 0x7e;
    int_mask = 0xa5;

    assert_int_equal((uint32_t)ka_rd((int32_t)(KABASE + (3u << 2))),
                     0x91017ea5u);
    assert_int_equal((uint32_t)ReadReg(KABASE + (3u << 2), L_LONG),
                     0x91017ea5u);
}

#if !defined(VAX_410)
/* Verify the KA420 timer register read preserves the high-half TIR field. */
static void test_ka420_timer_read_preserves_high_half(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_sysdev_behavior_state();
    fault_PC = ROMBASE;
    tmr_inst = true;
    assert_int_equal(sim_activate(&sysd_unit, 0x611a), SCPE_OK);

    assert_int_equal((uint32_t)ReadReg(KABASE + (7u << 2), L_LONG),
                     0x9ee60000u);
}

/* Verify KA420 timer writes preserve the high-half interval field. */
static void test_ka420_timer_write_preserves_high_half(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_sysdev_behavior_state();

    WriteReg(KABASE + (7u << 2), (int32_t)0xe0ad0000u, L_LONG);
    assert_int_equal(tmr_tir & 0xffffu, 0xe0adu);
}
#endif

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_unaligned_register_read_preserves_high_half),
        cmocka_unit_test(test_word_width_register_reads_preserve_high_half),
        cmocka_unit_test(test_ddb_register_writes_preserve_high_lanes),
        cmocka_unit_test(
            test_ddb_unaligned_register_writes_preserve_high_lanes),
        cmocka_unit_test(test_ddb_direct_writes_preserve_high_lanes),
        cmocka_unit_test(test_ka_register_read_preserves_high_lanes),
#if !defined(VAX_410)
        cmocka_unit_test(test_ka420_timer_read_preserves_high_half),
        cmocka_unit_test(test_ka420_timer_write_preserves_high_half),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
