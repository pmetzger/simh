#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_imd.h"
#include "test_support.h"

struct sim_imd_fixture {
    char temp_dir[1024];
    char image_path[1024];
};

static int setup_sim_imd_fixture(void **state)
{
    struct sim_imd_fixture *fixture;

    fixture = calloc(1, sizeof(*fixture));
    assert_non_null(fixture);

    assert_int_equal(simh_test_make_temp_dir(fixture->temp_dir,
                                             sizeof(fixture->temp_dir),
                                             "sim-imd"),
                     0);
    assert_int_equal(simh_test_join_path(fixture->image_path,
                                         sizeof(fixture->image_path),
                                         fixture->temp_dir, "image.imd"),
                     0);

    *state = fixture;
    return 0;
}

static int teardown_sim_imd_fixture(void **state)
{
    struct sim_imd_fixture *fixture = *state;

    assert_int_equal(simh_test_remove_path(fixture->temp_dir), 0);
    free(fixture);
    *state = NULL;
    return 0;
}

/* Verify a zero-content IMD file is rejected as invalid media. */
static void test_disk_open_rejects_blank_image(void **state)
{
    struct sim_imd_fixture *fixture = *state;
    FILE *image;

    image = fopen(fixture->image_path, "wb+");
    assert_non_null(image);

    assert_null(diskOpen(image, 0));

    fclose(image);
}

/* Verify obviously malformed track metadata is rejected on open. */
static void test_disk_open_rejects_corrupt_header(void **state)
{
    static const uint8_t corrupt_image[] = {'I',
                                            'M',
                                            'D',
                                            ' ',
                                            't',
                                            'e',
                                            's',
                                            't',
                                            '\n',
                                            0x1A,
                                            IMD_MODE_500K_FM,
                                            MAX_CYL,
                                            0x00,
                                            0x01,
                                            0x00,
                                            0x01,
                                            SECT_RECORD_NORM,
                                            0xE5};
    struct sim_imd_fixture *fixture = *state;
    FILE *image;

    assert_int_equal(simh_test_write_file(fixture->image_path, corrupt_image,
                                          sizeof(corrupt_image)),
                     0);

    image = fopen(fixture->image_path, "rb");
    assert_non_null(image);

    assert_null(diskOpen(image, 0));

    fclose(image);
}

/* Verify a minimal valid image opens and reads its lone sector. */
static void test_disk_open_reads_minimal_single_sector_image(void **state)
{
    struct sim_imd_fixture *fixture = *state;
    uint8_t image_bytes[10 + 5 + 1 + 1 + 128];
    uint8_t sector_data[128];
    uint8_t read_buffer[128];
    uint32_t flags;
    uint32_t readlen;
    FILE *image;
    DISK_INFO *disk;

    memset(sector_data, 0xE5, sizeof(sector_data));

    memcpy(image_bytes, "IMD test\n\x1A", 10);
    image_bytes[10] = IMD_MODE_500K_FM;
    image_bytes[11] = 0x00;
    image_bytes[12] = 0x00;
    image_bytes[13] = 0x01;
    image_bytes[14] = 0x00;
    image_bytes[15] = 0x01;
    image_bytes[16] = SECT_RECORD_NORM;
    memcpy(&image_bytes[17], sector_data, sizeof(sector_data));

    assert_int_equal(simh_test_write_file(fixture->image_path, image_bytes,
                                          sizeof(image_bytes)),
                     0);

    image = fopen(fixture->image_path, "rb");
    assert_non_null(image);

    disk = diskOpen(image, 0);
    assert_non_null(disk);

    assert_int_equal(disk->ntracks, 1);
    assert_int_equal(imdGetSides(disk), 1);
    assert_int_equal(imdIsWriteLocked(disk), 0);
    assert_int_equal(disk->track[0][0].nsects, 1);
    assert_int_equal(disk->track[0][0].sectsize, 128);
    assert_int_equal(disk->track[0][0].start_sector, 1);

    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sectRead(disk, 0, 0, 1, read_buffer, sizeof(read_buffer),
                              &flags, &readlen),
                     SCPE_OK);
    assert_int_equal(flags, 0);
    assert_int_equal(readlen, sizeof(read_buffer));
    assert_memory_equal(read_buffer, sector_data, sizeof(read_buffer));

    assert_int_equal(diskClose(&disk), SCPE_OK);
    assert_null(disk);
    fclose(image);
}

