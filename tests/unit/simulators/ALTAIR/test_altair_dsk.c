#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "altair_dsk_internal.h"
#include "sim_tempfile.h"
#include "sim_types.h"

#define TEST_DSK_SECTSIZE 137
#define TEST_DSK_SECT 32
#define TEST_DSK_TRACSIZE (TEST_DSK_SECTSIZE * TEST_DSK_SECT)

extern DEVICE dsk_dev;
extern UNIT dsk_unit[];
extern int32_t cur_disk;
extern int32_t cur_track[];
extern int32_t cur_sect[];
extern int32_t cur_byte[];
extern int32_t cur_flags[];
extern char dskbuf[];
extern int32_t dirty;
extern UNIT *dptr;

int32_t PCX;

struct altair_dsk_test_state {
    FILE *file;
    char path[512];
};

static long sector_offset(int32_t track, int32_t sector)
{
    return (track * TEST_DSK_TRACSIZE) + (sector * TEST_DSK_SECTSIZE);
}

static int test_seek_calls;
static int test_read_calls;
static int test_write_calls;
static long test_last_seek_offset;
static size_t test_last_read_size;
static size_t test_last_write_size;

static void reset_io_hook_counters(void)
{
    test_seek_calls = 0;
    test_read_calls = 0;
    test_write_calls = 0;
    test_last_seek_offset = -1;
    test_last_read_size = 0;
    test_last_write_size = 0;
}

static int test_seek_hook(FILE *file, long offset, int whence)
{
    ++test_seek_calls;
    test_last_seek_offset = offset;
    return fseek(file, offset, whence);
}

static size_t test_read_hook(void *ptr, size_t size, size_t count, FILE *file)
{
    ++test_read_calls;
    test_last_read_size = size * count;
    return fread(ptr, size, count, file);
}

static size_t test_write_hook(const void *ptr, size_t size, size_t count,
                              FILE *file)
{
    ++test_write_calls;
    test_last_write_size = size * count;
    return fwrite(ptr, size, count, file);
}

static int test_seek_fail_hook(FILE *file, long offset, int whence)
{
    (void)file;
    (void)whence;

    ++test_seek_calls;
    test_last_seek_offset = offset;
    return -1;
}

static size_t test_read_fail_hook(void *ptr, size_t size, size_t count,
                                  FILE *file)
{
    (void)ptr;
    (void)file;

    ++test_read_calls;
    test_last_read_size = size * count;
    return 0;
}

static size_t test_write_fail_hook(const void *ptr, size_t size, size_t count,
                                   FILE *file)
{
    (void)ptr;
    (void)file;

    ++test_write_calls;
    test_last_write_size = size * count;
    return 0;
}

/*
 * Start each test from a detached, reset controller so direct state checks are
 * not affected by a previous test's selected drive, dirty buffer, or FILE.
 */
static int setup_disk(void **state)
{
    struct altair_dsk_test_state *test_state = calloc(1, sizeof(*test_state));

    assert_non_null(test_state);
    *state = test_state;

    for (int i = 0; i < 8; ++i) {
        dsk_unit[i].fileref = NULL;
        dsk_unit[i].flags &= ~UNIT_ATT;
    }

    for (int i = 0; i < 9; ++i) {
        cur_track[i] = (i == 8) ? 0377 : 0;
        cur_sect[i] = (i == 8) ? 0377 : 0;
        cur_byte[i] = (i == 8) ? 0377 : 0;
        cur_flags[i] = 0;
    }

    memset(dskbuf, 0, 138);
    dirty = 0;
    dptr = NULL;
    reset_io_hook_counters();
    altair_dsk_reset_io_hooks();
    assert_int_equal(dsk_reset(&dsk_dev), SCPE_OK);

    return 0;
}

