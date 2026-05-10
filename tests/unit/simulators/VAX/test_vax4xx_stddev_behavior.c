#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "vax_defs.h"
#include "vax4xx_rom_patch.h"

/*
 * These tests preserve the legacy KA4xx standard-device ROM, NVR, and option
 * ROM bit-packing behavior at the real entry points.
 */

int32_t rom_rd(int32_t pa);
void rom_wr_B(int32_t pa, int32_t val);
int32_t nvr_rd(int32_t pa);
void nvr_wr(int32_t pa, int32_t val, int32_t lnt);
int32_t or_rd(int32_t pa);

extern uint32_t *rom;
extern uint32_t *nvr;
extern UNIT or_unit[];

static uint32_t test_rom[ROMSIZE >> 2];
static uint32_t test_nvr[NVRSIZE >> 2];

int32_t wtc_rd(int32_t rg)
{
    /* Stubbed watch-chip read for tests that target non-watch-chip NVR. */
    (void)rg;

    return 0;
}

void wtc_wr(int32_t rg, int32_t val)
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
    rom_set_patch(NULL, 0, "NONE", NULL);

    for (size_t i = 0; i < OR_COUNT; i++) {
        or_unit[i].filebuf = NULL;
        or_unit[i].capac = 0;
        or_unit[i].flags &= ~UNIT_ATT;
    }
}

static void set_rom_bytes(uint32_t address, const uint8_t *bytes, size_t len)
{
    for (size_t i = 0; i < len; i++)
        rom_wr_B((int32_t)(address + i), bytes[i]);
}

/* Verify the KA4xx ROM byte writer preserves legacy byte-lane behavior. */
static void test_rom_write_byte_preserves_legacy_behavior(void **state)
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
    };

    /* Cmocka test callback signature.
       This implementation does not use every parameter. */
    (void)state;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset_stddev_behavior_state();
        test_rom[0] = 0x12345678u;
        rom_wr_B((int32_t)cases[i].pa, cases[i].val);
        assert_int_equal(test_rom[0], cases[i].expected);
        assert_int_equal((uint32_t)rom_rd(ROMBASE), cases[i].expected);
    }
}

static void test_rom_patch_setting_applies_loaded_ka48_patch(void **state)
{
    static const uint8_t original[] = {0xc2, 0x2c, 0x5e, 0xd0};
    static const uint8_t patched[] = {0xd0, 0x01, 0x50, 0x04};
    (void)state;

    reset_stddev_behavior_state();
    test_rom[0] = 0x12345678u;
    set_rom_bytes(0x2004de0a, original, sizeof(original));

    assert_int_equal(rom_set_patch(NULL, 0, "SKIP-RAM-SELFTEST", NULL),
                     SCPE_OK);

    assert_memory_equal((uint8_t *)&test_rom[0x0de0a >> 2] + 2, patched,
                        sizeof(patched));
}

static void test_rom_patch_setting_can_be_pending_until_rom_load(void **state)
{
    static const uint8_t original[] = {0xc2, 0x2c, 0x5e, 0xd0};
    static const uint8_t patched[] = {0xd0, 0x01, 0x50, 0x04};
    (void)state;

    reset_stddev_behavior_state();
    rom = NULL;

    assert_int_equal(rom_set_patch(NULL, 0, "SKIP-RAM-SELFTEST", NULL),
                     SCPE_OK);

    rom = test_rom;
    test_rom[0] = 0x12345678u;
    set_rom_bytes(0x2004de0a, original, sizeof(original));
    assert_int_equal(rom_apply_patches(), SCPE_OK);

    assert_memory_equal((uint8_t *)&test_rom[0x0de0a >> 2] + 2, patched,
                        sizeof(patched));
}

static void test_rom_patch_setting_none_reverts_loaded_patch(void **state)
{
    static const uint8_t original[] = {0xc2, 0x2c, 0x5e, 0xd0};
    static const uint8_t patched[] = {0xd0, 0x01, 0x50, 0x04};
    (void)state;

    reset_stddev_behavior_state();
    test_rom[0] = 0x12345678u;
    set_rom_bytes(0x2004de0a, patched, sizeof(patched));
    assert_int_equal(rom_set_patch(NULL, 0, "SKIP-RAM-SELFTEST", NULL),
                     SCPE_OK);

    assert_int_equal(rom_set_patch(NULL, 0, "NONE", NULL), SCPE_OK);

    assert_memory_equal((uint8_t *)&test_rom[0x0de0a >> 2] + 2, original,
                        sizeof(original));
}

