#include <stddef.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax4xx_stddev.h"
#include "vax4xx_stddev_internal.h"

extern uint32 *rom;
extern uint32 *nvr;
extern UNIT or_unit[];

static uint32 test_rom[ROMSIZE >> 2];
static uint32 test_nvr[NVRSIZE >> 2];

int32 wtc_rd(int32 rg)
{
    (void)rg;

    return 0;
}

void wtc_wr(int32 rg, int32 val)
{
    (void)rg;
    (void)val;
}

void wtc_set_valid(void) {}

void wtc_set_invalid(void) {}

static void reset_stddev_state(void)
{
    memset(test_rom, 0, sizeof(test_rom));
    memset(test_nvr, 0, sizeof(test_nvr));
    rom = test_rom;
    nvr = test_nvr;

    for (size_t i = 0; i < OR_COUNT; i++) {
        or_unit[i].filebuf = NULL;
        or_unit[i].capac = 0;
        or_unit[i].flags &= ~UNIT_ATT;
    }
}

/* Verify the KA4xx ROM byte writer preserves non-selected byte lanes. */
static void test_rom_replace_byte_covers_every_byte_lane(void **state)
{
    static const struct {
        uint32 pa;
        uint32 val;
        uint32 expected;
    } cases[] = {
        {0, 0x55, 0x12345655},
        {1, 0xaa, 0x1234aa78},
        {2, 0x5a, 0x125a5678},
        {3, 0xff, 0xff345678},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(
            vax4xx_replace_byte_lane(0x12345678u, cases[i].pa, cases[i].val),
            cases[i].expected);
}

/* Verify only the low eight bits of the replacement value are stored. */
static void test_rom_replace_byte_masks_replacement_value(void **state)
{
    (void)state;

    assert_int_equal(vax4xx_replace_byte_lane(0x12345678u, 3, 0x1a5),
                     0xa5345678u);
}

/* Verify the exported ROM byte writer handles high-bit machine words. */
static void test_rom_write_byte_updates_exported_rom_storage(void **state)
{
    (void)state;

    reset_stddev_state();
    test_rom[0] = 0x12345678u;

    rom_wr_B((int32)(ROMBASE + 3), 0xff);

    assert_int_equal(test_rom[0], 0xff345678u);
    assert_int_equal((uint32)rom_rd(ROMBASE), 0xff345678u);
}

/* Verify NVR byte writes preserve adjacent byte lanes after its value shift. */
static void test_nvr_write_byte_preserves_other_lanes(void **state)
{
    uint32 nvr_index = 14;

    (void)state;

    reset_stddev_state();
    test_nvr[nvr_index] = 0x12345678u;

    nvr_wr((int32)(NVRBASE + (nvr_index << 2) + 3), 0x3fc, L_BYTE);

    assert_int_equal(test_nvr[nvr_index], 0xff345678u);
}

/* Verify NVR reads return the stored register value in the encoded position. */
static void
test_nvr_read_shifts_machine_word_without_signed_overflow(void **state)
{
    uint32 nvr_index = 14;

    (void)state;

    reset_stddev_state();
    test_nvr[nvr_index] = 0x40000000u;

    assert_int_equal((uint32)nvr_rd((int32)(NVRBASE + (nvr_index << 2))),
                     0x00000000u);
}

/* Verify byte-lane packing can set the sign bit without signed shifting. */
static void test_pack_byte_lane_covers_all_shift_positions(void **state)
{
    static const struct {
        uint32 shift;
        uint32 expected;
    } cases[] = {
        {0, 0x000000ff},
        {8, 0x0000ff00},
        {16, 0x00ff0000},
        {24, 0xff000000},
    };

    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert_int_equal(vax4xx_pack_byte_lane(0xff, cases[i].shift),
                         cases[i].expected);
}

/* Verify option ROM packing preserves all byte lanes of high-bit words. */
static void test_option_rom_read_packs_all_chip_widths(void **state)
{
    static uint8 one_chip_rom[8];
    static uint8 two_chip_rom[8];
    static uint8 four_chip_rom[8];

    (void)state;

    reset_stddev_state();

    one_chip_rom[0] = 1;
    one_chip_rom[1] = 0x80;
    or_unit[0].filebuf = one_chip_rom;
    or_unit[0].capac = sizeof(one_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32)or_rd((int32)(ORBASE + 4)), 0xffffff80u);

    reset_stddev_state();
    two_chip_rom[0] = 2;
    two_chip_rom[1] = 0x34;
    two_chip_rom[2] = 0x80;
    or_unit[0].filebuf = two_chip_rom;
    or_unit[0].capac = sizeof(two_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32)or_rd((int32)(ORBASE + 2)), 0xffff8034u);

    reset_stddev_state();
    four_chip_rom[0] = 4;
    four_chip_rom[4] = 0x78;
    four_chip_rom[5] = 0x56;
    four_chip_rom[6] = 0x34;
    four_chip_rom[7] = 0x80;
    or_unit[0].filebuf = four_chip_rom;
    or_unit[0].capac = sizeof(four_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32)or_rd((int32)(ORBASE + 4)), 0x80345678u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rom_replace_byte_covers_every_byte_lane),
        cmocka_unit_test(test_rom_replace_byte_masks_replacement_value),
        cmocka_unit_test(test_rom_write_byte_updates_exported_rom_storage),
        cmocka_unit_test(test_nvr_write_byte_preserves_other_lanes),
        cmocka_unit_test(
            test_nvr_read_shifts_machine_word_without_signed_overflow),
        cmocka_unit_test(test_pack_byte_lane_covers_all_shift_positions),
        cmocka_unit_test(test_option_rom_read_packs_all_chip_widths),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
