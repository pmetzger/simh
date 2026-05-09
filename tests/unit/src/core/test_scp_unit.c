#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"
#include "test_support.h"

t_stat set_dev_enbdis(DEVICE *dptr, UNIT *uptr, int32_t flag, const char *cptr);

struct scp_unit_fixture {
    char temp_dir[1024];
    char file_path[1024];
    UNIT unit;
    DEVICE device;
};

static int setup_scp_unit_fixture(void **state)
{
    struct scp_unit_fixture *fixture;
    DEVICE *devices[2];

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    assert_int_equal(simh_test_make_temp_dir(fixture->temp_dir,
                                             sizeof(fixture->temp_dir),
                                             "scp-unit"),
                     0);
    assert_int_equal(simh_test_join_path(fixture->file_path,
                                         sizeof(fixture->file_path),
                                         fixture->temp_dir, "unit.bin"),
                     0);

    simh_test_init_device_unit(&fixture->device, &fixture->unit, "DSK", "DSK0",
                               0, UNIT_ATTABLE, 8, 1);

    devices[0] = &fixture->device;
    devices[1] = NULL;
    assert_int_equal(simh_test_install_devices("zimh-unit-scp-unit", devices),
                     0);

    *state = fixture;
    return 0;
}

static int teardown_scp_unit_fixture(void **state)
{
    struct scp_unit_fixture *fixture = *state;

    sim_cancel(&fixture->unit);
    if ((fixture->unit.flags & UNIT_ATT) != 0)
        assert_int_equal(detach_unit(&fixture->unit), SCPE_OK);

    assert_int_equal(simh_test_remove_path(fixture->temp_dir), 0);
    free(fixture);
    *state = NULL;
    return 0;
}

/* Verify unit-to-device lookup finds and caches the owning device. */
static void test_find_dev_from_unit_locates_simulator_device(void **state)
{
    struct scp_unit_fixture *fixture = *state;

    fixture->unit.dptr = NULL;

    assert_ptr_equal(find_dev_from_unit(&fixture->unit), &fixture->device);
    assert_ptr_equal(fixture->unit.dptr, &fixture->device);
}

/* Verify attach creates a new empty file and detach clears state cleanly. */
static void test_attach_unit_creates_new_file_and_detaches_cleanly(void **state)
{
    struct scp_unit_fixture *fixture = *state;
    void *file_data;
    size_t file_size;

    sim_switches = SWMASK('N');

    assert_int_equal(attach_unit(&fixture->unit, fixture->file_path), SCPE_OK);
    assert_true((fixture->unit.flags & UNIT_ATT) != 0);
    assert_non_null(fixture->unit.fileref);
    assert_non_null(fixture->unit.filename);

    assert_int_equal(detach_unit(&fixture->unit), SCPE_OK);
    assert_true((fixture->unit.flags & UNIT_ATT) == 0);
    assert_null(fixture->unit.fileref);
    assert_null(fixture->unit.filename);

    assert_int_equal(
        simh_test_read_file(fixture->file_path, &file_data, &file_size), 0);
    assert_int_equal(file_size, 0);
    free(file_data);
}

/* Verify read-only attach mode sets and clears UNIT_RO as expected. */
static void test_attach_unit_honors_read_only_mode(void **state)
{
    static const uint8_t initial_data[] = {0x11, 0x22, 0x33, 0x44};
    struct scp_unit_fixture *fixture = *state;

    fixture->unit.flags = UNIT_ATTABLE | UNIT_ROABLE;
    assert_int_equal(simh_test_write_file(fixture->file_path, initial_data,
                                          sizeof(initial_data)),
                     0);

    sim_switches = SWMASK('R');

    assert_int_equal(attach_unit(&fixture->unit, fixture->file_path), SCPE_OK);
    assert_true((fixture->unit.flags & UNIT_RO) != 0);

    assert_int_equal(detach_unit(&fixture->unit), SCPE_OK);
    assert_true((fixture->unit.flags & UNIT_RO) == 0);
}

/* Verify attach rejects units that are not marked attachable. */
static void test_attach_unit_rejects_non_attachable_units(void **state)
{
    struct scp_unit_fixture *fixture = *state;

    fixture->unit.flags = 0;

    assert_int_equal(attach_unit(&fixture->unit, fixture->file_path),
                     SCPE_NOATT);
    assert_null(fixture->unit.filename);
    assert_null(fixture->unit.fileref);
}

/* Verify -E turns a missing input file into an open error. */
static void test_attach_unit_honors_existing_file_requirement(void **state)
{
    struct scp_unit_fixture *fixture = *state;

    sim_switches = SWMASK('E');

    assert_int_equal(
        SCPE_BARE_STATUS(attach_unit(&fixture->unit, fixture->file_path)),
        SCPE_OPENERR);
    assert_null(fixture->unit.filename);
    assert_null(fixture->unit.fileref);
}

/* Verify forced read-only attach is rejected for non-ROABLE units. */
static void test_attach_unit_rejects_read_only_without_roable(void **state)
{
    static const uint8_t initial_data[] = {0x10, 0x20};
    struct scp_unit_fixture *fixture = *state;

    fixture->unit.flags = UNIT_ATTABLE;
    assert_int_equal(simh_test_write_file(fixture->file_path, initial_data,
                                          sizeof(initial_data)),
                     0);

    sim_switches = SWMASK('R');

    assert_int_equal(
        SCPE_BARE_STATUS(attach_unit(&fixture->unit, fixture->file_path)),
        SCPE_NORO);
    assert_null(fixture->unit.filename);
    assert_null(fixture->unit.fileref);
}

