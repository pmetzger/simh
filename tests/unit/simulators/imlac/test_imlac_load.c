#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"
#include "test_support.h"

#include "imlac_defs.h"
#include "sim_tempfile.h"

REG cpu_reg[1];
uint16_t memmask = 017777;

DEVICE cpu_dev;
DEVICE irq_dev;
DEVICE rom_dev;
DEVICE dp_dev;
DEVICE crt_dev;
DEVICE kbd_dev;
DEVICE tty_dev;
DEVICE ptr_dev;
DEVICE ptp_dev;
DEVICE sync_dev;
DEVICE bel_dev;

extern t_stat sim_load(FILE *fileref, const char *cptr, const char *fnam,
                       int flag);

struct temp_stream {
    FILE *file;
    char path[256];
};

struct load_capture {
    FILE *file;
    t_stat status;
};

/*
 * Satisfy the simulator core's execution callback for loader-only tests.
 */
t_stat sim_instr(void)
{
    return SCPE_OK;
}

/*
 * Close and remove a temporary stream created for an Imlac loader test.
 */
static void remove_temp_stream(struct temp_stream *stream)
{
    if (stream->file != NULL) {
        fclose(stream->file);
        stream->file = NULL;
    }
    if (stream->path[0] != '\0') {
        remove(stream->path);
        stream->path[0] = '\0';
    }
}

/*
 * Open a portable read/write temporary stream for an Imlac loader test.
 */
static void open_temp_stream(struct temp_stream *stream)
{
    memset(stream, 0, sizeof(*stream));
    stream->file = sim_tempfile_open_stream(stream->path, sizeof(stream->path),
                                            "zimh-imlac-load-", ".tmp", "w+b");
    assert_non_null(stream->file);
}

/*
 * Replace the temporary stream contents and rewind it for sim_load().
 */
static void write_stream(struct temp_stream *stream, const void *data,
                         size_t size)
{
    assert_int_equal(fwrite(data, 1, size, stream->file), size);
    assert_int_equal(fflush(stream->file), 0);
    rewind(stream->file);
}

/*
 * Reset Imlac loader state that can affect address masking or verbosity.
 */
static int setup_imlac_load(void **state)
{
    (void)state;

    memset(M, 0, 040000 * sizeof(M[0]));
    memmask = 017777;
    sim_switches = 0;

    return 0;
}

/*
 * Adapter used by simh_test_capture_stdout() to capture sim_load() output.
 */
static void call_load_ptr_for_capture(void *context)
{
    struct load_capture *capture = context;

    sim_switches = SWMASK('P') | SWMASK('V');
    capture->status = sim_load(capture->file, "", "test.ptp", 0);
}

/*
 * Run a paper-tape load, capture stdout text, and return both text and
 * status.
 */
static char *capture_load_ptr(FILE *file, t_stat *status)
{
    struct load_capture capture = {
        .file = file,
        .status = SCPE_OK,
    };
    char *text;
    size_t size;

    assert_int_equal(simh_test_capture_stdout(call_load_ptr_for_capture,
                                              &capture, &text, &size),
                     0);
    (void)size;
    *status = capture.status;
    return text;
}

/*
 * A verbose paper-tape load should report each loaded block, matching the
 * existing special-TTY loader's address/count output.
 */
static void test_load_ptr_verbose_reports_loaded_block(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;
    const uint8_t tape[] = {
        002,  [124] = 002, 002,  0234, 000,  0111, 000,
        0222, 000,         0333, 001,  0377, 0377,
    };

    (void)state;

    open_temp_stream(&stream);
    write_stream(&stream, tape, sizeof(tape));

    output = capture_load_ptr(stream.file, &status);

    assert_int_equal(status, SCPE_OK);
    assert_string_equal(output, "%SIM-INFO: Address 001234: 2 words.\n");
    assert_int_equal(M[01234], 0111);
    assert_int_equal(M[01235], 0222);

    free(output);
    remove_temp_stream(&stream);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_load_ptr_verbose_reports_loaded_block,
                               setup_imlac_load),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
