#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"

/*
 * These tests preserve the legacy VAX 3900 system-device lane write behavior
 * at the real ROM, NVR, register, CMCTL, CDG, and SSC entry points.
 */

#define TEST_SSC_TNIR0_PA (SSCBASE + (0x42u << 2))

int32 rom_rd(int32 pa);
void rom_wr_B(int32 pa, int32 val);
int32 nvr_rd(int32 pa);
void nvr_wr(int32 pa, int32 val, int32 lnt);
void WriteRegU(uint32 pa, int32 val, int32 lnt);
void cmctl_wr(int32 pa, int32 val, int32 lnt);
void cdg_wr(int32 pa, int32 val, int32 lnt);
void ssc_wr(int32 pa, int32 val, int32 lnt);

extern uint32 *rom;
extern uint32 *nvr;
extern int32 cmctl_reg[];
extern int32 cdg_dat[];
extern uint32 tmr_tnir[];

static uint32 test_rom[ROMSIZE >> 2];
static uint32 test_nvr[NVRSIZE >> 2];

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
int32 int_req[IPL_HLVL];
TLBENT stlb[VA_TBSIZE];
TLBENT ptlb[VA_TBSIZE];
DEVICE cpu_dev;
UNIT cpu_unit;
UNIT clk_unit;
int32 tmr_poll;
DEVICE vc_dev;
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

