#include <stdlib.h>

#include "test_cmocka.h"

#include "system_defs.h"

extern UNIT RAM_unit;

t_stat RAM_cfg(uint16 base, uint16 size, uint8 dummy);
t_stat RAM_clr(void);
uint8 RAM_get_mbyte(uint16 addr);
void RAM_put_mbyte(uint16 addr, uint8 val);

/*
 * Reset the shared RAM device state so each test starts without configured
 * storage and without a stale pointer from a previous test.
 */
static int setup_iram8(void **state)
{
    (void)state;

    free(RAM_unit.filebuf);
    RAM_unit.filebuf = NULL;
    RAM_unit.capac = 0;
    RAM_unit.u3 = 0;

    return 0;
}

/*
 * Release configured RAM storage after a test.
 */
static int teardown_iram8(void **state)
{
    (void)state;

    free(RAM_unit.filebuf);
    RAM_unit.filebuf = NULL;
    RAM_unit.capac = 0;
    RAM_unit.u3 = 0;

    return 0;
}

/*
 * Unconfigured RAM reads represent absent memory and should return 0xff
 * instead of dereferencing a NULL buffer.
 */
static void test_unconfigured_ram_read_returns_absent_memory(void **state)
{
    (void)state;

    assert_int_equal(RAM_get_mbyte(0x2120), BYTEMASK);
}

/*
 * Unconfigured RAM writes represent writes to absent memory and should be
 * ignored instead of dereferencing a NULL buffer.
 */
static void test_unconfigured_ram_write_is_ignored(void **state)
{
    (void)state;

    RAM_put_mbyte(0x2120, 0x5A);
    assert_null(RAM_unit.filebuf);
}

/*
 * Configured RAM writes should still store bytes that are inside the
 * configured range.
 */
static void test_configured_ram_stores_in_range_bytes(void **state)
{
    (void)state;

    assert_int_equal(RAM_cfg(0x2000, 0x0010, 0), SCPE_OK);

    RAM_put_mbyte(0x2003, 0xA5);

    assert_int_equal(RAM_get_mbyte(0x2003), 0xA5);
}

/*
 * Reconfiguring RAM should replace the old storage.  The current allocation
 * is released by teardown; LeakSanitizer verifies that the prior allocation
 * was not orphaned.
 */
static void test_reconfiguring_ram_releases_old_storage(void **state)
{
    (void)state;

    assert_int_equal(RAM_cfg(0x2000, 0x0010, 0), SCPE_OK);
    RAM_put_mbyte(0x2000, 0xA5);

    assert_int_equal(RAM_cfg(0x3000, 0x0020, 0), SCPE_OK);

    assert_int_equal(RAM_unit.u3, 0x3000);
    assert_int_equal(RAM_unit.capac, 0x0020);
    assert_int_equal(RAM_get_mbyte(0x2000), BYTEMASK);
    assert_int_equal(RAM_get_mbyte(0x3000), 0);
}

/*
 * Reads outside the configured RAM range should behave as absent memory.
 */
static void
test_configured_ram_read_outside_range_returns_absent_memory(void **state)
{
    (void)state;

    assert_int_equal(RAM_cfg(0x2000, 0x0010, 0), SCPE_OK);

    assert_int_equal(RAM_get_mbyte(0x1FFF), BYTEMASK);
    assert_int_equal(RAM_get_mbyte(0x2010), BYTEMASK);
}

/*
 * Writes outside the configured RAM range should not corrupt configured
 * storage.
 */
static void test_configured_ram_write_outside_range_is_ignored(void **state)
{
    (void)state;

    assert_int_equal(RAM_cfg(0x2000, 0x0010, 0), SCPE_OK);
    RAM_put_mbyte(0x2000, 0x11);

    RAM_put_mbyte(0x1FFF, 0x22);
    RAM_put_mbyte(0x2010, 0x33);

    assert_int_equal(RAM_get_mbyte(0x2000), 0x11);
}

/*
 * Clearing RAM should leave later absent-memory reads and writes safe.
 */
static void test_cleared_ram_access_is_safe(void **state)
{
    (void)state;

    assert_int_equal(RAM_cfg(0x2000, 0x0010, 0), SCPE_OK);
    RAM_put_mbyte(0x2000, 0x77);
    assert_int_equal(RAM_clr(), SCPE_OK);

    assert_null(RAM_unit.filebuf);
    assert_int_equal(RAM_get_mbyte(0x2000), BYTEMASK);
    RAM_put_mbyte(0x2000, 0x88);
    assert_null(RAM_unit.filebuf);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_unconfigured_ram_read_returns_absent_memory, setup_iram8,
            teardown_iram8),
        cmocka_unit_test_setup_teardown(test_unconfigured_ram_write_is_ignored,
                                        setup_iram8, teardown_iram8),
        cmocka_unit_test_setup_teardown(
            test_configured_ram_stores_in_range_bytes, setup_iram8,
            teardown_iram8),
        cmocka_unit_test_setup_teardown(
            test_reconfiguring_ram_releases_old_storage, setup_iram8,
            teardown_iram8),
        cmocka_unit_test_setup_teardown(
            test_configured_ram_read_outside_range_returns_absent_memory,
            setup_iram8, teardown_iram8),
        cmocka_unit_test_setup_teardown(
            test_configured_ram_write_outside_range_is_ignored, setup_iram8,
            teardown_iram8),
        cmocka_unit_test_setup_teardown(test_cleared_ram_access_is_safe,
                                        setup_iram8, teardown_iram8),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