static void test_rom_patch_setting_rejects_unknown_patch(void **state)
{
    (void)state;

    reset_stddev_behavior_state();

    assert_int_equal(SCPE_BARE_STATUS(rom_set_patch(NULL, 0, "BOGUS", NULL)),
                     SCPE_ARG);
}

/* Verify KA4xx NVR byte writes preserve legacy byte-lane behavior. */
static void test_nvr_write_byte_preserves_legacy_behavior(void **state)
{
    static const struct {
        uint32_t pa;
        int32_t val;
        uint32_t expected;
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
        nvr_wr((int32_t)cases[i].pa, cases[i].val, L_BYTE);
        assert_int_equal(test_nvr[14], cases[i].expected);
    }
}

/* Verify KA4xx NVR reads preserve legacy encoded-position behavior. */
static void test_nvr_read_preserves_legacy_shift_behavior(void **state)
{
    static const struct {
        uint32_t stored;
        uint32_t expected;
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
        assert_int_equal((uint32_t)nvr_rd((int32_t)(NVRBASE + (14u << 2))),
                         cases[i].expected);
    }
}

/* Verify KA4xx option ROM reads preserve legacy chip-width packing. */
static void test_option_rom_read_preserves_legacy_packing(void **state)
{
    static uint8_t one_chip_rom[8];
    static uint8_t two_chip_rom[8];
    static uint8_t four_chip_rom[8];

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
    assert_int_equal((uint32_t)or_rd((int32_t)(ORBASE + 4)), 0xffffff80u);

    reset_stddev_behavior_state();
    memset(two_chip_rom, 0, sizeof(two_chip_rom));
    two_chip_rom[0] = 2;
    two_chip_rom[1] = 0x34;
    two_chip_rom[2] = 0x80;
    or_unit[0].filebuf = two_chip_rom;
    or_unit[0].capac = sizeof(two_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32_t)or_rd((int32_t)(ORBASE + 2)), 0xffff8034u);

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
    assert_int_equal((uint32_t)or_rd((int32_t)(ORBASE + 4)), 0x80345678u);
}

/* Verify multi-chip option ROM reads wrap every byte inside the ROM image. */
static void test_option_rom_read_wraps_each_chip_byte(void **state)
{
    static uint8_t one_chip_rom[8];
    static uint8_t two_chip_rom[8];
    static uint8_t four_chip_rom[8];

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
    assert_int_equal((uint32_t)or_rd((int32_t)(ORBASE + 28)), 0xffffff80u);

    reset_stddev_behavior_state();
    memset(two_chip_rom, 0, sizeof(two_chip_rom));
    two_chip_rom[0] = 2;
    two_chip_rom[7] = 0x80;
    or_unit[0].filebuf = two_chip_rom;
    or_unit[0].capac = sizeof(two_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32_t)or_rd((int32_t)(ORBASE + 14)), 0xffff0280u);

    reset_stddev_behavior_state();
    memset(four_chip_rom, 0, sizeof(four_chip_rom));
    four_chip_rom[0] = 4;
    four_chip_rom[1] = 0x56;
    four_chip_rom[2] = 0x34;
    four_chip_rom[7] = 0x80;
    or_unit[0].filebuf = four_chip_rom;
    or_unit[0].capac = sizeof(four_chip_rom);
    or_unit[0].flags |= UNIT_ATT;
    assert_int_equal((uint32_t)or_rd((int32_t)(ORBASE + 7)), 0x34560480u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rom_write_byte_preserves_legacy_behavior),
        cmocka_unit_test(test_rom_patch_setting_applies_loaded_ka48_patch),
        cmocka_unit_test(test_rom_patch_setting_can_be_pending_until_rom_load),
        cmocka_unit_test(test_rom_patch_setting_none_reverts_loaded_patch),
        cmocka_unit_test(test_rom_patch_setting_rejects_unknown_patch),
        cmocka_unit_test(test_nvr_write_byte_preserves_legacy_behavior),
        cmocka_unit_test(test_nvr_read_preserves_legacy_shift_behavior),
        cmocka_unit_test(test_option_rom_read_preserves_legacy_packing),
        cmocka_unit_test(test_option_rom_read_wraps_each_chip_byte),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
