#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scp.h"
#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_disk.h"
#include "test_simh_personality.h"
#include "test_support.h"

struct sim_disk_fixture {
    DEVICE byte_device;
    UNIT byte_unit;
    DEVICE sector_device;
    UNIT sector_unit;
    char temp_dir[CBUFSIZE];
    char image_path[CBUFSIZE];
};

extern t_offset pseudo_filesystem_size;

static MTAB disk_unit_modifiers[] = {
    {DKUF_NOAUTOSIZE, 0, "autosize", "AUTOSIZE", NULL},
    {DKUF_NOAUTOSIZE, DKUF_NOAUTOSIZE, "noautosize", "NOAUTOSIZE", NULL},
    {0},
};

static int setup_sim_disk_fixture(void **state)
{
    struct sim_disk_fixture *fixture;
    DEVICE *devices[3];

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    simh_test_reset_simulator_state();

    simh_test_init_device_unit(&fixture->byte_device, &fixture->byte_unit,
                               "DSKB", "DSKB0", DEV_DISABLE | DEV_DISK,
                               UNIT_ATTABLE | UNIT_ROABLE, 8, 1);
    fixture->byte_unit.flags |= DK_F_STD;
    fixture->byte_device.modifiers = disk_unit_modifiers;

    simh_test_init_device_unit(&fixture->sector_device, &fixture->sector_unit,
                               "DSKS", "DSKS0",
                               DEV_DISABLE | DEV_DISK | DEV_SECTORS,
                               UNIT_ATTABLE | UNIT_ROABLE, 16, 1);
    fixture->sector_unit.flags |= DK_F_STD;
    fixture->sector_unit.capac = 1;
    fixture->sector_device.modifiers = disk_unit_modifiers;

    assert_int_equal(simh_test_make_temp_dir(fixture->temp_dir,
                                             sizeof(fixture->temp_dir),
                                             "sim-disk-test"),
                     0);
    assert_int_equal(simh_test_join_path(fixture->image_path,
                                         sizeof(fixture->image_path),
                                         fixture->temp_dir, "disk.img"),
                     0);

    devices[0] = &fixture->byte_device;
    devices[1] = &fixture->sector_device;
    devices[2] = NULL;
    assert_int_equal(simh_test_set_devices(devices), 0);
    sim_dflt_dev = &fixture->byte_device;
    sim_quiet = 0;
    sim_show_message = 1;
    sim_switches = 0;

    *state = fixture;
    return 0;
}

static int teardown_sim_disk_fixture(void **state)
{
    struct sim_disk_fixture *fixture = *state;

    sim_disk_clear_all_test_backends();
    pseudo_filesystem_size = 0;
    sim_switches = 0;
    simh_test_remove_path(fixture->temp_dir);
    simh_test_reset_simulator_state();
    free(fixture);
    *state = NULL;
    return 0;
}

static void create_temp_disk_image(struct sim_disk_fixture *fixture,
                                   size_t size)
{
    uint8 *data;

    data = calloc(1, size);
    assert_non_null(data);
    assert_int_equal(simh_test_write_file(fixture->image_path, data, size), 0);
    free(data);
}

static void attach_temp_disk_image(struct sim_disk_fixture *fixture, UNIT *uptr)
{
    create_temp_disk_image(fixture, 512);
    sim_switches = 0;
    assert_int_equal(sim_disk_attach_ex(uptr, fixture->image_path, 512, 1, true,
                                        0, "TEST", 0, 0, NULL),
                     SCPE_OK);
}

static t_stat test_backend_read(UNIT *uptr, t_lba lba, uint8 *buf,
                                t_seccnt *sectsread, t_seccnt sects)
{
    (void)buf;

    uptr->u3++;
    assert_int_equal(lba, 7);
    assert_int_equal(sects, 3);
    if (sectsread != NULL)
        *sectsread = sects;
    return SCPE_OK;
}

static t_stat test_backend_write(UNIT *uptr, t_lba lba, uint8 *buf,
                                 t_seccnt *sectswritten, t_seccnt sects)
{
    (void)buf;

    uptr->u3++;
    assert_int_equal(lba, 11);
    assert_int_equal(sects, 5);
    if (sectswritten != NULL)
        *sectswritten = sects;
    return SCPE_OK;
}