static void attach_temp_image(void **state, int32_t unit)
{
    struct altair_dsk_test_state *test_state = *state;
    FILE *file;

    file = sim_tempfile_open_stream(test_state->path, sizeof(test_state->path),
                                    "zimh-altair-dsk-", ".dsk", "w+b");
    assert_non_null(file);
    test_state->file = file;
    dsk_unit[unit].fileref = file;
    dsk_unit[unit].flags |= UNIT_ATT;
}

static void fill_image_sector(FILE *file, int32_t track, int32_t sector, int value)
{
    uchar_t data[TEST_DSK_SECTSIZE];

    memset(data, value, sizeof(data));
    assert_int_equal(fseek(file, sector_offset(track, sector), SEEK_SET), 0);
    assert_int_equal(fwrite(data, 1, sizeof(data), file), sizeof(data));
    assert_int_equal(fflush(file), 0);
}

static void read_image_sector(FILE *file, int32_t track, int32_t sector,
                              uchar_t *data)
{
    assert_int_equal(fseek(file, sector_offset(track, sector), SEEK_SET), 0);
    assert_int_equal(fread(data, 1, TEST_DSK_SECTSIZE, file),
                     TEST_DSK_SECTSIZE);
}

static int teardown_disk(void **state)
{
    struct altair_dsk_test_state *test_state = *state;

    for (int i = 0; i < 8; ++i) {
        dsk_unit[i].fileref = NULL;
        dsk_unit[i].flags &= ~UNIT_ATT;
    }

    if (test_state != NULL) {
        if (test_state->file != NULL)
            fclose(test_state->file);
        if (test_state->path[0] != '\0')
            remove(test_state->path);
        free(test_state);
    }
    *state = NULL;
    altair_dsk_reset_io_hooks();

    return 0;
}

/*
 * Resetting the disk controller selects drive 0 and leaves its status clear;
 * the Altair hardware presents status bits inverted on reads.
 */
static void test_reset_selects_drive_zero_with_inverted_status(void **state)
{
    (void)state;

    assert_int_equal(cur_disk, 0);
    assert_int_equal(cur_flags[0], 0);
    assert_int_equal(dsk10(0, 0), 0xff);
}

/*
 * OUT 10 selects a drive and enables it. The simulator tracks status in
 * positive logic internally, but IN 10 returns the hardware's inverted bits.
 */
static void
test_select_drive_enables_status_and_reports_track_zero(void **state)
{
    (void)state;

    cur_track[3] = 0;

    assert_int_equal(dsk10(1, 3), 0);

    assert_int_equal(cur_disk, 3);
    assert_int_equal(cur_flags[3], 0x5a);
    assert_int_equal(cur_sect[3], 0377);
    assert_int_equal(cur_byte[3], 0377);
    assert_int_equal(dsk10(0, 0), 0xa5);
}

/*
 * OUT 10 with the controller-clear bit disables the selected drive and resets
 * the per-drive sector and byte counters.
 */
static void test_controller_clear_disables_selected_drive(void **state)
{
    (void)state;

    assert_int_equal(dsk10(1, 4), 0);
    cur_flags[4] = 0xff;
    cur_sect[4] = 12;
    cur_byte[4] = 34;

    assert_int_equal(dsk10(1, 0x84), 0);

    assert_int_equal(cur_disk, 4);
    assert_int_equal(cur_flags[4], 0);
    assert_int_equal(cur_sect[4], 0377);
    assert_int_equal(cur_byte[4], 0377);
}

/*
 * Head-load and head-unload commands drive the status bits used by the
 * controller status port and reset the byte/sector cursor when unloading.
 */
static void test_head_load_and_unload_update_status_bits(void **state)
{
    (void)state;

    assert_int_equal(dsk10(1, 0), 0);

    assert_int_equal(dsk11(1, 0x04), 0);
    assert_true((cur_flags[0] & 0x04) != 0);
    assert_true((cur_flags[0] & 0x80) != 0);

    cur_sect[0] = 7;
    cur_byte[0] = 9;
    assert_int_equal(dsk11(1, 0x08), 0);

    assert_false((cur_flags[0] & 0x04) != 0);
    assert_false((cur_flags[0] & 0x80) != 0);
    assert_int_equal(cur_sect[0], 0377);
    assert_int_equal(cur_byte[0], 0377);
}

