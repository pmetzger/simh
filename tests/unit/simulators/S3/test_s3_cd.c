#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "s3_defs.h"
#include "sim_types.h"

extern UNIT cdr_unit;
extern UNIT stack_unit[];
extern int32_t DAR;
extern uint8_t rbuf[];

t_stat cdr_svc(UNIT *uptr);
int32_t GetMem(int32_t addr);
int32_t PutMem(int32_t addr, int32_t data);
t_stat read_card(int32_t ilnt, int32_t mod);

uchar_t ebcdic_to_ascii[256];
uchar_t ascii_to_ebcdic[256];

static int putmem_count;
static int32_t first_putmem_addr;
static int32_t first_putmem_data;

int32_t GetMem(int32_t addr)
{
    (void)addr;
    return 0;
}

int32_t PutMem(int32_t addr, int32_t data)
{
    if (putmem_count == 0) {
        first_putmem_addr = addr;
        first_putmem_data = data;
    }
    putmem_count++;
    return data;
}

static int setup_card_reader(void **state)
{
    static const uchar_t card[CDR_WIDTH] = {
        0xF1, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    };

    FILE *file = tmpfile();
    assert_non_null(file);
    assert_int_equal(fwrite(card, 1, sizeof(card), file), sizeof(card));
    assert_int_equal(fseek(file, 0, SEEK_SET), 0);

    *state = file;
    cdr_unit.fileref = file;
    cdr_unit.flags |= UNIT_ATT;
    DAR = 0x1234;
    putmem_count = 0;
    first_putmem_addr = -1;
    first_putmem_data = -1;
    sim_cancel(&cdr_unit);
    return 0;
}

static int setup_stacker(void **state)
{
    FILE *file = tmpfile();
    assert_non_null(file);

    *state = file;
    stack_unit[0].fileref = file;
    stack_unit[0].flags |= UNIT_ATT;

    memset(rbuf, 0x40, CDR_WIDTH);
    rbuf[0] = 0xF1;
    ebcdic_to_ascii[0x40] = ' ';
    ebcdic_to_ascii[0xF1] = '1';
    return 0;
}

static int teardown_card_reader(void **state)
{
    sim_cancel(&cdr_unit);
    cdr_unit.flags &= ~UNIT_ATT;
    cdr_unit.fileref = NULL;

    if (*state != NULL)
        fclose((FILE *)*state);

    return 0;
}

static int teardown_stacker(void **state)
{
    stack_unit[0].flags &= ~UNIT_ATT;
    stack_unit[0].fileref = NULL;

    if (*state != NULL)
        fclose((FILE *)*state);

    return 0;
}

static void test_ebcdic_card_bytes_are_written_unsigned(void **state)
{
    (void)state;

    assert_int_equal(read_card(0, 1), SCPE_OK);

    assert_int_equal(putmem_count, CDR_WIDTH);
    assert_int_equal(first_putmem_addr, 0x1234);
    assert_int_equal(first_putmem_data, 0xF1);
}

static void test_ebcdic_stacker_bytes_index_translation_table(void **state)
{
    char output[4];
    FILE *file = (FILE *)*state;

    assert_int_equal(cdr_svc(&cdr_unit), SCPE_OK);

    assert_int_equal(fseek(file, 0, SEEK_SET), 0);
    assert_non_null(fgets(output, sizeof(output), file));
    assert_string_equal(output, "1\n");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_ebcdic_card_bytes_are_written_unsigned,
            setup_card_reader,
            teardown_card_reader),
        cmocka_unit_test_setup_teardown(
            test_ebcdic_stacker_bytes_index_translation_table,
            setup_stacker,
            teardown_stacker),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