static void assert_disk_show_output(t_stat (*show_fn)(FILE *, UNIT *, int32,
                                                      const void *),
                                    UNIT *uptr, const char *expected)
{
    FILE *stream;
    char *text;
    size_t size;

    stream = tmpfile();
    assert_non_null(stream);

    assert_int_equal(show_fn(stream, uptr, 0, NULL), SCPE_OK);
    assert_int_equal(simh_test_read_stream(stream, &text, &size), 0);
    assert_string_equal(text, expected);

    free(text);
    fclose(stream);
}

/* Verify disk format selection updates the unit flags and that the show
   helper reflects the chosen format. */
static void test_sim_disk_set_and_show_format(void **state)
{
    struct sim_disk_fixture *fixture = *state;

    assert_int_equal(sim_disk_set_fmt(&fixture->byte_unit, 0, "SIMH", NULL),
                     SCPE_OK);
    assert_int_equal(DK_GET_FMT(&fixture->byte_unit), DKUF_F_STD);
    assert_disk_show_output(sim_disk_show_fmt, &fixture->byte_unit,
                            "SIMH format");

    assert_true(sim_disk_set_fmt(&fixture->byte_unit, 0, "UNKNOWN", NULL) !=
                SCPE_OK);
}

/* Verify byte-addressed disk capacity parsing stores byte counts and the
   show helper renders them with byte units. */
static void test_sim_disk_set_and_show_byte_capacity(void **state)
{
    struct sim_disk_fixture *fixture = *state;

    assert_int_equal(sim_disk_set_capac(&fixture->byte_unit, 0, "25", NULL),
                     SCPE_OK);
    assert_int_equal(fixture->byte_unit.capac, (t_addr)25000000);
    assert_disk_show_output(sim_disk_show_capac, &fixture->byte_unit,
                            "capacity=25MB");

    fixture->byte_unit.flags |= UNIT_ATT;
    assert_int_equal(sim_disk_set_capac(&fixture->byte_unit, 0, "5", NULL),
                     SCPE_ALATT);
}

/* Verify sector-addressed capacity parsing converts megabytes into
   sector counts for DEV_SECTORS devices. */
static void test_sim_disk_set_capacity_scales_for_sector_devices(void **state)
{
    struct sim_disk_fixture *fixture = *state;

    assert_int_equal(sim_disk_set_capac(&fixture->sector_unit, 0, "8", NULL),
                     SCPE_OK);
    assert_int_equal(fixture->sector_unit.capac, (t_addr)(8000000 / 512));
}

/* Verify availability and write-protect predicates track simple unit
   flag state without needing a live disk context. */
static void test_sim_disk_status_predicates_use_unit_flags(void **state)
{
    struct sim_disk_fixture *fixture = *state;

    assert_false(sim_disk_isavailable(&fixture->byte_unit));
    assert_false(sim_disk_wrp(&fixture->byte_unit));

    fixture->byte_unit.flags |= DKUF_WRP;
    assert_true(sim_disk_wrp(&fixture->byte_unit));
}

/* Verify the process-local test backend intercepts sector reads before the
   normal disk context is required. */
static void test_sim_disk_test_backend_intercepts_read(void **state)
{
    struct sim_disk_fixture *fixture = *state;
    SIM_DISK_TEST_BACKEND backend = {
        .rdsect = test_backend_read,
    };
    uint8 data[1] = {0};
    t_seccnt sectsread = 0;

    assert_int_equal(sim_disk_set_test_backend(&fixture->byte_unit, &backend),
                     SCPE_OK);
    assert_int_equal(
        sim_disk_rdsect(&fixture->byte_unit, 7, data, &sectsread, 3), SCPE_OK);
    assert_int_equal(sectsread, 3);
    assert_int_equal(fixture->byte_unit.u3, 1);
}

/* Verify the process-local test backend intercepts sector writes before the
   normal disk context is required. */
static void test_sim_disk_test_backend_intercepts_write(void **state)
{
    struct sim_disk_fixture *fixture = *state;
    SIM_DISK_TEST_BACKEND backend = {
        .wrsect = test_backend_write,
    };
    uint8 data[1] = {0};
    t_seccnt sectswritten = 0;

    assert_int_equal(sim_disk_set_test_backend(&fixture->sector_unit, &backend),
                     SCPE_OK);
    assert_int_equal(
        sim_disk_wrsect(&fixture->sector_unit, 11, data, &sectswritten, 5),
        SCPE_OK);
    assert_int_equal(sectswritten, 5);
    assert_int_equal(fixture->sector_unit.u3, 1);
}

