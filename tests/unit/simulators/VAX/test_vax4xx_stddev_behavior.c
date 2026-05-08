#include <stddef.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"

/*
 * These tests preserve the legacy KA4xx standard-device ROM, NVR, and option
 * ROM bit-packing behavior at the real entry points.
 */

int32 rom_rd(int32 pa);
int32 nvr_rd(int32 pa);
void nvr_wr(int32 pa, int32 val, int32 lnt);
int32 or_rd(int32 pa);

extern uint32 *rom;
extern uint32 *nvr;
extern UNIT or_unit[];

static uint32 test_rom[ROMSIZE >> 2];
static uint32 test_nvr[NVRSIZE >> 2];

int32 wtc_rd(int32 rg)
{
    /* Stubbed watch-chip read for tests that target non-watch-chip NVR. */
    (void)rg;

    return 0;
}

void wtc_wr(int32 rg, int32 val)
{
    /* Stubbed watch-chip write for tests that target non-watch-chip NVR. */
    (void)rg;
    (void)val;
}

void wtc_set_valid(void)
{
}

void wtc_set_invalid(void)
{
}

static void reset_stddev_behavior_state(void)
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

/* Verify the KA4xx ROM byte writer preserves legacy byte-lane behavior. */
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
        reset_stddev_behavior_state();
        test_rom[0] = 0x12345678u;
        rom_wr_B((int32)cases[i].pa, cases[i].val);
        assert_int_equal(test_rom[0], cases[i].expected);
        assert_int_equal((uint32)rom_rd(ROMBASE), cases[i].expected);
    }
}

/* Verify KA4xx NVR byte writes preserve legacy byte-lane behavior. */
static void test_nvr_write_byte_preserves_legacy_behavior(void **state)
{
    static const struct {
        uint32 pa;
        int32 val;
        uint32 expected;
    } cases[] = {
        {NVRBASE + (14u << 2), 0x294, 0x123456a5u},
        {NVRBASE + (14u << 2) + 1, 0x294, 0x1234a578u},
        {NVRBASE + (14u << 2) + 2, 0x294, 0x12a55678u},
        {NVRBASE + (14u << 2) + 3, 0x294, 0xa5345678u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_stddev_behavior_state();
        test_nvr[14] = 0x12345678u;
        nvr_wr((int32)cases[i].pa, cases[i].val, L_BYTE);
        assert_int_equal(test_nvr[14], cases[i].expected);
    }
}

/* Verify KA4xx NVR reads preserve legacy encoded-position behavior. */
static void test_nvr_read_preserves_legacy_shift_behavior(void **state)
{
    static const struct {
        uint32 stored;
        uint32 expected;
    } cases[] = {
        {0x00000001u, 0x00000004u},
        {0x3fffffffu, 0xfffffffcu},
        {0x40000000u, 0x00000000u},
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_stddev_behavior_state();
        test_nvr[14] = cases[i].stored;
        assert_int_equal((uint32)nvr_rd((int32)(NVRBASE + (14u << 2))),
                         cases[i].expected);
    }
}

/* Verify KA4xx option ROM reads preserve legacy chip-width packing. */
static void test_option_rom_read_preserves_legacy_packing(void **state)
{
    static uint8 one_chip_rom[8];
    static uint8 two_chip_rom[8];
    static uint8 four_chip_rom[8];

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_stddev_behavior_state();
    memset(one_chip_rom, 0, sizeof(one_chip_rom));
    one_chip_rom[0] = 1;
    one_chip_rom[1] = 0x80;
    or_unit[0].filebuf = one_chip_rom;
    or_unit[0].capac = sizeof(one_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32)or_rd((int32)(ORBASE + 4)), 0xffffff80u);

    reset_stddev_behavior_state();
    memset(two_chip_rom, 0, sizeof(two_chip_rom));
    two_chip_rom[0] = 2;
    two_chip_rom[1] = 0x34;
    two_chip_rom[2] = 0x80;
    or_unit[0].filebuf = two_chip_rom;
    or_unit[0].capac = sizeof(two_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32)or_rd((int32)(ORBASE + 2)), 0xffff8034u);

    reset_stddev_behavior_state();
    memset(four_chip_rom, 0, sizeof(four_chip_rom));
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

/* Verify multi-chip option ROM reads wrap every byte inside the ROM image. */
static void test_option_rom_read_wraps_each_chip_byte(void **state)
{
    static uint8 one_chip_rom[8];
    static uint8 two_chip_rom[8];
    static uint8 four_chip_rom[8];

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    reset_stddev_behavior_state();
    memset(one_chip_rom, 0, sizeof(one_chip_rom));
    one_chip_rom[0] = 1;
    one_chip_rom[7] = 0x80;
    or_unit[0].filebuf = one_chip_rom;
    or_unit[0].capac = sizeof(one_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32)or_rd((int32)(ORBASE + 28)), 0xffffff80u);

    reset_stddev_behavior_state();
    memset(two_chip_rom, 0, sizeof(two_chip_rom));
    two_chip_rom[0] = 2;
    two_chip_rom[7] = 0x80;
    or_unit[0].filebuf = two_chip_rom;
    or_unit[0].capac = sizeof(two_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32)or_rd((int32)(ORBASE + 14)), 0xffff0280u);

    reset_stddev_behavior_state();
    memset(four_chip_rom, 0, sizeof(four_chip_rom));
    four_chip_rom[0] = 4;
    four_chip_rom[1] = 0x56;
    four_chip_rom[2] = 0x34;
    four_chip_rom[7] = 0x80;
    or_unit[0].filebuf = four_chip_rom;
    or_unit[0].capac = sizeof(four_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32)or_rd((int32)(ORBASE + 7)), 0x34560480u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rom_write_byte_preserves_legacy_behavior),
        cmocka_unit_test(test_nvr_write_byte_preserves_legacy_behavior),
        cmocka_unit_test(test_nvr_read_preserves_legacy_shift_behavior),
        cmocka_unit_test(test_option_rom_read_preserves_legacy_packing),
        cmocka_unit_test(test_option_rom_read_wraps_each_chip_byte),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
