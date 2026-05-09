#include <stddef.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"

/*
 * These tests preserve legacy VAX ROM byte-write lane behavior at the real
 * ROM byte writer used while patching model-specific boot ROM images.
 */

void rom_wr_B(int32 pa, int32 val);

#if defined(TEST_VAX630_SYSDEV)
int32 rom_rd(int32 pa, int32 lnt);
int32 ReadRegU(uint32 pa, int32 lnt);
#else
int32 rom_rd(int32 pa);
#endif

extern uint32 *rom;
extern UNIT rom_unit;

static uint32 test_rom[ROMSIZE >> 2];

int32 wtc_rd(int32 rg);
void wtc_wr(int32 rg, int32 val);
void wtc_set_valid(void);
void wtc_set_invalid(void);

int32 wtc_rd(int32 rg)
{
    /* Stubbed watch-chip read for tests that target ROM byte writes. */
    (void)rg;

    return 0;
}

void wtc_wr(int32 rg, int32 val)
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
TLBENT fill(uint32 va, int32 lnt, int32 acc, int32 *stat);
t_stat show_mapped_addr(FILE *st, UNIT *uptr, int32 val, const void *desc);
t_stat cpu_load_bootcode(const char *filename,
                         const unsigned char *builtin_code, size_t size,
                         t_bool load_rom, t_addr offset);
int32 intexc(int32 vec, int32 cc, int32 ipl, int ei);
int32 qbmap_rd(int32 pa, int32 lnt);
void qbmap_wr(int32 pa, int32 val, int32 lnt);
int32 qbmem_rd(int32 pa, int32 lnt);
void qbmem_wr(int32 pa, int32 val, int32 lnt);
void WriteIO(uint32 pa, int32 val, int32 lnt);
void WriteIOU(uint32 pa, int32 val, int32 lnt);
int32 iccs_rd(void);
int32 todr_rd(void);
int32 rxcs_rd(void);
int32 rxdb_rd(void);
int32 txcs_rd(void);
void iccs_wr(int32 dat);
void todr_wr(int32 dat);
void rxcs_wr(int32 dat);
void txcs_wr(int32 dat);
void txdb_wr(int32 dat);
void ioreset_wr(int32 dat);
void cpu_idle(void);

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
int32 tmr_poll;
DEVICE va_dev;
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

t_stat show_mapped_addr(FILE *st, UNIT *uptr, int32 val, const void *desc)
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

int32 qbmap_rd(int32 pa, int32 lnt)
{
    /* Stubbed Qbus map read for uncalled register dispatch entries. */
    (void)pa;
    (void)lnt;

    return 0;
}

void qbmap_wr(int32 pa, int32 val, int32 lnt)
{
    /* Stubbed Qbus map write for uncalled register dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
}

int32 qbmem_rd(int32 pa, int32 lnt)
{
    /* Stubbed Qbus memory read for uncalled register dispatch entries. */
    (void)pa;
    (void)lnt;

    return 0;
}

void qbmem_wr(int32 pa, int32 val, int32 lnt)
{
    /* Stubbed Qbus memory write for uncalled register dispatch entries. */
    (void)pa;
    (void)val;
    (void)lnt;
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
    /* Stubbed idle hook for uncalled halt paths. */
}
#else
int32 nar_rd(int32 pa);

int32 nar_rd(int32 pa)
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

static uint32 read_rom_word(void)
{
#if defined(TEST_VAX630_SYSDEV)
    return (uint32)rom_rd((int32)ROMBASE, L_LONG);
#else
    return (uint32)rom_rd((int32)ROMBASE);
#endif
}

/* Verify ROM byte writes preserve legacy byte-lane behavior. */
static void test_rom_byte_write_preserves_legacy_lanes(void **state)
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
        {ROMBASE + 3, 0x1a5, 0xa5345678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_rom_byte_write_state();
        test_rom[0] = 0x12345678u;
        rom_wr_B((int32)cases[i].pa, cases[i].val);
        assert_int_equal(test_rom[0], cases[i].expected);
        assert_int_equal(read_rom_word(), cases[i].expected);
    }
}

#if defined(TEST_VAX630_SYSDEV)
/* Verify unaligned register reads preserve legacy high-half composition. */
static void test_unaligned_read_preserves_high_half(void **state)
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
        reset_rom_byte_write_state();
        test_rom[0] = cases[i].stored;
        assert_int_equal((uint32)ReadRegU(ROMBASE + 1, L_WORD),
                         cases[i].expected);
        assert_int_equal((uint32)ReadRegU(ROMBASE + 1, 3), cases[i].expected);
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