/*
 * Reading port 11 while the head is loaded advances through the 32 hard
 * sectors and returns the sector number with the sector-true bit asserted.
 */
static void test_sector_status_advances_and_wraps_when_head_loaded(void **state)
{
    (void)state;

    assert_int_equal(dsk10(1, 0), 0);
    assert_int_equal(dsk11(1, 0x04), 0);
    cur_sect[0] = 30;

    assert_int_equal(dsk11(0, 0), 0xfe);
    assert_int_equal(cur_sect[0], 31);
    assert_int_equal(cur_byte[0], 0377);

    assert_int_equal(dsk11(0, 0), 0xc0);
    assert_int_equal(cur_sect[0], 0);
}

/*
 * With the head unloaded, port 11 returns zero and does not advance the
 * sector cursor or reset the data-byte cursor.
 */
static void
test_sector_status_without_head_loaded_does_not_advance(void **state)
{
    (void)state;

    assert_int_equal(dsk10(1, 0), 0);
    cur_sect[0] = 5;
    cur_byte[0] = 9;

    assert_int_equal(dsk11(0, 0), 0);

    assert_int_equal(cur_sect[0], 5);
    assert_int_equal(cur_byte[0], 9);
}

/*
 * Step commands clamp the simulated head to the drive's 0..76 track range and
 * reset the sector and byte cursors after movement.
 */
static void test_step_commands_clamp_track_and_reset_position(void **state)
{
    (void)state;

    assert_int_equal(dsk10(1, 0), 0);

    cur_track[0] = 76;
    assert_int_equal(dsk11(1, 0x01), 0);
    assert_int_equal(cur_track[0], 76);
    assert_int_equal(cur_sect[0], 0377);
    assert_int_equal(cur_byte[0], 0377);

    cur_track[0] = 0;
    cur_flags[0] &= ~0x40;
    assert_int_equal(dsk11(1, 0x02), 0);
    assert_int_equal(cur_track[0], 0);
    assert_true((cur_flags[0] & 0x40) != 0);
    assert_int_equal(cur_sect[0], 0377);
    assert_int_equal(cur_byte[0], 0377);
}

/*
 * If no valid drive is selected, drive-function writes are ignored. This
 * covers the sentinel drive slot used by the controller's initial state.
 */
static void test_drive_function_is_ignored_without_selected_drive(void **state)
{
    (void)state;

    cur_disk = 8;
    cur_flags[8] = 0;
    cur_byte[8] = 0377;

    assert_int_equal(dsk11(1, 0x80), 0);

    assert_int_equal(cur_flags[8], 0);
    assert_int_equal(cur_byte[8], 0377);
}

/*
 * A read from port 12 with no attached image returns zero rather than touching
 * host I/O. This is the current unattached-drive behavior that later error
 * handling must not accidentally break.
 */
static void test_data_read_without_attached_image_returns_zero(void **state)
{
    (void)state;

    assert_int_equal(dsk10(1, 0), 0);
    cur_byte[0] = 0377;

    assert_int_equal(dsk12(0, 0), 0);
    assert_int_equal(cur_byte[0], 0377);
}

/*
 * The first read of a sector loads 137 bytes from the selected track/sector
 * and returns byte zero. Later reads come from the in-memory sector buffer.
 */
