#include <stddef.h>

#include "test_cmocka.h"

#include "vax_qbus_internal.h"

/*
 * These tests preserve the legacy ReadIO/ReadIOU contract by supplying
 * controlled ReadQb results and recording the exact Qbus read sequence.
 */

int32 ReadIO(uint32 pa, int32 lnt);
int32 ReadIOU(uint32 pa, int32 lnt);
int32 va_mem_rd(int32 pa);
void va_mem_wr(int32 pa, int32 val, int32 lnt);

typedef int32 (*qbus_io_read_fn)(uint32 pa, int32 lnt);

typedef struct {
    uint32 pa;
    int32 val;
} supplied_qbus_read;

typedef struct {
    qbus_io_read_fn read;
    uint32 pa;
    int32 lnt;
    size_t supplied_count;
    supplied_qbus_read supplied[2];
    int32 expected_result;
} qbus_io_read_case;

static supplied_qbus_read supplied_reads[2];
static uint32 recorded_read_addresses[2];
static size_t supplied_read_count;
static size_t recorded_read_count;

uint32 trpirq;
uint32 *M;
uint32 R[16];
uint32 PSL;
uint32 SISR;
uint32 fault_PC;
uint32 p1;
uint32 p2;
uint32 mchk_ref;
uint32 *vc_buf;
uint32 *va_buf;
uint32 va_addr;
jmp_buf save_env;
int32 hlt_pin;
int32 mem_err;
int32 crd_err;
int32 sys_model;
int32 ka_mser;
DEVICE cpu_dev;
UNIT cpu_unit;

int32 ReadReg(uint32 pa, int32 lnt)
{
    /* Stubbed register-space read for uncalled legacy helpers. */
    (void)pa;
    (void)lnt;

    return 0;
}

void WriteReg(uint32 pa, int32 val, int32 lnt)
{
    /* Stubbed register-space write for uncalled legacy helpers. */
    (void)pa;
    (void)val;
    (void)lnt;
}

int32 vc_mem_rd(int32 pa)
{
    /* Stubbed video memory read for uncalled legacy helpers. */
    (void)pa;

    return 0;
}

void vc_mem_wr(int32 pa, int32 val, int32 lnt)
{
    /* Stubbed video memory write for uncalled legacy helpers. */
    (void)pa;
    (void)val;
    (void)lnt;
}

int32 va_mem_rd(int32 pa)
{
    /* Stubbed video memory read for uncalled legacy helpers. */
    (void)pa;

    return 0;
}

void va_mem_wr(int32 pa, int32 val, int32 lnt)
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

t_stat set_autocon(UNIT *uptr, int32 val, const char *cptr, void *desc)
{
    /* Stubbed modifier callback for static modifier tables. */
    (void)uptr;
    (void)val;
    (void)cptr;
    (void)desc;

    return SCPE_OK;
}

t_stat show_autocon(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    /* Stubbed modifier callback for static modifier tables. */
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

t_stat show_iospace(FILE *st, UNIT *uptr, int32 val, const void *desc)
{
    /* Stubbed modifier callback for static modifier tables. */
    (void)st;
    (void)uptr;
    (void)val;
    (void)desc;

    return SCPE_OK;
}

t_stat show_bus_map(FILE *st, const char *cptr, uint32 *busmap,
                    uint32 nmapregs, const char *busname, uint32 mapvalid)
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

int32 vax_qbus_test_record_read(uint32 pa)
{
    assert_true(recorded_read_count < supplied_read_count);
    assert_true(recorded_read_count < sizeof(recorded_read_addresses) /
                                          sizeof(recorded_read_addresses[0]));
    assert_int_equal(pa, supplied_reads[recorded_read_count].pa);
    recorded_read_addresses[recorded_read_count] = pa;
    return supplied_reads[recorded_read_count++].val;
}

static void reset_recorded_reads(const supplied_qbus_read *supplied,
                                 size_t supplied_count)
{
    assert_true(supplied_count <= sizeof(supplied_reads) /
                                      sizeof(supplied_reads[0]));
    supplied_read_count = supplied_count;
    recorded_read_count = 0;
    for (size_t i = 0; i < supplied_count; i++)
        supplied_reads[i] = supplied[i];
}

static void assert_recorded_reads(const supplied_qbus_read *expected,
                                  size_t expected_count)
{
    assert_int_equal(recorded_read_count, expected_count);
    for (size_t i = 0; i < expected_count; i++)
        assert_int_equal(recorded_read_addresses[i], expected[i].pa);
}

static void run_read_cases(const qbus_io_read_case *cases, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        reset_recorded_reads(cases[i].supplied, cases[i].supplied_count);
        assert_int_equal(cases[i].read(cases[i].pa, cases[i].lnt),
                         cases[i].expected_result);
        assert_recorded_reads(cases[i].supplied, cases[i].supplied_count);
        }
}

/* Verify aligned ReadIO cases preserve legacy ReadQb sequencing. */
static void test_readio_records_legacy_sequences(void **state)
{
    static const qbus_io_read_case cases[] = {
        {ReadIO, 0x1000, L_BYTE, 1, {{0x1000, 0xff00}},
         (int32)0x0000ff00u},
        {ReadIO, 0x1002, L_WORD, 1, {{0x1002, 0xff00}},
         (int32)0xff000000u},
        {ReadIO, 0x1000, L_LONG, 2, {{0x1000, 0x1234}, {0x1002, 0xff00}},
         (int32)0xff001234u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    run_read_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

/* Verify ReadIOU byte reads preserve legacy ReadQb sequencing. */
static void test_readiou_byte_records_legacy_sequences(void **state)
{
    static const qbus_io_read_case cases[] = {
        {ReadIOU, 0x1000, L_BYTE, 1, {{0x1000, 0x12a5}},
         (int32)0x000012a5u},
        {ReadIOU, 0x1001, L_BYTE, 1, {{0x1001, 0x12a5}},
         (int32)0x000012a5u},
        {ReadIOU, 0x1002, L_BYTE, 1, {{0x1002, 0x80a5}},
         (int32)0x80a50000u},
        {ReadIOU, 0x1003, L_BYTE, 1, {{0x1003, 0x80a5}},
         (int32)0x80a50000u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    run_read_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

/* Verify ReadIOU word reads preserve legacy ReadQb sequencing. */
static void test_readiou_word_records_legacy_sequences(void **state)
{
    static const qbus_io_read_case cases[] = {
        {ReadIOU, 0x1000, L_WORD, 1, {{0x1000, 0xff00}},
         (int32)0x0000ff00u},
        {ReadIOU, 0x1001, L_WORD, 2, {{0x1001, 0x1234}, {0x1003, 0xff00}},
         (int32)0xff001234u},
        {ReadIOU, 0x1002, L_WORD, 1, {{0x1002, 0xff00}},
         (int32)0xff000000u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    run_read_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

/* Verify ReadIOU tribyte reads preserve legacy ReadQb sequencing. */
static void test_readiou_tribyte_records_legacy_sequences(void **state)
{
    static const qbus_io_read_case cases[] = {
        {ReadIOU, 0x1000, 3, 2, {{0x1000, 0x1234}, {0x1002, 0xff00}},
         (int32)0xff001234u},
        {ReadIOU, 0x1001, 3, 2, {{0x1001, 0x1234}, {0x1003, 0xff00}},
         (int32)0xff001234u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    run_read_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_readio_records_legacy_sequences),
        cmocka_unit_test(test_readiou_byte_records_legacy_sequences),
        cmocka_unit_test(test_readiou_word_records_legacy_sequences),
        cmocka_unit_test(test_readiou_tribyte_records_legacy_sequences),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
