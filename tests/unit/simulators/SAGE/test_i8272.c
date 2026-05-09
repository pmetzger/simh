#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"
#include "sim_types.h"

#define sectWrite test_i8272_sectWrite
#include "i8272.c"
#undef sectWrite

uint8_t test_i8272_dma[256];
t_addr saved_PC;
static uint_t test_i8272_irq_calls;
static int test_i8272_irq_delay;
static uint_t test_i8272_sector_writes;
static uint32_t test_i8272_write_cyl;
static uint32_t test_i8272_write_head;
static uint32_t test_i8272_write_sector;
static uint32_t test_i8272_write_len;
static uint8_t test_i8272_write_data[I8272_MAX_SECTOR_SZ];

uint8_t GetByteDMA(uint32_t Addr)
{
    return test_i8272_dma[Addr % sizeof(test_i8272_dma)];
}

void PutByteDMA(uint32_t Addr, uint8_t Value)
{
    test_i8272_dma[Addr % sizeof(test_i8272_dma)] = Value;
}

t_stat test_i8272_sectWrite(DISK_INFO *myDisk, uint32_t Cyl, uint32_t Head,
                            uint32_t Sector, uint8_t *buf, uint32_t buflen,
                            uint32_t *flags, uint32_t *writelen)
{
    (void)myDisk;

    ++test_i8272_sector_writes;
    test_i8272_write_cyl = Cyl;
    test_i8272_write_head = Head;
    test_i8272_write_sector = Sector;
    test_i8272_write_len = buflen;
    memcpy(test_i8272_write_data, buf, buflen);

    if (flags)
        *flags = 0;
    if (writelen)
        *writelen = buflen;
    return SCPE_OK;
}

static void test_i8272_irq(I8272 *chip, int delay)
{
    (void)chip;

    ++test_i8272_irq_calls;
    test_i8272_irq_delay = delay;
}

static void init_write_chip(I8272 *chip)
{
    memset(chip, 0, sizeof(*chip));
    memset(test_i8272_dma, 0, sizeof(test_i8272_dma));
    memset(test_i8272_write_data, 0, sizeof(test_i8272_write_data));
    test_i8272_irq_calls = 0;
    test_i8272_irq_delay = 0;
    test_i8272_sector_writes = 0;
    test_i8272_write_cyl = 0;
    test_i8272_write_head = 0;
    test_i8272_write_sector = 0;
    test_i8272_write_len = 0;

    chip->irq = test_i8272_irq;
    chip->cmd[0] = I8272_WRITE_DATA;
    chip->fdc_state = S_DATAWRITE;
    chip->fdc_curdrv = 0;
    chip->fdc_head = 1;
    chip->fdc_secsz = 3;
    chip->drive[0].imd = (DISK_INFO *)chip;
    chip->drive[0].track = 4;
}

static void
test_datawrite_past_eot_enters_result_without_sector_write(void **state)
{
    (void)state;

    I8272 chip;
    init_write_chip(&chip);
    chip.fdc_sector = 2;
    chip.fdc_eot = 1;
    chip.fdc_msr = I8272_MSR_NON_DMA;

    assert_int_equal(i8272_write(&chip, I8272_FDC_DATA, 0x99), SCPE_OK);

    assert_int_equal(test_i8272_sector_writes, 0);
    assert_int_equal(chip.fdc_state, S_RESULT);
    assert_int_equal(chip.result_cnt, 0);
    assert_int_equal(chip.result_len, resultsizes[I8272_WRITE_DATA]);
    assert_int_equal(chip.fdc_msr & I8272_MSR_NON_DMA, 0);
    assert_int_equal(test_i8272_irq_calls, 1);
    assert_int_equal(test_i8272_irq_delay, 200);
}

static void
test_dma_datawrite_writes_final_sector_and_enters_result(void **state)
{
    (void)state;

    I8272 chip;
    init_write_chip(&chip);
    chip.fdc_sector = 1;
    chip.fdc_eot = 1;
    chip.fdc_dma_addr = 10;
    test_i8272_dma[10] = 0x11;
    test_i8272_dma[11] = 0x22;
    test_i8272_dma[12] = 0x33;

    assert_int_equal(i8272_write(&chip, I8272_FDC_DATA, 0), SCPE_OK);

    assert_int_equal(test_i8272_sector_writes, 1);
    assert_int_equal(test_i8272_write_cyl, 4);
    assert_int_equal(test_i8272_write_head, 1);
    assert_int_equal(test_i8272_write_sector, 1);
    assert_int_equal(test_i8272_write_len, 3);
    assert_memory_equal(test_i8272_write_data, &test_i8272_dma[10], 3);
    assert_int_equal(chip.fdc_state, S_RESULT);
    assert_int_equal(test_i8272_irq_calls, 1);
    assert_int_equal(test_i8272_irq_delay, 200);
}

static void test_secwrite_final_sector_returns_after_result_phase(void **state)
{
    (void)state;

    I8272 chip;
    init_write_chip(&chip);
    chip.fdc_state = S_SECWRITE;
    chip.fdc_sector = 1;
    chip.fdc_eot = 1;
    chip.fdc_sdata[0] = 0x55;
    chip.fdc_sdata[1] = 0x66;
    chip.fdc_sdata[2] = 0x77;

    assert_true(i8272_secwrite(&chip));

    assert_int_equal(test_i8272_sector_writes, 1);
    assert_int_equal(test_i8272_write_sector, 1);
    assert_memory_equal(test_i8272_write_data, chip.fdc_sdata, 3);
    assert_int_equal(chip.fdc_state, S_RESULT);
    assert_int_equal(chip.result_cnt, 0);
    assert_int_equal(chip.result_len, resultsizes[I8272_WRITE_DATA]);
    assert_int_equal(test_i8272_irq_calls, 1);
    assert_int_equal(test_i8272_irq_delay, 200);
}

static void test_non_dma_datawrite_pauses_until_buffer_is_complete(void **state)
{
    (void)state;

    I8272 chip;
    init_write_chip(&chip);
    chip.fdc_nd = 1;
    chip.fdc_sector = 1;
    chip.fdc_eot = 1;

    assert_int_equal(i8272_write(&chip, I8272_FDC_DATA, 0x44), SCPE_OK);

    assert_int_equal(test_i8272_sector_writes, 0);
    assert_int_equal(chip.fdc_state, S_DATAWRITE);
    assert_int_equal(chip.fdc_nd_cnt, 1);
    assert_int_equal(chip.fdc_sdata[0], 0x44);
    assert_int_equal(test_i8272_irq_calls, 1);
    assert_int_equal(test_i8272_irq_delay, 10);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_datawrite_past_eot_enters_result_without_sector_write),
        cmocka_unit_test(
            test_dma_datawrite_writes_final_sector_and_enters_result),
        cmocka_unit_test(test_secwrite_final_sector_returns_after_result_phase),
        cmocka_unit_test(
            test_non_dma_datawrite_pauses_until_buffer_is_complete),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