static void
test_data_read_loads_selected_sector_then_streams_buffer(void **state)
{
    struct altair_dsk_test_state *test_state = *state;
    FILE *file;
    uchar_t sector[TEST_DSK_SECTSIZE];

    attach_temp_image(state, 0);
    file = test_state->file;
    for (int i = 0; i < TEST_DSK_SECTSIZE; ++i)
        sector[i] = (uchar_t)(0x40 + i);

    assert_int_equal(fseek(file, sector_offset(2, 5), SEEK_SET), 0);
    assert_int_equal(fwrite(sector, 1, sizeof(sector), file), sizeof(sector));
    assert_int_equal(fflush(file), 0);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 2;
    cur_sect[0] = 5;
    cur_byte[0] = 0377;

    assert_int_equal(dsk12(0, 0), sector[0]);
    assert_int_equal(cur_byte[0], 1);
    assert_int_equal(dsk12(0, 0), sector[1]);
    assert_int_equal(cur_byte[0], 2);
}

/*
 * The private I/O hook table is the seam used to test host stdio failures
 * deterministically; reads must go through the installed seek/read hooks.
 */
static void test_data_read_uses_installed_io_hooks(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_hook,
        test_read_hook,
        NULL,
    };
    struct altair_dsk_test_state *test_state = *state;
    FILE *file;
    uchar_t sector[TEST_DSK_SECTSIZE];

    attach_temp_image(state, 0);
    file = test_state->file;
    for (int i = 0; i < TEST_DSK_SECTSIZE; ++i)
        sector[i] = (uchar_t)(0x20 + i);

    assert_int_equal(fseek(file, sector_offset(3, 6), SEEK_SET), 0);
    assert_int_equal(fwrite(sector, 1, sizeof(sector), file), sizeof(sector));
    assert_int_equal(fflush(file), 0);

    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 3;
    cur_sect[0] = 6;
    cur_byte[0] = 0377;

    assert_int_equal(dsk12(0, 0), sector[0]);
    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_read_calls, 1);
    assert_int_equal(test_write_calls, 0);
    assert_int_equal(test_last_seek_offset, sector_offset(3, 6));
    assert_int_equal(test_last_read_size, TEST_DSK_SECTSIZE);
}

/*
 * Resetting the private I/O hooks must restore direct stdio calls so test
 * hooks cannot leak into later tests or ordinary simulator execution.
 */
static void test_data_read_reset_restores_default_io_hooks(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_hook,
        test_read_hook,
        NULL,
    };
    struct altair_dsk_test_state *test_state = *state;
    FILE *file;
    uchar_t sector[TEST_DSK_SECTSIZE];

    attach_temp_image(state, 0);
    file = test_state->file;
    memset(sector, 0x5a, sizeof(sector));

    assert_int_equal(fseek(file, sector_offset(1, 2), SEEK_SET), 0);
    assert_int_equal(fwrite(sector, 1, sizeof(sector), file), sizeof(sector));
    assert_int_equal(fflush(file), 0);

    altair_dsk_set_io_hooks(&hooks);
    altair_dsk_reset_io_hooks();

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 1;
    cur_sect[0] = 2;
    cur_byte[0] = 0377;

    assert_int_equal(dsk12(0, 0), 0x5a);
    assert_int_equal(test_seek_calls, 0);
    assert_int_equal(test_read_calls, 0);
}

/*
 * If the host seek fails before a sector read, the controller should expose a
 * failed byte value instead of stale data from an earlier buffer load.
 */
static void
test_data_read_seek_failure_returns_zero_without_stale_data(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_fail_hook,
        test_read_hook,
        NULL,
    };

    attach_temp_image(state, 0);
    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 2;
    cur_sect[0] = 7;
    cur_byte[0] = 0377;
    dskbuf[0] = 0x7e;

    assert_int_equal(dsk12(0, 0), 0);

    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_read_calls, 0);
    assert_int_equal(test_last_seek_offset, sector_offset(2, 7));
    assert_int_equal(cur_byte[0], 0377);
    assert_int_equal(dskbuf[0], 0);
}

/*
 * A short or failed host read must not make the sector look loaded. Returning
 * zero mirrors the unattached-drive read behavior and avoids leaking stale
 * buffered bytes to the emulated machine.
 */
