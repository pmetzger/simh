#include <stddef.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"
#include "vax4xx_stddev.h"

/*
 * These tests preserve legacy KA410/KA420 system-device unaligned register
 * read behavior at the real disk data buffer entry point.
 */

int32 ReadReg(uint32 pa, int32 lnt);
int32 ReadRegU(uint32 pa, int32 lnt);
void WriteReg(uint32 pa, int32 val, int32 lnt);
void WriteRegU(uint32 pa, int32 val, int32 lnt);
void ddb_WriteB(uint32 ba, uint32 bc, uint8 *buf);
void ddb_WriteW(uint32 ba, uint32 bc, uint16 *buf);
int32 nar_rd(int32 pa);
int32 dz_rd(int32 pa);
int32 rd_rd(int32 pa);
int32 va_rd(int32 pa);
int32 ve_rd(int32 pa);
void dz_wr(int32 pa, int32 val, int32 lnt);
void rd_wr(int32 pa, int32 val, int32 lnt);
void vc_wr(int32 pa, int32 val, int32 lnt);
void va_wr(int32 pa, int32 val, int32 lnt);
void ve_wr(int32 pa, int32 val, int32 lnt);

extern uint32 *ddb;

#if defined(VAX_410)
#define TEST_DDB_BASE D16BASE
#define TEST_DDB_WORDS (D16SIZE >> 2)
#else
#define TEST_DDB_BASE D128BASE
#define TEST_DDB_WORDS (D128SIZE >> 2)
#endif

static uint32 test_ddb[TEST_DDB_WORDS];

uint32 *rom;
uint32 *M;
uint32 R[16];
uint32 STK[5];
uint32 PSL;
uint32 SISR;
uint32 fault_PC;
uint32 p1;
uint32 p2;
uint32 pcq[PCQ_SIZE];
uint32 mchk_va;
uint32 mchk_ref;
uint32 trpirq;
int32 pcq_p;
int32 in_ie;
int32 ibcnt;
int32 ppc;
int32 mapen;
TLBENT stlb[VA_TBSIZE];
TLBENT ptlb[VA_TBSIZE];
int32 hlt_pin;
int32 crd_err;
int32 tmr_int;
int32 tmr_poll;
uint32 vc_sel;
uint32 vc_org;
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

TLBENT fill(uint32 va, int32 lnt, int32 acc, int32 *stat)
{
    /* Stubbed MMU fill for uncalled memory access paths. */
    (void)va;
    (void)lnt;
    (void)acc;
    (void)stat;

    return (TLBENT){0, 0};
}

t_stat cpu_load_bootcode(const char *filename,
                         const unsigned char *builtin_code, size_t size,
                         t_bool load_rom, t_addr offset)
{
    /* Stubbed boot-code loader for uncalled boot paths. */
    (void)filename;
    (void)builtin_code;
    (void)size;
    (void)load_rom;
    (void)offset;

    return SCPE_OK;
}

void cpu_idle(void) {}

void rom_wr_B(int32 pa, int32 val)
{
    /* Stubbed ROM byte write for uncalled boot-code load paths. */
    (void)pa;
    (void)val;
}

t_stat or_map(uint32 index, uint8 *rom_buf, t_addr size)
{
    /* Stubbed option ROM map helper for uncalled boot paths. */
    (void)index;
    (void)rom_buf;
    (void)size;

    return SCPE_OK;
}

t_stat or_unmap(uint32 index)
{
    /* Stubbed option ROM unmap helper for uncalled boot paths. */
    (void)index;

    return SCPE_OK;
}

int32 intexc(int32 vec, int32 cc, int32 ipl, int ei)
{
    /* Stubbed interrupt helper for uncalled legacy paths. */
    (void)vec;
    (void)cc;
    (void)ipl;
    (void)ei;

    return 0;
}

int32 iccs_rd(void)
{
    return 0;
}

void iccs_wr(int32 data)
{
    /* Stubbed interval clock write for uncalled legacy paths. */
    (void)data;
}

#define STUB_READ(name)                                                        \
    int32 name(int32 pa)                                                       \
    {                                                                          \
        /* Stubbed register read for uncalled dispatch entries. */             \
        (void)pa;                                                              \
                                                                               \
        return 0;                                                              \
    }

#define STUB_WRITE(name)                                                       \
    void name(int32 pa, int32 val, int32 lnt)                                  \
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

int32 va_rd(int32 pa)
{
    /* Return a low or high 16-bit register value for word-width read tests. */
    return (pa & 2) ? 0x8001 : 0x5678;
}

static void reset_sysdev_behavior_state(void)
{
    memset(test_ddb, 0, sizeof(test_ddb));
    ddb = test_ddb;
}

/*
 * Verify unaligned long-width register reads preserve the full low and high
 * halves of the backing register image.
 */
static void test_unaligned_register_read_preserves_high_half(void **state)
{
    static const struct {
        uint32 stored;
        uint32 expected;
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
        assert_int_equal((uint32)ReadRegU(TEST_DDB_BASE + 1, L_WORD),
                         cases[i].expected);
        assert_int_equal((uint32)ReadRegU(TEST_DDB_BASE + 1, 3),
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

    assert_int_equal((uint32)ReadRegU(VABASE, L_LONG), 0x80015678u);
    assert_int_equal((uint32)ReadRegU(VABASE + 1, L_WORD), 0x80015678u);
    assert_int_equal((uint32)ReadRegU(VABASE + 2, L_WORD), 0x80010000u);
    assert_int_equal((uint32)ReadReg(VABASE, L_LONG), 0x80015678u);
    assert_int_equal((uint32)ReadReg(VABASE + 2, L_WORD), 0x80010000u);
}

/* Verify DDB partial writes preserve high-bit byte and word values. */
static void test_ddb_register_writes_preserve_high_lanes(void **state)
{
    static const struct {
        uint32 addr;
        int32 val;
        int32 lnt;
        uint32 expected;
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
        assert_int_equal((uint32)ReadReg(TEST_DDB_BASE, L_LONG),
                         cases[i].expected);
    }
}

/* Verify unaligned DDB register writes preserve high-bit field groups. */
static void test_ddb_unaligned_register_writes_preserve_high_lanes(
    void **state)
{
    static const struct {
        uint32 addr;
        int32 val;
        int32 lnt;
        uint32 expected;
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
        assert_int_equal((uint32)ReadReg(TEST_DDB_BASE, L_LONG),
                         cases[i].expected);
    }
}

/* Verify direct DDB byte and word writes preserve high-bit lanes. */
static void test_ddb_direct_writes_preserve_high_lanes(void **state)
{
    uint8 bytes[] = {0x78, 0x56, 0x34, 0x80};
    uint8 high_byte = 0x80;
    uint16 high_word = 0x8034;

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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_unaligned_register_read_preserves_high_half),
        cmocka_unit_test(test_word_width_register_reads_preserve_high_half),
        cmocka_unit_test(test_ddb_register_writes_preserve_high_lanes),
        cmocka_unit_test(
            test_ddb_unaligned_register_writes_preserve_high_lanes),
        cmocka_unit_test(test_ddb_direct_writes_preserve_high_lanes),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
