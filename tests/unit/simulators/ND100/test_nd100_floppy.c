#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "sim_defs.h"
#include "sim_types.h"
#include "nd100_defs.h"

#define TEST_CB 0100
#define TEST_MEM 0200

#define TEST_FL_CW_FCE 0000400
#define TEST_CW_FL_RD 0000000
#define TEST_CB_CW 000
#define TEST_CB_DAL 001
#define TEST_CB_DAHMAH 002
#define TEST_CB_MAL 003
#define TEST_CB_OPTWCH 004
#define TEST_CB_WCL 005
#define TEST_CB_OPT_WC 0100000

extern UNIT floppy_unit[];
t_stat floppy_reset(DEVICE *dptr);
t_stat floppy_svc(UNIT *uptr);

uint16_t PM[65536];
uint16_t R[8];
uint16_t regSTH;
int ald;
int curlvl;
int userring;

static uint16_t test_memory[65536];
static int interrupt_calls;

static FILE *create_disk_file(const uchar_t *data, size_t size)
{
    FILE *file;

    file = tmpfile();
    assert_non_null(file);
    if (size != 0)
        assert_int_equal(fwrite(data, 1, size, file), size);
    assert_int_equal(fflush(file), 0);
    rewind(file);
    return file;
}

static void prepare_floppy_read(FILE *file, int words)
{
    memset(test_memory, 0, sizeof(test_memory));
    memset(R, 0, sizeof(R));
    interrupt_calls = 0;

    assert_int_equal(floppy_reset(&floppy_dev), SCPE_OK);

    floppy_unit[0].fileref = file;
    floppy_unit[0].flags |= UNIT_ATT;

    regA = 0;
    assert_int_equal(iox_floppy(5), SCPE_OK);
    regA = TEST_CB;
    assert_int_equal(iox_floppy(7), SCPE_OK);

    test_memory[TEST_CB + TEST_CB_CW] = TEST_CW_FL_RD;
    test_memory[TEST_CB + TEST_CB_DAL] = 0;
    test_memory[TEST_CB + TEST_CB_DAHMAH] = 0;
    test_memory[TEST_CB + TEST_CB_MAL] = TEST_MEM;
    test_memory[TEST_CB + TEST_CB_OPTWCH] = TEST_CB_OPT_WC;
    test_memory[TEST_CB + TEST_CB_WCL] = (uint16_t)words;

    regA = TEST_FL_CW_FCE;
    assert_int_equal(iox_floppy(3), SCPE_OK);
}

static void finish_floppy(FILE *file)
{
    sim_cancel(&floppy_unit[0]);
    floppy_unit[0].flags &= ~UNIT_ATT;
    floppy_unit[0].fileref = NULL;
    fclose(file);
}

static void test_floppy_read_copies_complete_word(void **state)
{
    const uchar_t data[] = {0x12, 0x34};
    FILE *file;

    (void)state;

    file = create_disk_file(data, sizeof(data));
    prepare_floppy_read(file, 1);

    assert_int_equal(floppy_svc(&floppy_unit[0]), SCPE_OK);
    assert_int_equal(test_memory[TEST_MEM], 0x1234);

    finish_floppy(file);
}

static void test_floppy_read_rejects_short_word(void **state)
{
    const uchar_t data[] = {0x12};
    FILE *file;

    (void)state;

    file = create_disk_file(data, sizeof(data));
    prepare_floppy_read(file, 1);

    assert_int_equal(floppy_svc(&floppy_unit[0]), STOP_UNHIOX);
    assert_int_equal(test_memory[TEST_MEM], 0);

    finish_floppy(file);
}

uint16_t prdmem(int addr, int how)
{
    (void)how;

    return test_memory[addr & 0177777];
}

void pwrmem(int addr, int val, int how)
{
    (void)how;

    test_memory[addr & 0177777] = (uint16_t)val;
}

void wrmem(int addr, int val, int how)
{
    (void)how;

    test_memory[addr & 0177777] = (uint16_t)val;
}

void extint(int lvl, struct intr *intr)
{
    (void)lvl;
    (void)intr;

    interrupt_calls++;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_floppy_read_copies_complete_word),
        cmocka_unit_test(test_floppy_read_rejects_short_word),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