static void
test_data_read_short_read_returns_zero_without_stale_data(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_hook,
        test_read_fail_hook,
        NULL,
    };

    attach_temp_image(state, 0);
    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 1;
    cur_sect[0] = 9;
    cur_byte[0] = 0377;
    dskbuf[0] = 0x6d;

    assert_int_equal(dsk12(0, 0), 0);

    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_read_calls, 1);
    assert_int_equal(test_last_seek_offset, sector_offset(1, 9));
    assert_int_equal(test_last_read_size, TEST_DSK_SECTSIZE);
    assert_int_equal(cur_byte[0], 0377);
    assert_int_equal(dskbuf[0], 0);
}

/*
 * Starting a write sequence sets the write-enable status and positions the
 * byte cursor at the beginning of the controller's sector buffer.
 */
static void test_write_sequence_start_enables_port_12_capture(void **state)
{
    (void)state;

    assert_int_equal(dsk10(1, 0), 0);

    assert_int_equal(dsk11(1, 0x80), 0);

    assert_int_equal(cur_byte[0], 0);
    assert_true((cur_flags[0] & 0x01) != 0);
}

/*
 * Port 12 write data is buffered until the legacy controller model receives
 * the byte after index 136. That extra OUT causes writebuf() to persist the
 * first 137 sector bytes and complete the write-enable sequence.
 */
static void test_data_write_flushes_first_137_bytes_on_completion(void **state)
{
    struct altair_dsk_test_state *test_state = *state;
    FILE *file;
    uchar_t sector[TEST_DSK_SECTSIZE];

    attach_temp_image(state, 0);
    file = test_state->file;

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 1;
    cur_sect[0] = 3;
    assert_int_equal(dsk11(1, 0x80), 0);

    for (int i = 0; i < TEST_DSK_SECTSIZE; ++i)
        assert_int_equal(dsk12(1, i), 0);

    assert_int_equal(dirty, 1);
    assert_int_equal(cur_byte[0], TEST_DSK_SECTSIZE);
    assert_true((cur_flags[0] & 0x01) != 0);

    assert_int_equal(dsk12(1, 0xee), 0);

    assert_int_equal(dirty, 0);
    assert_int_equal(cur_byte[0], 0377);
    assert_false((cur_flags[0] & 0x01) != 0);

    assert_int_equal(fseek(file, sector_offset(1, 3), SEEK_SET), 0);
    assert_int_equal(fread(sector, 1, sizeof(sector), file), sizeof(sector));
    for (int i = 0; i < TEST_DSK_SECTSIZE; ++i)
        assert_int_equal(sector[i], (uchar_t)i);
}

/*
 * Write completion must also use the private I/O hook table; later failure
 * tests rely on this seam to force seek/write errors portably.
 */
static void test_data_write_uses_installed_io_hooks(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_hook,
        NULL,
        test_write_hook,
    };

    attach_temp_image(state, 0);
    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 2;
    cur_sect[0] = 4;
    assert_int_equal(dsk11(1, 0x80), 0);

    for (int i = 0; i < TEST_DSK_SECTSIZE; ++i)
        assert_int_equal(dsk12(1, i), 0);

    assert_int_equal(test_seek_calls, 0);
    assert_int_equal(test_write_calls, 0);

    assert_int_equal(dsk12(1, 0xee), 0);

    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_read_calls, 0);
    assert_int_equal(test_write_calls, 1);
    assert_int_equal(test_last_seek_offset, sector_offset(2, 4));
    assert_int_equal(test_last_write_size, TEST_DSK_SECTSIZE);
}

/*
 * A failed seek while flushing a dirty sector must not discard the pending
 * write. The controller should leave write-enable and dirty state set so a
 * later flush can still retry the buffered sector.
 */
