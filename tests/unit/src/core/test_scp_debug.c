#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"

static DEBTAB test_debug_flags[] = {
    {"LOW", 0x00000001u, "low debug bit"},
    {"HIGH", 0x80000000u, "high debug bit"},
    {NULL, 0, NULL},
};

/*
 * Initialize a minimal debug-capable device and unit for set_dev_debug()
 * tests.  The command helper only needs the debug capability, debug table,
 * debug-control fields, and a unit pointer.
 */
static void init_debug_device(DEVICE *device, UNIT *unit, DEBTAB *debug_flags)
{
    memset(unit, 0, sizeof(*unit));
    memset(device, 0, sizeof(*device));

    device->name = "DBG";
    device->units = unit;
    device->numunits = 1;
    device->flags = DEV_DEBUG;
    device->debflags = debug_flags;
    unit->dptr = device;
}

/*
 * Devices without a named debug table enable every debug bit when SET DEBUG
 * is issued without an explicit option list.
 */
static void
test_set_dev_debug_enables_all_device_bits_without_table(void **state)
{
    DEVICE device;
    UNIT unit;

    (void)state;
    init_debug_device(&device, &unit, NULL);

    assert_int_equal(set_dev_debug(&device, &unit, 1, NULL), SCPE_OK);
    assert_int_equal(device.dctrl, UINT32_MAX);
    assert_int_equal(unit.dctrl, 0);
}

/*
 * Unit debug uses the same all-bits behavior when the device has no named
 * debug table, but it writes the unit debug-control field.
 */
static void test_set_dev_debug_enables_all_unit_bits_without_table(void **state)
{
    DEVICE device;
    UNIT unit;

    (void)state;
    init_debug_device(&device, &unit, NULL);

    assert_int_equal(set_dev_debug(&device, &unit, 3, NULL), SCPE_OK);
    assert_int_equal(device.dctrl, 0);
    assert_int_equal(unit.dctrl, UINT32_MAX);
}

/*
 * NODEBUG without an option list clears device debug bits without touching
 * unit-specific debug state.
 */
static void test_set_dev_debug_clears_device_bits_without_table(void **state)
{
    DEVICE device;
    UNIT unit;

    (void)state;
    init_debug_device(&device, &unit, NULL);
    device.dctrl = UINT32_MAX;
    unit.dctrl = UINT32_MAX;

    assert_int_equal(set_dev_debug(&device, &unit, 0, NULL), SCPE_OK);
    assert_int_equal(device.dctrl, 0);
    assert_int_equal(unit.dctrl, UINT32_MAX);
}

/*
 * Devices with a named debug table enable exactly the table masks, including
 * high-bit masks that must be treated as unsigned debug-control bits.
 */
static void test_set_dev_debug_enables_table_device_bits(void **state)
{
    DEVICE device;
    UNIT unit;

    (void)state;
    init_debug_device(&device, &unit, test_debug_flags);

    assert_int_equal(set_dev_debug(&device, &unit, 1, NULL), SCPE_OK);
    assert_int_equal(device.dctrl, 0x80000001u);
    assert_int_equal(unit.dctrl, 0);
}

/*
 * Unit table-driven DEBUG also enables exactly the table masks on the unit.
 */
static void test_set_dev_debug_enables_table_unit_bits(void **state)
{
    DEVICE device;
    UNIT unit;

    (void)state;
    init_debug_device(&device, &unit, test_debug_flags);

    assert_int_equal(set_dev_debug(&device, &unit, 3, NULL), SCPE_OK);
    assert_int_equal(device.dctrl, 0);
    assert_int_equal(unit.dctrl, 0x80000001u);
}

/*
 * Named DEBUG options set exactly the requested table mask, including masks
 * whose high bit would be negative if treated as a signed integer.
 */
static void test_set_dev_debug_enables_named_high_device_bit(void **state)
{
    DEVICE device;
    UNIT unit;

    (void)state;
    init_debug_device(&device, &unit, test_debug_flags);

    assert_int_equal(set_dev_debug(&device, &unit, 1, "HIGH"), SCPE_OK);
    assert_int_equal(device.dctrl, 0x80000000u);
    assert_int_equal(unit.dctrl, 0);
}

/*
 * Named NODEBUG options clear only the requested table mask.
 */
static void test_set_dev_debug_clears_named_unit_bit(void **state)
{
    DEVICE device;
    UNIT unit;

    (void)state;
    init_debug_device(&device, &unit, test_debug_flags);
    unit.dctrl = 0x80000001u;

    assert_int_equal(set_dev_debug(&device, &unit, 2, "HIGH"), SCPE_OK);
    assert_int_equal(device.dctrl, 0);
    assert_int_equal(unit.dctrl, 0x00000001u);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_set_dev_debug_enables_all_device_bits_without_table),
        cmocka_unit_test(
            test_set_dev_debug_enables_all_unit_bits_without_table),
        cmocka_unit_test(test_set_dev_debug_clears_device_bits_without_table),
        cmocka_unit_test(test_set_dev_debug_enables_table_device_bits),
        cmocka_unit_test(test_set_dev_debug_enables_table_unit_bits),
        cmocka_unit_test(test_set_dev_debug_enables_named_high_device_bit),
        cmocka_unit_test(test_set_dev_debug_clears_named_unit_bit),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
