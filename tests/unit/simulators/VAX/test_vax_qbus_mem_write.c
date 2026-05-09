#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"

/*
 * These tests preserve the legacy mapped Qbus memory write behavior at the
 * cqm_wr/qbmem_wr boundary, not just the shared lane helper arithmetic.
 */

#define TEST_MEM_BYTES 0x10000u
#define TEST_MEM_WORDS (TEST_MEM_BYTES >> 2)
#define TEST_TARGET_PA 0x2000u
#define TEST_TARGET_WORD (TEST_TARGET_PA >> 2)
#define TEST_QBUS_PAGE (TEST_TARGET_PA >> VA_V_VPN)
#define TEST_QBUS_MAP_VALID 0x80000000u

#if defined(VAX_630)
t_stat qbmem_wr(int32_t dat, int32_t pa, int32_t md);
#define mapped_qbus_write qbmem_wr
extern int32_t qb_map[];
#else
t_stat cqm_wr(int32_t dat, int32_t pa, int32_t md);
#define mapped_qbus_write cqm_wr
extern int32_t cq_mbr;
#endif

int32_t vc_mem_rd(int32_t pa);
void vc_mem_wr(int32_t pa, int32_t val, int32_t lnt);
int32_t va_mem_rd(int32_t pa);
void va_mem_wr(int32_t pa, int32_t val, int32_t lnt);

typedef struct {
    uint32_t qbus_pa;
    int32_t val;
    int32_t mode;
    uint32_t expected;
} mapped_qbus_write_case;

static uint32_t test_memory[TEST_MEM_WORDS];

uint32_t *M;
uint32_t R[16];
uint32_t PSL;
uint32_t SISR;
uint32_t fault_PC;
uint32_t p1;
uint32_t p2;
uint32_t mchk_ref;
uint32_t trpirq;
uint32_t *vc_buf;
uint32_t *va_buf;
uint32_t va_addr;
jmp_buf save_env;
int32_t hlt_pin;
int32_t mem_err;
int32_t crd_err;
int32_t sys_model;
int32_t ka_mser;
DEVICE cpu_dev;
UNIT cpu_unit;

int32_t ReadReg(uint32_t pa, int32_t lnt)
{
    /* Stubbed register-space read for uncalled legacy helpers. */
    (void)pa;
    (void)lnt;

    return 0;
}

void WriteReg(uint32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed register-space write for uncalled legacy helpers. */
    (void)pa;
    (void)val;
    (void)lnt;
}

int32_t vc_mem_rd(int32_t pa)
{
    /* Stubbed video memory read for uncalled legacy helpers. */
    (void)pa;

    return 0;
}

void vc_mem_wr(int32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed video memory write for uncalled legacy helpers. */
    (void)pa;
    (void)val;
    (void)lnt;
}

int32_t va_mem_rd(int32_t pa)
{
    /* Stubbed video memory read for uncalled legacy helpers. */
    (void)pa;

    return 0;
}

void va_mem_wr(int32_t pa, int32_t val, int32_t lnt)
{
    /* Stubbed video memory write for uncalled legacy helpers. */
    (void)pa;
    (void)val;
    (void)lnt;
}

void init_ubus_tab(void)
{
}

t_stat build_ubus_tab(DEVICE *dptr, DIB *dibp)
{
    /* Stubbed bus table construction for uncalled reset helpers. */
    (void)dptr;
    (void)dibp;

    return SCPE_OK;
}

t_stat set_autocon(UNIT *uptr, int32_t val, const char *cptr, void *desc)
{
    /* Stubbed modifier callback for static modifier tables. */
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;

    return SCPE_OK;
}

t_stat show_autocon(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    /* Stubbed modifier callback for static modifier tables. */
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

t_stat show_iospace(FILE *st, UNIT *uptr, int32_t val, const void *desc)
{
    /* Stubbed modifier callback for static modifier tables. */
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

t_stat show_bus_map(FILE *st, const char *cptr, uint32_t *busmap,
                    uint32_t nmapregs, const char *busname, uint32_t mapvalid)
{
    /* Stubbed map display helper for uncalled show helpers. */
    (void)st;
    (void)cptr;
    (void)busmap;
    (void)nmapregs;
    (void)busname;
    (void)mapvalid;

    return SCPE_OK;
}

static void reset_mapped_qbus_memory(uint32_t initial)
{
    memset(test_memory, 0, sizeof(test_memory));
    M = test_memory;
    cpu_unit.capac = TEST_MEM_BYTES;
    mem_err = 0;
    sys_model = 0;
    test_memory[TEST_TARGET_WORD] = initial;

#if defined(VAX_630)
    qb_map[0] = (int32_t)(TEST_QBUS_MAP_VALID | TEST_QBUS_PAGE);
    ka_mser = 0;
#else
    cq_mbr = 0;
    test_memory[0] = TEST_QBUS_MAP_VALID | TEST_QBUS_PAGE;
#endif
}

/* Verify mapped Qbus word writes preserve legacy memory-lane behavior. */
static void test_mapped_qbus_word_writes_preserve_behavior(void **state)
{
    static const mapped_qbus_write_case cases[] = {
        {0, 0xabcd, WRITE, 0x1234abcdu},
        {2, 0xffff, WRITE, 0xffff5678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_mapped_qbus_memory(0x12345678u);
        assert_int_equal(mapped_qbus_write(cases[i].val,
                                           (int32_t)cases[i].qbus_pa,
                                           cases[i].mode),
                         SCPE_OK);
        assert_int_equal(test_memory[TEST_TARGET_WORD], cases[i].expected);
    }
}

/* Verify mapped Qbus byte writes preserve legacy memory-lane behavior. */
static void test_mapped_qbus_byte_writes_preserve_behavior(void **state)
{
    static const mapped_qbus_write_case cases[] = {
        {0, 0xa5, WRITEB, 0x123456a5u},
        {1, 0xa5, WRITEB, 0x1234a578u},
        {2, 0xa5, WRITEB, 0x12a55678u},
        {3, 0xa5, WRITEB, 0xa5345678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_mapped_qbus_memory(0x12345678u);
        assert_int_equal(mapped_qbus_write(cases[i].val,
                                           (int32_t)cases[i].qbus_pa,
                                           cases[i].mode),
                         SCPE_OK);
        assert_int_equal(test_memory[TEST_TARGET_WORD], cases[i].expected);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mapped_qbus_word_writes_preserve_behavior),
        cmocka_unit_test(test_mapped_qbus_byte_writes_preserve_behavior),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
