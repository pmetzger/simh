#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_tempfile.h"
#include "swtp_defs.h"

#include "dc-4.c"

struct dc4_fixture {
    FILE *file;
    char path[512];
};

static int setup_dc4(void **state)
{
    struct dc4_fixture *fixture = calloc(1, sizeof(*fixture));

    assert_non_null(fixture);
    *state = fixture;

    sim_finit();

    for (int unit = 0; unit < NUM_DISK; ++unit) {
        dsk_unit[unit].fileref = NULL;
        dsk_unit[unit].flags &=
            ~(UNIT_ATT | UNIT_ENABLE | UNIT_RO | UNIT_BUF | UNIT_WPRT);
        free(dsk_unit[unit].filebuf);
        dsk_unit[unit].filebuf = NULL;
    }

    assert_int_equal(dsk_reset(&dsk_dev), SCPE_OK);
    return 0;
}

static int teardown_dc4(void **state)
{
    struct dc4_fixture *fixture = *state;

    for (int unit = 0; unit < NUM_DISK; ++unit) {
        dsk_unit[unit].fileref = NULL;
        dsk_unit[unit].flags &= ~(UNIT_ATT | UNIT_ENABLE | UNIT_RO);
        free(dsk_unit[unit].filebuf);
        dsk_unit[unit].filebuf = NULL;
    }

    if (fixture != NULL) {
        if (fixture->file != NULL)
            fclose(fixture->file);
        if (fixture->path[0] != '\0')
            remove(fixture->path);
        free(fixture);
    }
    *state = NULL;
    return 0;
}

static void open_image(struct dc4_fixture *fixture)
{
    fixture->file = sim_tempfile_open_stream(
        fixture->path, sizeof(fixture->path), "zimh-dc4-", ".img", "w+b");
    assert_non_null(fixture->file);
}

static void size_image(FILE *file, long size)
{
    assert_int_equal(fseek(file, size - 1, SEEK_SET), 0);
    assert_true(fputc(0, file) != EOF);
    assert_int_equal(fflush(file), 0);
    assert_int_equal(fseek(file, 0, SEEK_SET), 0);
}

static void write_sir_geometry(FILE *file, uint8_t tracks, uint8_t sectors)
{
    uint8_t sir[SECT_SIZE];

    memset(sir, 0, sizeof(sir));
    sir[MAXCYL] = tracks - 1;
    sir[MAXSEC] = sectors;

    assert_int_equal(fseek(file, 0x200, SEEK_SET), 0);
    assert_int_equal(fwrite(sir, 1, sizeof(sir), file), sizeof(sir));
    assert_int_equal(fflush(file), 0);
    assert_int_equal(fseek(file, 0, SEEK_SET), 0);
}

static void attach_image_to_unit(int unit, FILE *file)
{
    dsk_unit[unit].fileref = file;
    dsk_unit[unit].flags |= UNIT_ATT;
    dsk_unit[unit].u3 &= ~NOTRDY;
}

/*
 * Selecting a drive reads the SIR sector and derives FLEX geometry from it.
 * The controller also reports that the buffered SIR data is ready to read.
 */
static void
test_drive_select_reads_sir_geometry_and_sets_data_ready(void **state)
{
    struct dc4_fixture *fixture = *state;

    open_image(fixture);
    size_image(fixture->file, 100000);
    write_sir_geometry(fixture->file, 40, 12);
    attach_image_to_unit(0, fixture->file);
    dsk_unit[0].flags |= UNIT_ENABLE;
    dsk_unit[0].u3 = LOST | NOTRDY;
    cur_dsk = NUM_DISK;

    assert_int_equal(fdcdrv(1, 0), 0);

    assert_int_equal(cur_dsk, 0);
    assert_int_equal(spt, 12);
    assert_int_equal(cpd, 40);
    assert_int_equal(sectsize, SECT_SIZE);
    assert_int_equal(sector_base, 1);
    assert_int_equal(trksiz, 12 * SECT_SIZE);
    assert_int_equal(dsksiz, 40 * 12 * SECT_SIZE);
    assert_int_equal(heds, 0);
    assert_true((dsk_unit[0].u3 & LOST) == 0);
    assert_true((dsk_unit[0].u3 & WRPROT) == 0);
    assert_true((dsk_unit[0].u3 & (BUSY | DRQ)) == (BUSY | DRQ));
    assert_int_equal(dsk_unit[0].pos, 0);
}

/*
 * Selecting an unattached drive only records the selection and write-protect
 * state. It must not invent geometry or mark sector data as ready.
 */
static void
test_drive_select_unattached_drive_does_not_read_geometry(void **state)
{
    (void)state;

    dsk_unit[2].u3 = LOST;

    assert_int_equal(fdcdrv(1, 2), 0);

    assert_int_equal(cur_dsk, 2);
    assert_true((dsk_unit[2].u3 & LOST) == 0);
    assert_true((dsk_unit[2].u3 & WRPROT) != 0);
    assert_true((dsk_unit[2].u3 & (BUSY | DRQ)) == 0);
    assert_int_equal(spt, 0);
    assert_int_equal(cpd, 0);
    assert_int_equal(trksiz, 0);
}

/*
 * A command/status read with no attached image reports the controller's seek
 * error value and latches NOTRDY in the unit status.
 */
static void test_command_status_without_attachment_sets_not_ready(void **state)
{
    (void)state;

    cur_dsk = 0;
    dsk_unit[0].u3 = 0;

    assert_int_equal(fdccmd(0, 0), SEEKERR);

    assert_true((dsk_unit[0].u3 & NOTRDY) != 0);
}

/*
 * Status reads on an attached drive return the unit status and synthesize the
 * index bit only when the type-I command countdown expires.
 */
static void
test_attached_status_read_reports_index_after_countdown(void **state)
{
    struct dc4_fixture *fixture = *state;

    open_image(fixture);
    size_image(fixture->file, 35 * 10 * SECT_SIZE);
    attach_image_to_unit(0, fixture->file);
    cur_dsk = 0;
    dsk_unit[0].u3 = BUSY;
    index_countdown = 2;

    assert_int_equal(fdccmd(0, 0), BUSY);
    assert_int_equal(fdccmd(0, 0), BUSY | INDEX);
    assert_int_equal(index_countdown, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_drive_select_reads_sir_geometry_and_sets_data_ready, setup_dc4,
            teardown_dc4),
        cmocka_unit_test_setup_teardown(
            test_drive_select_unattached_drive_does_not_read_geometry,
            setup_dc4, teardown_dc4),
        cmocka_unit_test_setup_teardown(
            test_command_status_without_attachment_sets_not_ready, setup_dc4,
            teardown_dc4),
        cmocka_unit_test_setup_teardown(
            test_attached_status_read_reports_index_after_countdown, setup_dc4,
            teardown_dc4),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