/* Verify detach writes buffered changes back to the attached file. */
static void test_detach_unit_flushes_buffered_changes(void **state)
{
    static const uint8_t initial_data[] = {'A', 'B', 'C', 'D'};
    static const uint8_t expected_data[] = {'Z', 'B', 'C', 'D'};
    struct scp_unit_fixture *fixture = *state;
    void *file_data;
    size_t file_size;

    fixture->unit.flags = UNIT_ATTABLE | UNIT_BUFABLE | UNIT_MUSTBUF;
    fixture->unit.capac = sizeof(initial_data);
    fixture->device.dwidth = 8;
    fixture->device.aincr = 1;

    assert_int_equal(simh_test_write_file(fixture->file_path, initial_data,
                                          sizeof(initial_data)),
                     0);

    assert_int_equal(attach_unit(&fixture->unit, fixture->file_path), SCPE_OK);
    assert_non_null(fixture->unit.filebuf);
    assert_non_null(fixture->unit.filebuf2);

    ((uint8_t *)fixture->unit.filebuf)[0] = 'Z';

    assert_int_equal(detach_unit(&fixture->unit), SCPE_OK);
    assert_null(fixture->unit.filebuf);
    assert_null(fixture->unit.filebuf2);

    assert_int_equal(
        simh_test_read_file(fixture->file_path, &file_data, &file_size), 0);
    assert_int_equal(file_size, sizeof(expected_data));
    assert_memory_equal(file_data, expected_data, sizeof(expected_data));
    free(file_data);
}

/* Verify detach reports null and unattached units consistently. */
static void test_detach_unit_rejects_null_and_unattached_units(void **state)
{
    struct scp_unit_fixture *fixture = *state;

    (void)fixture;

    assert_int_equal(detach_unit(NULL), SCPE_IERR);
    assert_int_equal(detach_unit(&fixture->unit), SCPE_UNATT);

    fixture->unit.flags = UNIT_ATTABLE;
    sim_switches = SIM_SW_REST;
    assert_int_equal(detach_unit(&fixture->unit), SCPE_OK);
}

/* Verify active units still block a normal device disable. */
static void test_set_device_disabled_rejects_active_units(void **state)
{
    struct scp_unit_fixture *fixture = *state;

    fixture->device.flags = DEV_DISABLE;
    sim_switches = 0;

    assert_int_equal(sim_activate(&fixture->unit, 100), SCPE_OK);
    assert_true(sim_is_active(&fixture->unit));

    assert_int_equal(
        SCPE_BARE_STATUS(set_dev_enbdis(&fixture->device, NULL, 0, NULL)),
        SCPE_NOFNC);
    assert_false((fixture->device.flags & DEV_DIS) != 0);
    assert_true(sim_is_active(&fixture->unit));
}

/* Verify -F cancels active units before disabling a device. */
static void test_set_device_disabled_force_cancels_active_units(void **state)
{
    struct scp_unit_fixture *fixture = *state;

    fixture->device.flags = DEV_DISABLE;
    sim_switches = SWMASK('F');

    assert_int_equal(sim_activate(&fixture->unit, 100), SCPE_OK);
    assert_true(sim_is_active(&fixture->unit));

    assert_int_equal(set_dev_enbdis(&fixture->device, NULL, 0, NULL), SCPE_OK);
    assert_true((fixture->device.flags & DEV_DIS) != 0);
    assert_false(sim_is_active(&fixture->unit));
}

/* Verify -F detaches attached units before disabling a device. */
static void test_set_device_disabled_force_detaches_attached_units(void **state)
{
    struct scp_unit_fixture *fixture = *state;

    fixture->device.flags = DEV_DISABLE;
    fixture->unit.flags = UNIT_ATTABLE;
    sim_switches = SWMASK('N');

    assert_int_equal(attach_unit(&fixture->unit, fixture->file_path), SCPE_OK);
    assert_true((fixture->unit.flags & UNIT_ATT) != 0);

    sim_switches = SWMASK('F');

    assert_int_equal(set_dev_enbdis(&fixture->device, NULL, 0, NULL), SCPE_OK);
    assert_true((fixture->device.flags & DEV_DIS) != 0);
    assert_false((fixture->unit.flags & UNIT_ATT) != 0);
    assert_null(fixture->unit.fileref);
    assert_null(fixture->unit.filename);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_find_dev_from_unit_locates_simulator_device,
            setup_scp_unit_fixture, teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(
            test_attach_unit_creates_new_file_and_detaches_cleanly,
            setup_scp_unit_fixture, teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(test_attach_unit_honors_read_only_mode,
                                        setup_scp_unit_fixture,
                                        teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(
            test_attach_unit_rejects_non_attachable_units,
            setup_scp_unit_fixture, teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(
            test_attach_unit_honors_existing_file_requirement,
            setup_scp_unit_fixture, teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(
            test_attach_unit_rejects_read_only_without_roable,
            setup_scp_unit_fixture, teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(
            test_detach_unit_flushes_buffered_changes, setup_scp_unit_fixture,
            teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(
            test_detach_unit_rejects_null_and_unattached_units,
            setup_scp_unit_fixture, teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(
            test_set_device_disabled_rejects_active_units,
            setup_scp_unit_fixture, teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(
            test_set_device_disabled_force_cancels_active_units,
            setup_scp_unit_fixture, teardown_scp_unit_fixture),
        cmocka_unit_test_setup_teardown(
            test_set_device_disabled_force_detaches_attached_units,
            setup_scp_unit_fixture, teardown_scp_unit_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
