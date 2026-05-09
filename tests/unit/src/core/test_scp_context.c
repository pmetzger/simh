#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "test_scp_fixture.h"
#include "test_simh_personality.h"
#include "test_support.h"

extern t_stat show_config(FILE *st, DEVICE *dnotused, UNIT *unotused,
                          int32_t flag, const char *cptr);

struct scp_context_fixture {
    DEVICE disk;
    UNIT disk_units[2];
    DEVICE reader;
    UNIT reader_unit;
    DEVICE disabled;
    UNIT disabled_unit;
    DEVICE internal;
    UNIT internal_unit;
};

static int setup_scp_context_fixture(void **state)
{
    struct scp_context_fixture *fixture;
    DEVICE *devices[4];

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    simh_test_init_multiunit_device(&fixture->disk, fixture->disk_units, 2,
                                    "DQ", "SYSDISK", 0);
    simh_test_init_device_unit(&fixture->reader, &fixture->reader_unit, "PTR",
                               NULL, 0, 0, 8, 1);
    fixture->reader.lname = "READER";
    fixture->reader_unit.uname = NULL;

    simh_test_init_device_unit(&fixture->disabled, &fixture->disabled_unit,
                               "TTY", NULL, DEV_DIS, 0, 8, 1);
    fixture->disabled_unit.uname = NULL;

    simh_test_init_device_unit(&fixture->internal, &fixture->internal_unit,
                               "CLK", NULL, 0, 0, 8, 1);
    fixture->internal.lname = "CLOCK";
    fixture->internal_unit.uname = NULL;

    devices[0] = &fixture->disk;
    devices[1] = &fixture->reader;
    devices[2] = &fixture->disabled;
    devices[3] = NULL;
    assert_int_equal(
        simh_test_install_devices("zimh-unit-scp-context", devices), 0);

    *state = fixture;
    return 0;
}

static int teardown_scp_context_fixture(void **state)
{
    struct scp_context_fixture *fixture = *state;

    simh_test_free_unit_names(fixture->disk_units, 2);
    simh_test_free_unit_names(&fixture->reader_unit, 1);
    simh_test_free_unit_names(&fixture->disabled_unit, 1);
    simh_test_free_unit_names(&fixture->internal_unit, 1);
    simh_test_reset_simulator_state();
    free(fixture);
    *state = NULL;
    return 0;
}

/* Verify device display names prefer logical aliases over physical names. */
static void test_sim_dname_prefers_logical_name(void **state)
{
    struct scp_context_fixture *fixture = *state;

    assert_string_equal(sim_dname(&fixture->disk), "SYSDISK");
    fixture->disk.lname = NULL;
    assert_string_equal(sim_dname(&fixture->disk), "DQ");
    assert_string_equal(sim_dname(NULL), "");
}

/* Verify unit display names are formatted once and then cached. */
static void test_sim_uname_formats_and_caches_names(void **state)
{
    struct scp_context_fixture *fixture = *state;
    const char *first_name;
    const char *cached_name;

    first_name = sim_uname(&fixture->disk_units[1]);
    assert_string_equal(first_name, "SYSDISK1");

    cached_name = sim_uname(&fixture->disk_units[1]);
    assert_ptr_equal(cached_name, first_name);

    assert_string_equal(sim_uname(&fixture->reader_unit), "READER");
    assert_string_equal(sim_uname(NULL), "");
}

/* Verify replacing a cached unit name frees and stores the new value. */
static void test_sim_set_uname_replaces_cached_name(void **state)
{
    struct scp_context_fixture *fixture = *state;
    const char *first_name;
    const char *second_name;

    first_name = sim_set_uname(&fixture->disk_units[0], "CUSTOM0");
    assert_string_equal(first_name, "CUSTOM0");

    second_name = sim_set_uname(&fixture->disk_units[0], "RENAMED0");
    assert_string_equal(second_name, "RENAMED0");
    assert_string_equal(fixture->disk_units[0].uname, "RENAMED0");
}

/* Verify device lookup sees primary, logical, and internal device names. */
static void
test_find_dev_matches_primary_logical_and_internal_names(void **state)
{
    struct scp_context_fixture *fixture = *state;

    assert_ptr_equal(find_dev("DQ"), &fixture->disk);
    assert_ptr_equal(find_dev("SYSDISK"), &fixture->disk);
    assert_null(find_dev("CLOCK"));

    assert_int_equal(sim_register_internal_device(&fixture->internal), SCPE_OK);
    assert_ptr_equal(find_dev("CLK"), &fixture->internal);
    assert_ptr_equal(find_dev("CLOCK"), &fixture->internal);
}

/* Verify unit lookup matches bare, qualified, and cached unit names. */
static void
test_find_unit_matches_device_names_and_cached_unit_names(void **state)
{
    struct scp_context_fixture *fixture = *state;
    UNIT *uptr;
    DEVICE *dptr;

    uptr = NULL;
    dptr = find_unit("DQ", &uptr);
    assert_ptr_equal(dptr, &fixture->disk);
    assert_ptr_equal(uptr, &fixture->disk_units[0]);

    uptr = NULL;
    dptr = find_unit("SYSDISK1", &uptr);
    assert_ptr_equal(dptr, &fixture->disk);
    assert_ptr_equal(uptr, &fixture->disk_units[1]);

    uptr = NULL;
    dptr = find_unit("READER", &uptr);
    assert_ptr_equal(dptr, &fixture->reader);
    assert_ptr_equal(uptr, &fixture->reader_unit);

    assert_string_equal(sim_set_uname(&fixture->disk_units[1], "CUSTOM1"),
                        "CUSTOM1");
    uptr = NULL;
    dptr = find_unit("CUSTOM1", &uptr);
    assert_ptr_equal(dptr, &fixture->disk);
    assert_ptr_equal(uptr, &fixture->disk_units[1]);
}