static void test_data_write_seek_failure_leaves_write_pending(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_fail_hook,
        NULL,
        test_write_hook,
    };

    attach_temp_image(state, 0);
    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 3;
    cur_sect[0] = 8;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0xaa), 0);
    assert_int_equal(dsk12(1, 0xbb), 0);

    writebuf();

    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_write_calls, 0);
    assert_int_equal(test_last_seek_offset, sector_offset(3, 8));
    assert_int_equal(dirty, 1);
    assert_true((cur_flags[0] & 0x01) != 0);
    assert_int_equal(cur_byte[0], 2);
}

/*
 * A short or failed sector write is also an incomplete flush. The dirty
 * buffer and write-enable state should remain available for retry.
 */
static void test_data_write_short_write_leaves_write_pending(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_hook,
        NULL,
        test_write_fail_hook,
    };

    attach_temp_image(state, 0);
    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 5;
    cur_sect[0] = 10;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0xcc), 0);
    assert_int_equal(dsk12(1, 0xdd), 0);

    writebuf();

    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_write_calls, 1);
    assert_int_equal(test_last_seek_offset, sector_offset(5, 10));
    assert_int_equal(test_last_write_size, TEST_DSK_SECTSIZE);
    assert_int_equal(dirty, 1);
    assert_true((cur_flags[0] & 0x01) != 0);
    assert_int_equal(cur_byte[0], 2);
}

/*
 * Step-in commands must flush the dirty sector at the old track before
 * changing the simulated head position.
 */
static void test_step_in_flushes_dirty_sector_before_moving_track(void **state)
{
    struct altair_dsk_test_state *test_state = *state;
    FILE *file;
    uchar_t sector[TEST_DSK_SECTSIZE];

    attach_temp_image(state, 0);
    file = test_state->file;
    fill_image_sector(file, 2, 4, 0x55);
    fill_image_sector(file, 3, 4, 0x66);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 2;
    cur_sect[0] = 4;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0xa1), 0);
    assert_int_equal(dsk12(1, 0xa2), 0);

    assert_int_equal(dsk11(1, 0x01), 0);

    assert_int_equal(cur_track[0], 3);
    assert_int_equal(dirty, 0);
    assert_int_equal(cur_sect[0], 0377);
    assert_int_equal(cur_byte[0], 0377);
    assert_false((cur_flags[0] & 0x01) != 0);

    read_image_sector(file, 2, 4, sector);
    assert_int_equal(sector[0], 0xa1);
    assert_int_equal(sector[1], 0xa2);
    for (int i = 2; i < TEST_DSK_SECTSIZE; ++i)
        assert_int_equal(sector[i], 0);

    read_image_sector(file, 3, 4, sector);
    for (int i = 0; i < TEST_DSK_SECTSIZE; ++i)
        assert_int_equal(sector[i], 0x66);
}

/*
 * Step-out commands have the same ordering requirement: flush the old track,
 * then move the head.
 */
static void test_step_out_flushes_dirty_sector_before_moving_track(void **state)
{
    struct altair_dsk_test_state *test_state = *state;
    FILE *file;
    uchar_t sector[TEST_DSK_SECTSIZE];

    attach_temp_image(state, 0);
    file = test_state->file;
    fill_image_sector(file, 2, 5, 0x55);
    fill_image_sector(file, 1, 5, 0x66);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 2;
    cur_sect[0] = 5;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0xb1), 0);
    assert_int_equal(dsk12(1, 0xb2), 0);

    assert_int_equal(dsk11(1, 0x02), 0);

    assert_int_equal(cur_track[0], 1);
    assert_int_equal(dirty, 0);
    assert_int_equal(cur_sect[0], 0377);
    assert_int_equal(cur_byte[0], 0377);
    assert_false((cur_flags[0] & 0x01) != 0);

    read_image_sector(file, 2, 5, sector);
    assert_int_equal(sector[0], 0xb1);
    assert_int_equal(sector[1], 0xb2);
    for (int i = 2; i < TEST_DSK_SECTSIZE; ++i)
        assert_int_equal(sector[i], 0);

    read_image_sector(file, 1, 5, sector);
    for (int i = 0; i < TEST_DSK_SECTSIZE; ++i)
        assert_int_equal(sector[i], 0x66);
}

