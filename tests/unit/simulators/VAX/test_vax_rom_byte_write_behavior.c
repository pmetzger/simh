#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sim_types.h"
#include "test_cmocka.h"

#include "vax_defs.h"
#include "vax_cpu.h"
#include "vax_cpu1.h"
#include "vax_watch.h"
#if defined(TEST_VAX630_SYSDEV)
#include "pdp11_io_lib.h"
#include "vax630_io.h"
#include "vax630_stddev.h"
#include "vax630_sysdev_internal.h"
#else
#include "vax4nn_stddev.h"
#include "vax4nn_stddev_internal.h"
#endif

/*
 * These tests preserve legacy VAX ROM byte-write lane behavior at the real
 * ROM byte writer used while patching model-specific boot ROM images.
 */

static uint32_t test_rom[ROMSIZE >> 2];

int32_t wtc_rd(int32_t rg)
{
    /* Stubbed watch-chip read for tests that target ROM byte writes. */
    (void)rg;

    return 0;
}

void wtc_wr(int32_t rg, int32_t val)
{
    /* Stubbed watch-chip write for tests that target ROM byte writes. */
    (void)rg;
    (void)val;
}

void wtc_set_valid(void)
{
    /* Stubbed watch-chip state update for tests that target ROM byte writes. */
}

void wtc_set_invalid(void)
{
    /* Stubbed watch-chip state update for tests that target ROM byte writes. */
}

#if defined(TEST_VAX630_SYSDEV)
int32_t qbmem_rd(int32_t pa, int32_t lnt);
void qbmem_wr(int32_t pa, int32_t val, int32_t lnt);
int32_t todr_rd(void);
void todr_wr(int32_t dat);

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
int32_t pcq_p;
int32_t in_ie;
int32_t ibcnt;
int32_t ppc;
int32_t mapen;
TLBENT stlb[VA_TBSIZE];
TLBENT ptlb[VA_TBSIZE];
DEVICE cpu_dev;
UNIT cpu_unit;
UNIT clk_unit;
int32_t tmr_poll;
DEVICE va_dev;
DEVICE vc_dev;
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

t_stat show_mapped_addr(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    /* Stubbed modifier callback for static modifier tables. */
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
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

int32_t intexc(int32_t vec, int32_t cc, int32_t ipl, int ei)
{
    /* Stubbed interrupt helper for uncalled legacy paths. */
    (void)vec;
    (void)cc;
    (void)ipl;
    (void)ei;

    return 0;
}

int32_t qbmap_rd(int32_t pa, int32_t lnt)
{
    /* Stubbed Qbus map read for uncalled register dispatch entries. */
    (void)pa;
    (void)lnt;

    return 0;
}

void qbmap_wr(int32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed Qbus map write for uncalled register dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
}

int32_t qbmem_rd(int32_t pa, int32_t lnt)
{
    /* Stubbed Qbus memory read for uncalled register dispatch entries. */
    (void)pa;
    (void)lnt;

    return 0;
}

void qbmem_wr(int32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed Qbus memory write for uncalled register dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
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

int32_t iccs_rd(void)
{
    return 0;
}

int32_t todr_rd(void)
{
    return 0;
}

int32_t rxcs_rd(void)
{
    return 0;
}

int32_t rxdb_rd(void)
{
    return 0;
}

int32_t txcs_rd(void)
{
    return 0;
}

void iccs_wr(int32_t dat)
{
    /* Stubbed interval clock write for uncalled legacy paths. */
    (void)dat;
}

void todr_wr(int32_t dat)
{
    /* Stubbed TODR write for uncalled legacy paths. */
    (void)dat;
}

void rxcs_wr(int32_t dat)
{
    /* Stubbed console receiver status write for uncalled legacy paths. */
    (void)dat;
}

void txcs_wr(int32_t dat)
{
    /* Stubbed console transmitter status write for uncalled legacy paths. */
    (void)dat;
}

void txdb_wr(int32_t dat)
{
    /* Stubbed console transmitter data write for uncalled legacy paths. */
    (void)dat;
}

void ioreset_wr(int32_t dat)
{
    /* Stubbed I/O reset write for uncalled legacy paths. */
    (void)dat;
}

void cpu_idle(void)
{
    /* Stubbed idle hook for uncalled halt paths. */
}
#else
int32_t nar_rd(int32_t pa)
{
    /* Stubbed NAR read for uncalled I/O paths. */
    (void)pa;

    return 0;
}
#endif

static void reset_rom_byte_write_state(void)
{
    memset(test_rom, 0, sizeof(test_rom));
    rom = test_rom;
    rom_unit.flags |= (1u << UNIT_V_UF);
}

static uint32_t read_rom_word(void)
{
#if defined(TEST_VAX630_SYSDEV)
    return (uint32_t)rom_rd((int32_t)ROMBASE, L_LONG);
#else
    return (uint32_t)rom_rd((int32_t)ROMBASE);
#endif
}

/* Verify ROM byte writes preserve legacy byte-lane behavior. */
static void test_rom_byte_write_preserves_legacy_lanes(void **state)
{
    static const struct {
        uint32_t pa;
        int32_t val;
        uint32_t expected;
    } cases[] = {
        {ROMBASE, 0xa5, 0x123456a5u},
        {ROMBASE + 1, 0xa5, 0x1234a578u},
        {ROMBASE + 2, 0xa5, 0x12a55678u},
        {ROMBASE + 3, 0xa5, 0xa5345678u},
        {ROMBASE + 3, 0x1a5, 0xa5345678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_rom_byte_write_state();
        test_rom[0] = 0x12345678u;
        rom_wr_B((int32_t)cases[i].pa, cases[i].val);
        assert_int_equal(test_rom[0], cases[i].expected);
        assert_int_equal(read_rom_word(), cases[i].expected);
    }
}

#if defined(TEST_VAX630_SYSDEV)
/* Verify unaligned register reads preserve legacy high-half composition. */
static void test_unaligned_read_preserves_high_half(void **state)
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
        reset_rom_byte_write_state();
        test_rom[0] = cases[i].stored;
        assert_int_equal((uint32_t)ReadRegU(ROMBASE + 1, L_WORD),
                         cases[i].expected);
        assert_int_equal((uint32_t)ReadRegU(ROMBASE + 1, 3), cases[i].expected);
    }
}
#endif

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rom_byte_write_preserves_legacy_lanes),
#if defined(TEST_VAX630_SYSDEV)
        cmocka_unit_test(test_unaligned_read_preserves_high_half),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