/* Verify invalid numeric suffixes leave the unit unresolved. */
static void test_find_unit_rejects_invalid_unit_numbers(void **state)
{
    struct scp_context_fixture *fixture = *state;
    UNIT *uptr;
    DEVICE *dptr;

    uptr = (UNIT *)1;
    dptr = find_unit("DQ99", &uptr);
    assert_ptr_equal(dptr, &fixture->disk);
    assert_null(uptr);

    uptr = (UNIT *)1;
    dptr = find_unit("SYSDISK2", &uptr);
    assert_ptr_equal(dptr, &fixture->disk);
    assert_null(uptr);
}

/* Verify disabled devices are rejected by lookup and flagged as disabled. */
static void test_find_unit_rejects_disabled_devices(void **state)
{
    struct scp_context_fixture *fixture = *state;
    UNIT *uptr;

    uptr = (UNIT *)1;
    assert_null(find_unit("TTY", &uptr));
    assert_null(uptr);

    assert_true(qdisable(&fixture->disabled));
    assert_false(qdisable(&fixture->disk));
}

/* Verify SHOW CONFIGURATION -E filters disabled devices. */
static void test_show_config_enabled_filter_hides_disabled_devices(void **state)
{
    FILE *stream;
    char *text;
    size_t text_size;
    int32_t saved_switches;

    (void)state;

    stream = tmpfile();
    assert_non_null(stream);

    saved_switches = sim_switches;
    sim_switches = SWMASK('E');
    assert_int_equal(show_config(stream, NULL, NULL, 0, NULL), SCPE_OK);
    sim_switches = saved_switches;

    assert_int_equal(simh_test_read_stream(stream, &text, &text_size), 0);
    fclose(stream);

    assert_non_null(strstr(text, "enabled devices"));
    assert_non_null(strstr(text, "SYSDISK"));
    assert_non_null(strstr(text, "READER"));
    assert_null(strstr(text, "TTY"));

    free(text);
}

/* Verify internal devices remain available by name but not unit lookup. */
static void test_internal_devices_do_not_participate_in_find_unit(void **state)
{
    struct scp_context_fixture *fixture = *state;
    UNIT *uptr;

    fixture->internal.flags = DEV_DIS;
    assert_int_equal(sim_register_internal_device(&fixture->internal), SCPE_OK);
    assert_ptr_equal(find_dev("CLK"), &fixture->internal);

    uptr = (UNIT *)1;
    assert_null(find_unit("CLK", &uptr));
    assert_null(uptr);
}

/* Verify internal-device registration ignores duplicates and main devices. */
static void test_register_internal_device_ignores_duplicates(void **state)
{
    struct scp_context_fixture *fixture = *state;

    assert_int_equal(sim_internal_device_count, 0);

    assert_int_equal(sim_register_internal_device(&fixture->internal), SCPE_OK);
    assert_int_equal(sim_internal_device_count, 1);
    assert_ptr_equal(sim_internal_devices[0], &fixture->internal);
    assert_null(sim_internal_devices[1]);

    assert_int_equal(sim_register_internal_device(&fixture->internal), SCPE_OK);
    assert_int_equal(sim_internal_device_count, 1);

    assert_int_equal(sim_register_internal_device(&fixture->disk), SCPE_OK);
    assert_int_equal(sim_internal_device_count, 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_sim_dname_prefers_logical_name,
                                        setup_scp_context_fixture,
                                        teardown_scp_context_fixture),
        cmocka_unit_test_setup_teardown(test_sim_uname_formats_and_caches_names,
                                        setup_scp_context_fixture,
                                        teardown_scp_context_fixture),
        cmocka_unit_test_setup_teardown(test_sim_set_uname_replaces_cached_name,
                                        setup_scp_context_fixture,
                                        teardown_scp_context_fixture),
        cmocka_unit_test_setup_teardown(
            test_find_dev_matches_primary_logical_and_internal_names,
            setup_scp_context_fixture, teardown_scp_context_fixture),
        cmocka_unit_test_setup_teardown(
            test_find_unit_matches_device_names_and_cached_unit_names,
            setup_scp_context_fixture, teardown_scp_context_fixture),
        cmocka_unit_test_setup_teardown(
            test_find_unit_rejects_invalid_unit_numbers,
            setup_scp_context_fixture, teardown_scp_context_fixture),
        cmocka_unit_test_setup_teardown(test_find_unit_rejects_disabled_devices,
                                        setup_scp_context_fixture,
                                        teardown_scp_context_fixture),
        cmocka_unit_test_setup_teardown(
            test_show_config_enabled_filter_hides_disabled_devices,
            setup_scp_context_fixture, teardown_scp_context_fixture),
        cmocka_unit_test_setup_teardown(
            test_internal_devices_do_not_participate_in_find_unit,
            setup_scp_context_fixture, teardown_scp_context_fixture),
        cmocka_unit_test_setup_teardown(
            test_register_internal_device_ignores_duplicates,
            setup_scp_context_fixture, teardown_scp_context_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