/*
 * If the dirty-sector flush fails, a step-in command must leave the head at
 * the old track so the pending write can be retried at the correct location.
 */
static void test_step_in_flush_failure_does_not_move_track(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_fail_hook,
        NULL,
        test_write_hook,
    };

    attach_temp_image(state, 0);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 2;
    cur_sect[0] = 6;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0xc1), 0);
    assert_int_equal(dsk12(1, 0xc2), 0);

    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk11(1, 0x01), 0);

    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_write_calls, 0);
    assert_int_equal(cur_track[0], 2);
    assert_int_equal(cur_sect[0], 6);
    assert_int_equal(dirty, 1);
    assert_true((cur_flags[0] & 0x01) != 0);
    assert_int_equal(cur_byte[0], 2);
}

/*
 * Failed step-out flushes are also non-movement events; otherwise the dirty
 * buffer would be associated with the wrong track.
 */
static void test_step_out_flush_failure_does_not_move_track(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_fail_hook,
        NULL,
        test_write_hook,
    };

    attach_temp_image(state, 0);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 2;
    cur_sect[0] = 7;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0xd1), 0);
    assert_int_equal(dsk12(1, 0xd2), 0);

    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk11(1, 0x02), 0);

    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_write_calls, 0);
    assert_int_equal(cur_track[0], 2);
    assert_int_equal(cur_sect[0], 7);
    assert_int_equal(dirty, 1);
    assert_true((cur_flags[0] & 0x01) != 0);
    assert_int_equal(cur_byte[0], 2);
}

/*
 * Selecting another drive first flushes a dirty sector. If that flush fails,
 * the controller must not switch cur_disk and orphan the pending write.
 */
static void
test_select_drive_flush_failure_leaves_current_drive_selected(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_fail_hook,
        NULL,
        test_write_hook,
    };

    attach_temp_image(state, 0);

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 0;
    cur_sect[0] = 4;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0x11), 0);
    assert_int_equal(dsk12(1, 0x22), 0);

    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk10(1, 1), 0);

    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_write_calls, 0);
    assert_int_equal(cur_disk, 0);
    assert_int_equal(dirty, 1);
    assert_true((cur_flags[0] & 0x01) != 0);
    assert_int_equal(cur_byte[0], 2);
}

/*
 * Reading sector status also flushes dirty data first. A failed flush should
 * return no sector status and leave the pending write position intact.
 */
static void
test_sector_status_flush_failure_does_not_advance_sector(void **state)
{
    static const ALTAIR_DSK_IO_HOOKS hooks = {
        test_seek_fail_hook,
        NULL,
        test_write_hook,
    };

    attach_temp_image(state, 0);

    assert_int_equal(dsk10(1, 0), 0);
    cur_sect[0] = 6;
    cur_flags[0] |= 0x04;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0x33), 0);
    assert_int_equal(dsk12(1, 0x44), 0);

    altair_dsk_set_io_hooks(&hooks);

    assert_int_equal(dsk11(0, 0), 0);

    assert_int_equal(test_seek_calls, 1);
    assert_int_equal(test_write_calls, 0);
    assert_int_equal(cur_sect[0], 6);
    assert_int_equal(dirty, 1);
    assert_true((cur_flags[0] & 0x01) != 0);
    assert_int_equal(cur_byte[0], 2);
}

/*
 * Selecting another drive flushes a dirty sector before changing cur_disk.
 * Later I/O-error handling must preserve this ordering on successful writes.
 */
