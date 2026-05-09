#include <stdint.h>

#include "test_cmocka.h"

#include "3b2_scsi_internal.h"

/*
 * A table entry of -1 means that the 3B2 SCSI host adapter has no device for
 * that subdevice number. The resolver must reject it instead of converting it
 * to target 255.
 */
static void test_subdev_target_rejects_unmapped_subdevice(void **state)
{
    int8_t subdev_tab[8] = {-1, 1, -1, -1, -1, -1, -1, -1};
    uint8_t target = 42;

    (void)state;

    assert_false(ha_subdev_target(subdev_tab, 0, &target));
    assert_int_equal(target, 42);
}

/*
 * A non-negative table entry is the SCSI target number backing that host
 * adapter subdevice.
 */
static void test_subdev_target_accepts_attached_subdevice(void **state)
{
    int8_t subdev_tab[8] = {-1, 1, -1, -1, -1, -1, -1, -1};
    uint8_t target = 0xff;

    (void)state;

    assert_true(ha_subdev_target(subdev_tab, 1, &target));
    assert_int_equal(target, 1);
}

/*
 * The command byte encodes the host-adapter subdevice in its low three bits,
 * so values larger than 7 wrap through the same table index.
 */
static void test_subdev_target_uses_three_bit_subdevice_index(void **state)
{
    int8_t subdev_tab[8] = {-1, 1, -1, -1, -1, -1, -1, 7};
    uint8_t target = 0xff;

    (void)state;

    assert_true(ha_subdev_target(subdev_tab, 0x0f, &target));
    assert_int_equal(target, 7);
}

/*
 * Boot, read-block, and write-block commands use the host-adapter subdevice
 * table. Each must reject an unmapped subdevice before the command dispatcher
 * uses the result as an index into the per-target state table.
 */
static void test_subdev_commands_reject_unmapped_subdevice(void **state)
{
    int8_t subdev_tab[8] = {-1, 1, -1, -1, -1, -1, -1, -1};
    uint8_t target = 42;

    (void)state;

    assert_false(ha_subdev_command_target(subdev_tab, HA_BOOT, 0,
                                          &target));
    assert_int_equal(target, 42);

    assert_false(ha_subdev_command_target(subdev_tab, HA_READ_BLK, 0,
                                          &target));
    assert_int_equal(target, 42);

    assert_false(ha_subdev_command_target(subdev_tab, HA_WRITE_BLK, 0,
                                          &target));
    assert_int_equal(target, 42);
}

/*
 * Valid boot, read-block, and write-block commands must still resolve to the
 * target mapped by the host-adapter subdevice table.
 */
static void test_subdev_commands_accept_mapped_subdevice(void **state)
{
    int8_t subdev_tab[8] = {-1, 3, -1, -1, -1, -1, -1, -1};
    uint8_t target = 0xff;

    (void)state;

    assert_true(ha_subdev_command_target(subdev_tab, HA_BOOT, 1, &target));
    assert_int_equal(target, 3);

    target = 0xff;
    assert_true(ha_subdev_command_target(subdev_tab, HA_READ_BLK, 1,
                                         &target));
    assert_int_equal(target, 3);

    target = 0xff;
    assert_true(ha_subdev_command_target(subdev_tab, HA_WRITE_BLK, 1,
                                         &target));
    assert_int_equal(target, 3);
}

/*
 * Commands that do not use host-adapter subdevice mapping are intentionally
 * outside this helper, so adding a new caller requires an explicit test.
 */
static void test_subdev_command_target_rejects_unhandled_opcode(void **state)
{
    int8_t subdev_tab[8] = {-1, 3, -1, -1, -1, -1, -1, -1};
    uint8_t target = 42;

    (void)state;

    assert_false(ha_subdev_command_target(subdev_tab, HA_CNTRL, 1,
                                          &target));
    assert_int_equal(target, 42);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_subdev_target_rejects_unmapped_subdevice),
        cmocka_unit_test(test_subdev_target_accepts_attached_subdevice),
        cmocka_unit_test(test_subdev_target_uses_three_bit_subdevice_index),
        cmocka_unit_test(test_subdev_commands_reject_unmapped_subdevice),
        cmocka_unit_test(test_subdev_commands_accept_mapped_subdevice),
        cmocka_unit_test(test_subdev_command_target_rejects_unhandled_opcode),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
