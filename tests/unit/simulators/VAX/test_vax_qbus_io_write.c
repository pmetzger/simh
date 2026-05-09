#include <stddef.h>
#include <stdint.h>

#include "test_cmocka.h"

#include "vax_qbus_internal.h"

/*
 * These tests preserve the legacy WriteIO/WriteIOU contract by recording
 * the exact WriteQb operations that the pre-cleanup code emitted.
 */

void WriteIO(uint32_t pa, int32_t val, int32_t lnt);
void WriteIOU(uint32_t pa, int32_t val, int32_t lnt);
int32_t va_mem_rd(int32_t pa);
void va_mem_wr(int32_t pa, int32_t val, int32_t lnt);

typedef struct {
    uint32_t pa;
    int32_t val;
    int32_t mode;
} recorded_qbus_write;

typedef void (*qbus_io_write_fn)(uint32_t pa, int32_t val, int32_t lnt);

typedef struct {
    qbus_io_write_fn write;
    uint32_t pa;
    int32_t val;
    int32_t lnt;
    size_t expected_count;
    recorded_qbus_write expected[2];
} qbus_io_write_case;

static recorded_qbus_write recorded_writes[2];
static size_t recorded_write_count;

uint32_t trpirq;
uint32_t *M;
uint32_t R[16];
uint32_t PSL;
uint32_t SISR;
uint32_t fault_PC;
uint32_t p1;
uint32_t p2;
uint32_t mchk_ref;
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

void vax_qbus_test_record_write(uint32_t pa, int32_t val, int32_t mode)
{
    assert_true(recorded_write_count < sizeof(recorded_writes) /
                                           sizeof(recorded_writes[0]));
    recorded_writes[recorded_write_count].pa = pa;
    recorded_writes[recorded_write_count].val = val;
    recorded_writes[recorded_write_count].mode = mode;
    recorded_write_count++;
}

static void reset_recorded_writes(void)
{
    recorded_write_count = 0;
}

static void assert_recorded_write(size_t index, uint32_t pa, int32_t val,
                                  int32_t mode)
{
    assert_true(index < recorded_write_count);
    assert_int_equal(recorded_writes[index].pa, pa);
    assert_int_equal(recorded_writes[index].val, val);
    assert_int_equal(recorded_writes[index].mode, mode);
}

static void assert_recorded_writes(const recorded_qbus_write *expected,
                                   size_t expected_count)
{
    assert_int_equal(recorded_write_count, expected_count);
    for (size_t i = 0; i < expected_count; i++)
        assert_recorded_write(i, expected[i].pa, expected[i].val,
                              expected[i].mode);
}

static void run_write_cases(const qbus_io_write_case *cases, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        reset_recorded_writes();
        cases[i].write(cases[i].pa, cases[i].val, cases[i].lnt);
        assert_recorded_writes(cases[i].expected, cases[i].expected_count);
        }
}

/* Verify aligned WriteIO cases preserve the legacy WriteQb sequence. */
static void test_writeio_records_legacy_sequences(void **state)
{
    static const qbus_io_write_case cases[] = {
        {WriteIO, 0x1003, (int32_t)0xff0012a5u, L_BYTE, 1,
         {{0x1003, (int32_t)0xff0012a5u, WRITEB}}},
        {WriteIO, 0x1002, (int32_t)0xff001234u, L_WORD, 1,
         {{0x1002, (int32_t)0xff001234u, WRITE}}},
        {WriteIO, 0x1000, (int32_t)0xff001234u, L_LONG, 2,
         {{0x1000, 0x1234, WRITE}, {0x1002, 0xff00, WRITE}}},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    run_write_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

/* Verify WriteIOU byte writes preserve the legacy WriteQb sequence. */
static void test_writeiou_byte_records_legacy_sequences(void **state)
{
    static const qbus_io_write_case cases[] = {
        {WriteIOU, 0x1000, (int32_t)0xff0012a5u, L_BYTE, 1,
         {{0x1000, 0xa5, WRITEB}}},
        {WriteIOU, 0x1001, (int32_t)0xff0012a5u, L_BYTE, 1,
         {{0x1001, 0xa5, WRITEB}}},
        {WriteIOU, 0x1002, (int32_t)0xff0012a5u, L_BYTE, 1,
         {{0x1002, 0xa5, WRITEB}}},
        {WriteIOU, 0x1003, (int32_t)0xff0012a5u, L_BYTE, 1,
         {{0x1003, 0xa5, WRITEB}}},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    run_write_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

/* Verify WriteIOU word writes preserve the legacy WriteQb sequence. */
static void test_writeiou_word_records_legacy_sequences(void **state)
{
    static const qbus_io_write_case cases[] = {
        {WriteIOU, 0x1000, (int32_t)0xff001234u, L_WORD, 1,
         {{0x1000, 0x1234, WRITE}}},
        {WriteIOU, 0x1001, (int32_t)0xff001234u, L_WORD, 2,
         {{0x1001, 0x34, WRITEB}, {0x1002, 0x12, WRITEB}}},
        {WriteIOU, 0x1002, (int32_t)0xff001234u, L_WORD, 1,
         {{0x1002, 0x1234, WRITE}}},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    run_write_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

/* Verify WriteIOU tribyte writes preserve the legacy WriteQb sequence. */
static void test_writeiou_tribyte_records_legacy_sequences(void **state)
{
    static const qbus_io_write_case cases[] = {
        {WriteIOU, 0x1000, (int32_t)0x00ff1234u, 3, 2,
         {{0x1000, 0x1234, WRITE}, {0x1002, 0xff, WRITEB}}},
        {WriteIOU, 0x1001, (int32_t)0xff001234u, 3, 2,
         {{0x1001, 0x34, WRITEB}, {0x1002, 0x0012, WRITE}}},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    run_write_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_writeio_records_legacy_sequences),
        cmocka_unit_test(test_writeiou_byte_records_legacy_sequences),
        cmocka_unit_test(test_writeiou_word_records_legacy_sequences),
        cmocka_unit_test(test_writeiou_tribyte_records_legacy_sequences),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
