#include <stddef.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"

/*
 * These tests preserve legacy KA43 system-device lane behavior at the real
 * disk data buffer, cache diagnostic, and KA register entry points.
 */

void ddb_WriteB(uint32 ba, uint32 bc, uint8 *buf);
void ddb_WriteW(uint32 ba, uint32 bc, uint16 *buf);
void ddb_ReadB(uint32 ba, uint32 bc, uint8 *buf);
void ddb_ReadW(uint32 ba, uint32 bc, uint16 *buf);
int32 ReadReg(uint32 pa, int32 lnt);
void WriteReg(uint32 pa, int32 val, int32 lnt);
void WriteRegU(uint32 pa, int32 val, int32 lnt);
int32 ka_rd(int32 pa);

extern uint32 *ddb;
extern int32 cdg_dat[];
extern int32 int_mask;
extern int32 int_req[];
extern t_bool tmr_inst;
extern uint32 tmr_tir;

static uint32 test_ddb[D128SIZE >> 2];

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
int32 pcq_p;
int32 in_ie;
int32 ibcnt;
int32 ppc;
int32 mapen;
TLBENT stlb[VA_TBSIZE];
TLBENT ptlb[VA_TBSIZE];
DEVICE cpu_dev;
UNIT cpu_unit;
UNIT clk_unit;
int32 hlt_pin;
int32 tmr_int;
int32 tmr_poll;
uint32 trpirq;
uint32 vc_org;
uint32 vc_sel;
DEVICE vc_dev;
DEVICE ve_dev;
DEVICE lk_dev;
DEVICE vs_dev;
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

void WriteIO(uint32 pa, int32 val, int32 lnt)
{
    /* Stubbed I/O write for uncalled memory access paths. */
    (void)pa;
    (void)val;
    (void)lnt;
}

void WriteIOU(uint32 pa, int32 val, int32 lnt)
{
    /* Stubbed unaligned I/O write for uncalled memory access paths. */
    (void)pa;
    (void)val;
    (void)lnt;
}