static void test_selecting_another_drive_flushes_dirty_sector(void **state)
{
    struct altair_dsk_test_state *test_state = *state;
    FILE *file;
    uchar_t sector[TEST_DSK_SECTSIZE];

    attach_temp_image(state, 0);
    file = test_state->file;

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 0;
    cur_sect[0] = 1;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0x12), 0);
    assert_int_equal(dsk12(1, 0x34), 0);
    assert_int_equal(dirty, 1);

    assert_int_equal(dsk10(1, 1), 0);

    assert_int_equal(dirty, 0);
    assert_int_equal(cur_disk, 1);

    assert_int_equal(fseek(file, sector_offset(0, 1), SEEK_SET), 0);
    assert_int_equal(fread(sector, 1, sizeof(sector), file), sizeof(sector));
    assert_int_equal(sector[0], 0x12);
    assert_int_equal(sector[1], 0x34);
    for (int i = 2; i < TEST_DSK_SECTSIZE; ++i)
        assert_int_equal(sector[i], 0);
}

/*
 * writebuf() null-fills the unwritten tail of a sector before persisting it;
 * this preserves the current partial-sector completion behavior.
 */
static void test_writebuf_zero_fills_unwritten_sector_tail(void **state)
{
    struct altair_dsk_test_state *test_state = *state;
    FILE *file;
    uchar_t sector[TEST_DSK_SECTSIZE];

    attach_temp_image(state, 0);
    file = test_state->file;

    assert_int_equal(dsk10(1, 0), 0);
    cur_track[0] = 4;
    cur_sect[0] = 2;
    assert_int_equal(dsk11(1, 0x80), 0);
    assert_int_equal(dsk12(1, 0xaa), 0);
    assert_int_equal(dsk12(1, 0xbb), 0);

    writebuf();

    assert_int_equal(fseek(file, sector_offset(4, 2), SEEK_SET), 0);
    assert_int_equal(fread(sector, 1, sizeof(sector), file), sizeof(sector));
    assert_int_equal(sector[0], 0xaa);
    assert_int_equal(sector[1], 0xbb);
    for (int i = 2; i < TEST_DSK_SECTSIZE; ++i)
        assert_int_equal(sector[i], 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_reset_selects_drive_zero_with_inverted_status, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_select_drive_enables_status_and_reports_track_zero, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_controller_clear_disables_selected_drive, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_head_load_and_unload_update_status_bits, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_sector_status_advances_and_wraps_when_head_loaded, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_sector_status_without_head_loaded_does_not_advance, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_step_commands_clamp_track_and_reset_position, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_drive_function_is_ignored_without_selected_drive, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_data_read_without_attached_image_returns_zero, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_data_read_loads_selected_sector_then_streams_buffer,
            setup_disk, teardown_disk),
        cmocka_unit_test_setup_teardown(test_data_read_uses_installed_io_hooks,
                                        setup_disk, teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_data_read_reset_restores_default_io_hooks, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_data_read_seek_failure_returns_zero_without_stale_data,
            setup_disk, teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_data_read_short_read_returns_zero_without_stale_data,
            setup_disk, teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_write_sequence_start_enables_port_12_capture, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_data_write_flushes_first_137_bytes_on_completion, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(test_data_write_uses_installed_io_hooks,
                                        setup_disk, teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_data_write_seek_failure_leaves_write_pending, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_data_write_short_write_leaves_write_pending, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_step_in_flushes_dirty_sector_before_moving_track, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_step_out_flushes_dirty_sector_before_moving_track, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_step_in_flush_failure_does_not_move_track, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_step_out_flush_failure_does_not_move_track, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_select_drive_flush_failure_leaves_current_drive_selected,
            setup_disk, teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_sector_status_flush_failure_does_not_advance_sector,
            setup_disk, teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_selecting_another_drive_flushes_dirty_sector, setup_disk,
            teardown_disk),
        cmocka_unit_test_setup_teardown(
            test_writebuf_zero_fills_unwritten_sector_tail, setup_disk,
            teardown_disk),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