t_stat show_mapped_addr(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    /* Stubbed modifier callback for static modifier tables. */
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
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

int32 intexc(int32 vec, int32 cc, int32 ipl, int ei)
{
    /* Stubbed interrupt helper for uncalled legacy paths. */
    (void)vec;
    (void)cc;
    (void)ipl;
    (void)ei;

    return 0;
}

int32 cqmap_rd(int32 pa)
{
    /* Stubbed Qbus map read for uncalled register dispatch entries. */
    (void)pa;

    return 0;
}

void cqmap_wr(int32 pa, int32 val, int32 lnt)
{
    /* Stubbed Qbus map write for uncalled register dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
}

int32 cqipc_rd(int32 pa)
{
    /* Stubbed Qbus IPC read for uncalled register dispatch entries. */
    (void)pa;

    return 0;
}

void cqipc_wr(int32 pa, int32 val, int32 lnt)
{
    /* Stubbed Qbus IPC write for uncalled register dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
}

int32 cqbic_rd(int32 pa)
{
    /* Stubbed CQBIC read for uncalled register dispatch entries. */
    (void)pa;

    return 0;
}

void cqbic_wr(int32 pa, int32 val, int32 lnt)
{
    /* Stubbed CQBIC write for uncalled register dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
}

int32 iccs_rd(void)
{
    return 0;
}

int32 todr_rd(void)
{
    return 0;
}

int32 rxcs_rd(void)
{
    return 0;
}

int32 rxdb_rd(void)
{
    return 0;
}

int32 txcs_rd(void)
{
    return 0;
}

void iccs_wr(int32 dat)
{
    /* Stubbed interval clock write for uncalled legacy paths. */
    (void)dat;
}

void todr_wr(int32 dat)
{
    /* Stubbed TODR write for uncalled legacy paths. */
    (void)dat;
}

void rxcs_wr(int32 dat)
{
    /* Stubbed console receiver status write for uncalled legacy paths. */
    (void)dat;
}

void txcs_wr(int32 dat)
{
    /* Stubbed console transmitter status write for uncalled legacy paths. */
    (void)dat;
}

void txdb_wr(int32 dat)
{
    /* Stubbed console transmitter data write for uncalled legacy paths. */
    (void)dat;
}

void ioreset_wr(int32 dat)
{
    /* Stubbed I/O reset write for uncalled legacy paths. */
    (void)dat;
}

void cpu_idle(void)
{
}

static void reset_sysdev_behavior_state(void)
{
    memset(test_rom, 0, sizeof(test_rom));
    memset(test_nvr, 0, sizeof(test_nvr));
    memset(cmctl_reg, 0, CMCTLSIZE);
    memset(cdg_dat, 0, CDASIZE);
    memset(tmr_tnir, 0, sizeof(uint32) * 2);
    rom = test_rom;
    nvr = test_nvr;
}

/* Verify the ROM byte writer preserves legacy byte-lane behavior. */
static void test_rom_write_byte_preserves_legacy_behavior(void **state)
{
    static const struct {
        uint32 pa;
        int32 val;
        uint32 expected;
    } cases[] = {
        {ROMBASE, 0xa5, 0x123456a5u},
        {ROMBASE + 1, 0xa5, 0x1234a578u},
        {ROMBASE + 2, 0xa5, 0x12a55678u},
        {ROMBASE + 3, 0xa5, 0xa5345678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sysdev_behavior_state();
        test_rom[0] = 0x12345678u;
        rom_wr_B((int32)cases[i].pa, cases[i].val);
        assert_int_equal(test_rom[0], cases[i].expected);
    }
}

/* Verify NVR writes preserve legacy byte, word, and longword behavior. */
static void test_nvr_write_preserves_legacy_behavior(void **state)
{
    static const struct {
        uint32 pa;
        int32 val;
        int32 lnt;
        uint32 expected;
    } cases[] = {
        {NVRBASE, 0xa5, L_BYTE, 0x123456a5u},
        {NVRBASE + 1, 0xa5, L_BYTE, 0x1234a578u},
        {NVRBASE + 2, 0xa5, L_BYTE, 0x12a55678u},
        {NVRBASE + 3, 0xa5, L_BYTE, 0xa5345678u},
        {NVRBASE, 0x1d617, L_WORD, 0x1234d617u},
        {NVRBASE + 2, 0xffff, L_WORD, 0xffff5678u},
        {NVRBASE, (int32)0x87654321u, L_LONG, 0x87654321u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sysdev_behavior_state();
        test_nvr[0] = 0x12345678u;
        nvr_wr((int32)cases[i].pa, cases[i].val, cases[i].lnt);
        assert_int_equal(test_nvr[0], cases[i].expected);
        assert_int_equal((uint32)nvr_rd((int32)NVRBASE), cases[i].expected);
    }
}

/* Verify unaligned register writes preserve legacy read-modify-write state. */
static void test_writeregu_preserves_legacy_nvr_behavior(void **state)
{
    static const struct {
        uint32 pa;
        int32 val;
        int32 lnt;
        uint32 expected;
    } cases[] = {
        {NVRBASE + 3, 0x1a5, L_BYTE, 0xa5345678u},
        {NVRBASE + 2, 0x1d617, L_WORD, 0xd6175678u},
        {NVRBASE + 1, 0x1d617a5, 3, 0xd617a578u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sysdev_behavior_state();
        test_nvr[0] = 0x12345678u;
        WriteRegU(cases[i].pa, cases[i].val, cases[i].lnt);
        assert_int_equal(test_nvr[0], cases[i].expected);
    }
}

/* Verify CMCTL partial writes preserve legacy lane positioning. */
static void test_cmctl_partial_write_preserves_legacy_behavior(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_sysdev_behavior_state();

    cmctl_wr(CMCTLBASE + 3, 0x80, L_BYTE);

    assert_int_equal((uint32)cmctl_reg[0], 0x80000000u);
}

/*
 * Verify CMCTL partial writes mask the source value to the requested byte or
 * word width before placing it in the addressed field.
 */
static void test_cmctl_partial_write_masks_source_value(void **state)
{
    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_sysdev_behavior_state();

    cmctl_wr(CMCTLBASE + 2, 0x1ff0, L_BYTE);

    assert_int_equal((uint32)cmctl_reg[0], 0x00f00000u);
}

/* Verify CDG partial writes preserve legacy byte-lane behavior. */
static void test_cdg_partial_write_preserves_legacy_behavior(void **state)
{
    static const struct {
        uint32 pa;
        int32 val;
        int32 lnt;
        uint32 expected;
    } cases[] = {
        {CDGBASE, 0xa5, L_BYTE, 0x123456a5u},
        {CDGBASE + 1, 0xa5, L_BYTE, 0x1234a578u},
        {CDGBASE + 2, 0xa5, L_BYTE, 0x12a55678u},
        {CDGBASE + 3, 0xa5, L_BYTE, 0xa5345678u},
        {CDGBASE, 0x1d617, L_WORD, 0x1234d617u},
        {CDGBASE + 2, 0x1d617, L_WORD, 0xd6175678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sysdev_behavior_state();
        cdg_dat[0] = 0x12345678;
        cdg_wr((int32)cases[i].pa, cases[i].val, cases[i].lnt);
        assert_int_equal((uint32)cdg_dat[0], cases[i].expected);
    }
}

/* Verify SSC partial writes preserve legacy byte-lane behavior. */
static void test_ssc_partial_write_preserves_legacy_behavior(void **state)
{
    static const struct {
        uint32 pa;
        int32 val;
        int32 lnt;
        uint32 expected;
    } cases[] = {
        {TEST_SSC_TNIR0_PA, 0xa5, L_BYTE, 0x123456a5u},
        {TEST_SSC_TNIR0_PA + 1, 0xa5, L_BYTE, 0x1234a578u},
        {TEST_SSC_TNIR0_PA + 2, 0xa5, L_BYTE, 0x12a55678u},
        {TEST_SSC_TNIR0_PA + 3, 0xa5, L_BYTE, 0xa5345678u},
        {TEST_SSC_TNIR0_PA, 0x1d617, L_WORD, 0x1234d617u},
        {TEST_SSC_TNIR0_PA + 2, 0x1d617, L_WORD, 0xd6175678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_sysdev_behavior_state();
        tmr_tnir[0] = 0x12345678u;
        ssc_wr((int32)cases[i].pa, cases[i].val, cases[i].lnt);
        assert_int_equal(tmr_tnir[0], cases[i].expected);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rom_write_byte_preserves_legacy_behavior),
        cmocka_unit_test(test_nvr_write_preserves_legacy_behavior),
        cmocka_unit_test(test_writeregu_preserves_legacy_nvr_behavior),
        cmocka_unit_test(test_cmctl_partial_write_preserves_legacy_behavior),
        cmocka_unit_test(test_cmctl_partial_write_masks_source_value),
        cmocka_unit_test(test_cdg_partial_write_preserves_legacy_behavior),
        cmocka_unit_test(test_ssc_partial_write_preserves_legacy_behavior),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