t_stat show_vec(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    /* Stubbed modifier callback for static modifier tables. */
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
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

int32 intexc(int32 vec, int32 cc, int32 ipl, int ei)
{
    /* Stubbed interrupt helper for uncalled legacy paths. */
    (void)vec;
    (void)cc;
    (void)ipl;
    (void)ei;

    return 0;
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
STUB_WRITE(ve_wr)
STUB_WRITE(nvr_wr)
STUB_WRITE(rz_wr)

int32 iccs_rd(void)
{
    return 0;
}

void iccs_wr(int32 data)
{
    /* Stubbed interval clock write for uncalled legacy paths. */
    (void)data;
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

void cpu_idle(void) {}

static void reset_vax43_sysdev_behavior_state(void)
{
    memset(test_ddb, 0, sizeof(test_ddb));
    memset(cdg_dat, 0, CDASIZE);
    int_req[0] = 0;
    int_mask = 0;
    tmr_inst = FALSE;
    tmr_tir = 0;
    vc_org = 0;
    vc_sel = 0;
    ddb = test_ddb;
}

/* Verify register byte writes preserve legacy DDB byte-lane behavior. */
static void test_ddb_register_byte_write_preserves_behavior(void **state)
{
    static const struct {
        uint32 addr;
        uint32 expected;
    } cases[] = {
        {D128BASE, 0x123456a5u},
        {D128BASE + 1, 0x1234a578u},
        {D128BASE + 2, 0x12a55678u},
        {D128BASE + 3, 0xa5345678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_vax43_sysdev_behavior_state();
        test_ddb[0] = 0x12345678u;
        WriteReg(cases[i].addr, 0xa5, L_BYTE);
        assert_int_equal(test_ddb[0], cases[i].expected);
        assert_int_equal((uint32)ReadReg(D128BASE, L_LONG), cases[i].expected);
    }
}

/* Verify register word writes preserve legacy DDB word-lane behavior. */
static void test_ddb_register_word_write_preserves_behavior(void **state)
{
    static const struct {
        uint32 addr;
        uint32 expected;
    } cases[] = {
        {D128BASE, 0x1234d617u},
        {D128BASE + 2, 0xd6175678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_vax43_sysdev_behavior_state();
        test_ddb[0] = 0x12345678u;
        WriteReg(cases[i].addr, 0xd617, L_WORD);
        assert_int_equal(test_ddb[0], cases[i].expected);
        assert_int_equal((uint32)ReadReg(D128BASE, L_LONG), cases[i].expected);
    }
}

/* Verify register writes mask source values to the requested lane width. */
static void test_ddb_register_write_masks_source_value(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_vax43_sysdev_behavior_state();
    test_ddb[0] = 0x12345678u;
    WriteReg(D128BASE + 2, 0x1d617, L_WORD);
    assert_int_equal(test_ddb[0], 0xd6175678u);

    reset_vax43_sysdev_behavior_state();
    test_ddb[0] = 0x12345678u;
    WriteReg(D128BASE + 3, 0x1a5, L_BYTE);
    assert_int_equal(test_ddb[0], 0xa5345678u);
}

/* Verify unaligned register writes preserve legacy DDB lane behavior. */
static void test_ddb_register_unaligned_write_preserves_behavior(void **state)
{
    static const struct {
        uint32 addr;
        int32 val;
        int32 lnt;
        uint32 expected;
    } cases[] = {
        {D128BASE + 3, 0x1a5, L_BYTE, 0xa5345678u},
        {D128BASE + 2, 0x1d617, L_WORD, 0xd6175678u},
        {D128BASE + 1, 0x1d617a5, 3, 0xd617a578u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_vax43_sysdev_behavior_state();
        test_ddb[0] = 0x12345678u;
        WriteRegU(cases[i].addr, cases[i].val, cases[i].lnt);
        assert_int_equal(test_ddb[0], cases[i].expected);
    }
}

/* Verify CDG partial writes preserve legacy byte-lane behavior. */
static void test_cdg_partial_write_preserves_behavior(void **state)
{
    static const struct {
        uint32 addr;
        int32 val;
        int32 lnt;
        uint32 expected;
    } cases[] = {
        {CDGBASE, 0xa5, L_BYTE, 0x123456a5u},
        {CDGBASE + 1, 0xa5, L_BYTE, 0x1234a578u},
        {CDGBASE + 2, 0xa5, L_BYTE, 0x12a55678u},
        {CDGBASE + 3, 0xa5, L_BYTE, 0xa5345678u},
        {CDGBASE, 0xd617, L_WORD, 0x1234d617u},
        {CDGBASE + 2, 0xd617, L_WORD, 0xd6175678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_vax43_sysdev_behavior_state();
        cdg_dat[0] = 0x12345678;
        WriteReg(cases[i].addr, cases[i].val, cases[i].lnt);
        assert_int_equal((uint32)cdg_dat[0], cases[i].expected);
    }
}

/* Verify KA register reads preserve legacy high-lane bit composition. */
static void test_ka_register_read_preserves_high_lane_behavior(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_vax43_sysdev_behavior_state();
    int_req[0] = 0x91;
    vc_sel = 1;
    vc_org = 0x7e;
    int_mask = 0xa5;

    assert_int_equal((uint32)ka_rd(KABASE + (3u << 2)), 0x91017ea5u);
}

/* Verify KA timer writes preserve the effective high-half interval field. */
static void test_ka_timer_write_preserves_high_half_behavior(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_vax43_sysdev_behavior_state();

    WriteReg(KABASE + (7u << 2), (int32)0xe0ad0000u, L_LONG);

    /* The effective timer interval is the low 16 bits of tmr_tir. */
    assert_int_equal(tmr_tir & 0xffffu, 0xe0adu);
}

/* Verify DDB byte writes preserve legacy byte-lane behavior. */
static void test_ddb_writeb_preserves_behavior(void **state)
{
    static const struct {
        uint32 addr;
        uint32 expected;
    } cases[] = {
        {0, 0x123456a5u},
        {1, 0x1234a578u},
        {2, 0x12a55678u},
        {3, 0xa5345678u},
    };
    uint8 full_word[] = {0x78, 0x56, 0x34, 0x80};
    uint8 byte = 0xa5;

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_vax43_sysdev_behavior_state();
        test_ddb[0] = 0x12345678u;
        ddb_WriteB(cases[i].addr, 1, &byte);
        assert_int_equal(test_ddb[0], cases[i].expected);
    }

    reset_vax43_sysdev_behavior_state();
    ddb_WriteB(0, sizeof(full_word), full_word);
    assert_int_equal(test_ddb[0], 0x80345678u);
}

/* Verify DDB word writes preserve legacy word-lane behavior. */
static void test_ddb_writew_preserves_behavior(void **state)
{
    static const struct {
        uint32 addr;
        uint32 expected;
    } cases[] = {
        {0, 0x1234d617u},
        {2, 0xd6175678u},
    };
    uint16 full_word[] = {0x5678, 0x8034};
    uint16 word = 0xd617;

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_vax43_sysdev_behavior_state();
        test_ddb[0] = 0x12345678u;
        ddb_WriteW(cases[i].addr, 2, &word);
        assert_int_equal(test_ddb[0], cases[i].expected);
    }

    reset_vax43_sysdev_behavior_state();
    ddb_WriteW(0, sizeof(full_word), full_word);
    assert_int_equal(test_ddb[0], 0x80345678u);
}

/* Verify DDB byte and word reads preserve legacy lane extraction. */
static void test_ddb_reads_preserve_behavior(void **state)
{
    uint8 byte_buf[4] = {0};
    uint16 word_buf[2] = {0};

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_vax43_sysdev_behavior_state();
    test_ddb[0] = 0x80345678u;

    ddb_ReadB(0, sizeof(byte_buf), byte_buf);
    assert_int_equal(byte_buf[0], 0x78);
    assert_int_equal(byte_buf[1], 0x56);
    assert_int_equal(byte_buf[2], 0x34);
    assert_int_equal(byte_buf[3], 0x80);

    ddb_ReadW(0, sizeof(word_buf), word_buf);
    assert_int_equal(word_buf[0], 0x5678);
    assert_int_equal(word_buf[1], 0x8034);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ddb_register_word_write_preserves_behavior),
        cmocka_unit_test(test_ddb_register_byte_write_preserves_behavior),
        cmocka_unit_test(test_ddb_register_write_masks_source_value),
        cmocka_unit_test(test_ddb_register_unaligned_write_preserves_behavior),
        cmocka_unit_test(test_cdg_partial_write_preserves_behavior),
        cmocka_unit_test(test_ka_register_read_preserves_high_lane_behavior),
        cmocka_unit_test(test_ka_timer_write_preserves_high_half_behavior),
        cmocka_unit_test(test_ddb_writeb_preserves_behavior),
        cmocka_unit_test(test_ddb_writew_preserves_behavior),
        cmocka_unit_test(test_ddb_reads_preserve_behavior),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