/* Verify compressed sectors expand on read and reject writes when locked. */
static void
test_disk_open_reads_compressed_sector_and_write_protects(void **state)
{
    uint8_t image_bytes[18];
    uint8_t expected_buffer[128];
    uint8_t read_buffer[128];
    struct sim_imd_fixture *fixture = *state;
    uint32_t flags;
    uint32_t readlen;
    FILE *image;
    DISK_INFO *disk;

    memcpy(image_bytes, "IMD test\n\x1A", 10);
    image_bytes[10] = IMD_MODE_500K_FM;
    image_bytes[11] = 0x00;
    image_bytes[12] = 0x00;
    image_bytes[13] = 0x01;
    image_bytes[14] = 0x00;
    image_bytes[15] = 0x01;
    image_bytes[16] = SECT_RECORD_NORM_COMP;
    image_bytes[17] = 0xA5;

    assert_int_equal(simh_test_write_file(fixture->image_path, image_bytes,
                                          sizeof(image_bytes)),
                     0);

    image = fopen(fixture->image_path, "rb+");
    assert_non_null(image);

    disk = diskOpen(image, 0);
    assert_non_null(disk);
    assert_int_equal(imdIsWriteLocked(disk), 1);

    memset(expected_buffer, 0xA5, sizeof(expected_buffer));
    memset(read_buffer, 0, sizeof(read_buffer));
    assert_int_equal(sectRead(disk, 0, 0, 1, read_buffer, sizeof(read_buffer),
                              &flags, &readlen),
                     SCPE_OK);
    assert_int_equal(readlen, sizeof(read_buffer));
    assert_true((flags & IMD_DISK_IO_COMPRESSED) != 0);
    assert_memory_equal(read_buffer, expected_buffer, sizeof(read_buffer));

    flags = 0;
    readlen = 0;
    assert_int_equal(sectWrite(disk, 0, 0, 1, read_buffer, sizeof(read_buffer),
                               &flags, &readlen),
                     SCPE_IOERR);
    assert_true((flags & IMD_DISK_IO_ERROR_WPROT) != 0);
    assert_int_equal(readlen, 0);

    assert_int_equal(diskClose(&disk), SCPE_OK);
    assert_null(disk);
    fclose(image);
}

/* Verify deleted-address-mark state survives a write/read round trip. */
static void test_sect_write_persists_deleted_address_mark(void **state)
{
    uint8_t image_bytes[145];
    uint8_t write_buffer[128];
    uint8_t read_buffer[128];
    struct sim_imd_fixture *fixture = *state;
    uint32_t flags;
    uint32_t readlen;
    FILE *image;
    DISK_INFO *disk;

    memset(write_buffer, 0x5A, sizeof(write_buffer));

    memcpy(image_bytes, "IMD test\n\x1A", 10);
    image_bytes[10] = IMD_MODE_500K_FM;
    image_bytes[11] = 0x00;
    image_bytes[12] = 0x00;
    image_bytes[13] = 0x01;
    image_bytes[14] = 0x00;
    image_bytes[15] = 0x01;
    image_bytes[16] = SECT_RECORD_NORM;
    memset(&image_bytes[17], 0xE5, 128);

    assert_int_equal(simh_test_write_file(fixture->image_path, image_bytes,
                                          sizeof(image_bytes)),
                     0);

    image = fopen(fixture->image_path, "rb+");
    assert_non_null(image);

    disk = diskOpen(image, 0);
    assert_non_null(disk);

    flags = IMD_DISK_IO_DELETED_ADDR_MARK;
    readlen = 0;
    assert_int_equal(sectWrite(disk, 0, 0, 1, write_buffer,
                               sizeof(write_buffer), &flags, &readlen),
                     SCPE_OK);
    assert_int_equal(readlen, sizeof(write_buffer));

    memset(read_buffer, 0, sizeof(read_buffer));
    flags = 0;
    readlen = 0;
    assert_int_equal(sectRead(disk, 0, 0, 1, read_buffer, sizeof(read_buffer),
                              &flags, &readlen),
                     SCPE_OK);
    assert_int_equal(readlen, sizeof(read_buffer));
    assert_true((flags & IMD_DISK_IO_DELETED_ADDR_MARK) != 0);
    assert_memory_equal(read_buffer, write_buffer, sizeof(read_buffer));

    assert_int_equal(diskClose(&disk), SCPE_OK);
    assert_null(disk);
    fclose(image);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_disk_open_rejects_blank_image,
                                        setup_sim_imd_fixture,
                                        teardown_sim_imd_fixture),
        cmocka_unit_test_setup_teardown(test_disk_open_rejects_corrupt_header,
                                        setup_sim_imd_fixture,
                                        teardown_sim_imd_fixture),
        cmocka_unit_test_setup_teardown(
            test_disk_open_reads_minimal_single_sector_image,
            setup_sim_imd_fixture, teardown_sim_imd_fixture),
        cmocka_unit_test_setup_teardown(
            test_disk_open_reads_compressed_sector_and_write_protects,
            setup_sim_imd_fixture, teardown_sim_imd_fixture),
        cmocka_unit_test_setup_teardown(
            test_sect_write_persists_deleted_address_mark,
            setup_sim_imd_fixture, teardown_sim_imd_fixture),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