static void test_sim_disk_test_backend_rejects_null_unit(void **state)
{
    SIM_DISK_TEST_BACKEND backend = {
        .rdsect = test_backend_read,
    };

    (void)state;

    assert_int_equal(sim_disk_set_test_backend(NULL, &backend), SCPE_ARG);
    sim_disk_clear_test_backend(NULL);
    sim_disk_clear_all_test_backends();
}

static void test_sim_disk_size_restores_nonboolean_quiet_value(void **state)
{
    struct sim_disk_fixture *fixture = *state;
    int32 saved_quiet = sim_quiet;

    attach_temp_disk_image(fixture, &fixture->sector_unit);
    pseudo_filesystem_size = 1024;

    sim_quiet = 7;

    assert_int_equal(sim_disk_size(&fixture->sector_unit), 1024);
    assert_int_equal(sim_quiet, 7);

    sim_quiet = saved_quiet;
    pseudo_filesystem_size = 0;
    assert_int_equal(sim_disk_detach(&fixture->sector_unit), SCPE_OK);
}

static void test_sim_disk_detach_restores_auto_format(void **state)
{
    struct sim_disk_fixture *fixture = *state;
    char attach_arg[2 * CBUFSIZE];

    assert_int_equal(sim_disk_set_fmt(&fixture->sector_unit, 0, "AUTO", NULL),
                     SCPE_OK);
    create_temp_disk_image(fixture, 512);
    assert_true(snprintf(attach_arg, sizeof(attach_arg), "SIMH %s",
                         fixture->image_path) < (int)sizeof(attach_arg));
    sim_switches = SWMASK('F');
    assert_int_equal(sim_disk_attach_ex(&fixture->sector_unit, attach_arg, 512,
                                        1, true, 0, "TEST", 0, 0, NULL),
                     SCPE_OK);

    assert_int_not_equal(DK_GET_FMT(&fixture->sector_unit), DKUF_F_AUTO);

    assert_int_equal(sim_disk_detach(&fixture->sector_unit), SCPE_OK);
    assert_int_equal(DK_GET_FMT(&fixture->sector_unit), DKUF_F_AUTO);
}

static void test_sim_disk_set_noautosize_normalizes_global_flag(void **state)
{
    struct sim_disk_fixture *fixture = *state;

    assert_int_equal(sim_disk_init(), SCPE_OK);
    assert_int_equal(fixture->byte_unit.flags & DKUF_NOAUTOSIZE, 0);
    assert_int_equal(fixture->sector_unit.flags & DKUF_NOAUTOSIZE, 0);

    assert_int_equal(sim_disk_set_noautosize(7, NULL), SCPE_OK);
    assert_int_equal(fixture->byte_unit.flags & DKUF_NOAUTOSIZE,
                     DKUF_NOAUTOSIZE);
    assert_int_equal(fixture->sector_unit.flags & DKUF_NOAUTOSIZE,
                     DKUF_NOAUTOSIZE);

    assert_int_equal(SCPE_BARE_STATUS(sim_disk_set_noautosize(1, NULL)),
                     SCPE_ARG);

    assert_int_equal(sim_disk_set_noautosize(false, NULL), SCPE_OK);
    assert_int_equal(fixture->byte_unit.flags & DKUF_NOAUTOSIZE, 0);
    assert_int_equal(fixture->sector_unit.flags & DKUF_NOAUTOSIZE, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_sim_disk_set_and_show_format,
                                        setup_sim_disk_fixture,
                                        teardown_sim_disk_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_disk_set_and_show_byte_capacity, setup_sim_disk_fixture,
            teardown_sim_disk_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_disk_set_capacity_scales_for_sector_devices,
            setup_sim_disk_fixture, teardown_sim_disk_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_disk_status_predicates_use_unit_flags,
            setup_sim_disk_fixture, teardown_sim_disk_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_disk_test_backend_intercepts_read, setup_sim_disk_fixture,
            teardown_sim_disk_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_disk_test_backend_intercepts_write, setup_sim_disk_fixture,
            teardown_sim_disk_fixture),
        cmocka_unit_test(test_sim_disk_test_backend_rejects_null_unit),
        cmocka_unit_test_setup_teardown(
            test_sim_disk_size_restores_nonboolean_quiet_value,
            setup_sim_disk_fixture, teardown_sim_disk_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_disk_detach_restores_auto_format, setup_sim_disk_fixture,
            teardown_sim_disk_fixture),
        cmocka_unit_test_setup_teardown(
            test_sim_disk_set_noautosize_normalizes_global_flag,
            setup_sim_disk_fixture, teardown_sim_disk_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
