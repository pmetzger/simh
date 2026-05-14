#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"
#include "test_support.h"

#include "sim_tempfile.h"
#include "sim_types.h"
#include "system_defs.h"

/*
 * sim_load() reaches memory through the simulator board callbacks. These
 * fakes provide a flat memory image and record writes so loader tests can
 * detect overlong Intel HEX records before they corrupt arbitrary memory.
 */
static uint8_t test_memory[MAXMEMSIZE + 1];
static uint32_t test_write_count;
static int32_t test_write_limit = -1;

uint32_t saved_PC;
REG *sim_PC = NULL;
DEVICE *sim_devices[] = {NULL};
char sim_name[] = "Intel-MDS";
const char *sim_stop_messages[SCPE_BASE];
int32_t sim_emax = 1;

extern t_stat sim_load(FILE *fileref, const char *cptr, const char *fnam,
                       int flag);
uint8_t get_mbyte(uint16_t addr);
void put_mbyte(uint16_t addr, uint8_t val);

/*
 * Satisfy the simulator core's execution callback for loader-only tests.
 */
t_stat sim_instr(void)
{
    return SCPE_OK;
}

/*
 * Satisfy the simulator core's symbolic output callback for loader-only
 * tests, which never disassemble memory.
 */
t_stat fprint_sym(FILE *ofile, t_addr addr, t_value *val, UNIT *uptr, int32_t sw)
{
    (void)ofile;
    (void)addr;
    (void)val;
    (void)uptr;
    (void)sw;

    return SCPE_OK;
}

/*
 * Satisfy the simulator core's symbolic input callback for loader-only tests,
 * which never assemble memory.
 */
t_stat parse_sym(const char *cptr, t_addr addr, UNIT *uptr, t_value *val,
                 int32_t sw)
{
    (void)cptr;
    (void)addr;
    (void)uptr;
    (void)val;
    (void)sw;

    return SCPE_OK;
}

/*
 * Read one byte from the fake i8080 memory image.
 */
uint8_t get_mbyte(uint16_t addr)
{
    return test_memory[addr];
}

/*
 * Write one byte to fake memory and optionally fail if a test has limited the
 * number of expected writes.
 */
void put_mbyte(uint16_t addr, uint8_t val)
{
    if ((test_write_limit >= 0) &&
        (test_write_count >= (uint32_t)test_write_limit)) {
        fail_msg("sim_load wrote more bytes than the record declared");
    }
    test_memory[addr] = val;
    test_write_count++;
}

struct temp_stream {
    FILE *file;
    char path[256];
};

struct load_capture {
    FILE *file;
    const char *args;
    int flag;
    t_stat status;
};

/*
 * Close and remove a temporary stream created for a loader test. Tests call
 * this explicitly so failures do not leave simulator-owned FILE pointers live.
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
 * Open a portable read/write temporary stream using the simulator tempfile
 * helper instead of C tmpfile().
 */
static void open_temp_stream(struct temp_stream *stream)
{
    memset(stream, 0, sizeof(*stream));
    stream->file = sim_tempfile_open_stream(stream->path, sizeof(stream->path),
                                            "zimh-i8080-load-", ".tmp", "w+b");
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
 * Read a temporary stream into a NUL-terminated heap string for dump checks.
 */
static char *read_stream_text(struct temp_stream *stream)
{
    char *text;
    size_t size;

    assert_int_equal(simh_test_read_stream(stream->file, &text, &size), 0);
    (void)size;
    return text;
}

/*
 * Reset the fake 8080 memory image and simulator switch state before each
 * loader test.
 */
static int setup_i8080_load(void **state)
{
    (void)state;

    memset(test_memory, 0xEE, sizeof(test_memory));
    test_write_count = 0;
    test_write_limit = -1;
    sim_switches = 0;

    return 0;
}

/*
 * Adapter used by simh_test_capture_stdout() to capture sim_load() output.
 */
static void call_sim_load_for_capture(void *context)
{
    struct load_capture *capture = context;

    capture->status =
        sim_load(capture->file, capture->args, "test", capture->flag);
}

/*
 * Run sim_load(), capture its stdout text, and return both text and status.
 */
static char *capture_sim_load(FILE *file, const char *args, int flag,
                              t_stat *status)
{
    struct load_capture capture = {
        .file = file,
        .args = args,
        .flag = flag,
        .status = SCPE_OK,
    };
    char *text;
    size_t size;

    assert_int_equal(simh_test_capture_stdout(call_sim_load_for_capture,
                                              &capture, &text, &size),
                     0);
    (void)size;
    *status = capture.status;
    return text;
}

/*
 * Binary loads use the optional command address as the starting address and
 * must write exactly the bytes present in the input stream.
 */
static void test_binary_load_uses_start_and_exact_input_size(void **state)
{
    struct temp_stream stream;
    const uint8_t data[] = {0xAA, 0xBB, 0xCC};
    char *output;
    t_stat status;

    (void)state;

    open_temp_stream(&stream);
    write_stream(&stream, data, sizeof(data));

    output = capture_sim_load(stream.file, "0100", 0, &status);

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(test_write_count, 3);
    assert_int_equal(test_memory[0x0100], 0xAA);
    assert_int_equal(test_memory[0x0101], 0xBB);
    assert_int_equal(test_memory[0x0102], 0xCC);
    assert_int_equal(test_memory[0x0103], 0xEE);
    assert_non_null(strstr(output, "3 Bytes loaded at 0100"));

    free(output);
    remove_temp_stream(&stream);
}

/*
 * A binary load without an explicit address starts at the current program
 * counter, matching the simulator loader comment.
 */
static void test_binary_load_without_address_uses_saved_pc(void **state)
{
    struct temp_stream stream;
    const uint8_t data[] = {0x44, 0x55};
    char *output;
    t_stat status;

    (void)state;

    saved_PC = 0x0200;
    open_temp_stream(&stream);
    write_stream(&stream, data, sizeof(data));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(test_write_count, 2);
    assert_int_equal(test_memory[0x0200], 0x44);
    assert_int_equal(test_memory[0x0201], 0x55);
    assert_int_equal(test_memory[0x0000], 0xEE);
    assert_non_null(strstr(output, "2 Bytes loaded at 0200"));

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Address arguments must be complete hex tokens. Trailing junk after an
 * address should reject the command before any binary data is loaded.
 */
static void test_binary_load_rejects_address_trailing_garbage(void **state)
{
    struct temp_stream stream;
    const uint8_t data[] = {0xAA};
    char *output;
    t_stat status;

    (void)state;

    open_temp_stream(&stream);
    write_stream(&stream, data, sizeof(data));

    output = capture_sim_load(stream.file, "0100XYZ", 0, &status);

    assert_int_equal(status, SCPE_ARG);
    assert_int_equal(test_write_count, 0);
    assert_int_equal(test_memory[0x0100], 0xEE);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Address arguments are 16-bit values. Five hex digits would silently truncate
 * under the old scanf-based parser and must be rejected.
 */
static void test_binary_load_rejects_out_of_range_address(void **state)
{
    struct temp_stream stream;
    const uint8_t data[] = {0xAA};
    char *output;
    t_stat status;

    (void)state;

    open_temp_stream(&stream);
    write_stream(&stream, data, sizeof(data));

    output = capture_sim_load(stream.file, "10000", 0, &status);

    assert_int_equal(status, SCPE_ARG);
    assert_int_equal(test_write_count, 0);
    assert_int_equal(test_memory[0x0000], 0xEE);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Binary dumps use an inclusive address range. This protects the existing
 * non-HEX path while the Intel HEX path is repaired.
 */
static void test_binary_dump_uses_inclusive_range(void **state)
{
    struct temp_stream stream;
    uchar_t data[3];
    char *output;
    t_stat status;

    (void)state;

    test_memory[0x0100] = 0x11;
    test_memory[0x0101] = 0x22;
    test_memory[0x0102] = 0x33;
    open_temp_stream(&stream);

    output = capture_sim_load(stream.file, "0100 0102", 1, &status);
    rewind(stream.file);

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(fread(data, 1, sizeof(data), stream.file), sizeof(data));
    assert_memory_equal(data, ((uchar_t[]){0x11, 0x22, 0x33}), 3);
    assert_non_null(strstr(output, "3 Bytes dumped from 0100"));

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Dump commands require exactly two clean address arguments. Extra tokens
 * after the range are malformed command input.
 */
static void test_binary_dump_rejects_extra_address_argument(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    open_temp_stream(&stream);

    output = capture_sim_load(stream.file, "0100 0102 0103", 1, &status);

    assert_int_equal(status, SCPE_ARG);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Dump address parsing should reject trailing garbage instead of dumping a
 * partially parsed range.
 */
static void test_binary_dump_rejects_address_trailing_garbage(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    open_temp_stream(&stream);

    output = capture_sim_load(stream.file, "0100 0102x", 1, &status);

    assert_int_equal(status, SCPE_ARG);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Intel HEX EOF records terminate a successful load. A file containing only
 * EOF is empty but valid, and must not be rejected as an unknown record type.
 */
static void test_intel_hex_load_accepts_eof_record(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    open_temp_stream(&stream);
    write_stream(&stream, ":00000001FF\n", strlen(":00000001FF\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(test_write_count, 0);
    assert_non_null(strstr(output, "0 Bytes loaded"));

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Intel HEX load records declare their own byte count. The loader must write
 * exactly that many bytes and report the first data-record address.
 */
static void test_intel_hex_load_uses_record_count_and_address(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    test_write_limit = 3;
    open_temp_stream(&stream);
    write_stream(&stream, ":03012000010203D6\n:00000001FF\n",
                 strlen(":03012000010203D6\n:00000001FF\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(test_write_count, 3);
    assert_int_equal(test_memory[0x0120], 0x01);
    assert_int_equal(test_memory[0x0121], 0x02);
    assert_int_equal(test_memory[0x0122], 0x03);
    assert_int_equal(test_memory[0x0123], 0xEE);
    assert_non_null(strstr(output, "3 Bytes loaded at 0120"));

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Intel HEX checksums protect the target memory image. A bad checksum should
 * reject the load before writing any bytes from the bad record.
 */
static void test_intel_hex_load_rejects_bad_checksum_before_write(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    test_write_limit = 0;
    open_temp_stream(&stream);
    write_stream(&stream, ":03012000010203D7\n:00000001FF\n",
                 strlen(":03012000010203D7\n:00000001FF\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_CSUM);
    assert_int_equal(test_write_count, 0);
    assert_int_equal(test_memory[0x0120], 0xEE);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Intel HEX fields are fixed-width hexadecimal fields. A one-digit byte
 * count must be rejected instead of being accepted as a permissive scanf
 * conversion.
 */
static void test_intel_hex_load_rejects_short_count_field(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    test_write_limit = 0;
    open_temp_stream(&stream);
    write_stream(&stream, ":301200010203D6\n:00000001FF\n",
                 strlen(":301200010203D6\n:00000001FF\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_ARG);
    assert_int_equal(test_write_count, 0);
    assert_int_equal(test_memory[0x0120], 0xEE);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Declared Intel HEX byte counts must match the number of data byte fields
 * present before the checksum.
 */
static void test_intel_hex_load_rejects_short_data_record(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    test_write_limit = 0;
    open_temp_stream(&stream);
    write_stream(&stream, ":030120000102D9\n:00000001FF\n",
                 strlen(":030120000102D9\n:00000001FF\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_ARG);
    assert_int_equal(test_write_count, 0);
    assert_int_equal(test_memory[0x0120], 0xEE);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Lowercase hexadecimal input is still valid Intel HEX and should load the
 * same as uppercase input.
 */
static void test_intel_hex_load_accepts_lowercase_hex(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    open_temp_stream(&stream);
    write_stream(&stream, ":01010000aa54\n:00000001ff\n",
                 strlen(":01010000aa54\n:00000001ff\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_OK);
    assert_int_equal(test_write_count, 1);
    assert_int_equal(test_memory[0x0100], 0xAA);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * A load that reaches host EOF before an Intel HEX EOF record is incomplete
 * and should be rejected.
 */
static void test_intel_hex_load_requires_eof_record(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    open_temp_stream(&stream);
    write_stream(&stream, ":01010000AA54\n", strlen(":01010000AA54\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_ARG);
    assert_int_equal(test_write_count, 1);
    assert_int_equal(test_memory[0x0100], 0xAA);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * The Intel HEX checksum must be the end of the meaningful record. Extra
 * non-whitespace bytes after the checksum indicate malformed input.
 */
static void test_intel_hex_load_rejects_trailing_garbage(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    test_write_limit = 0;
    open_temp_stream(&stream);
    write_stream(&stream, ":03012000010203D6XX\n:00000001FF\n",
                 strlen(":03012000010203D6XX\n:00000001FF\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_ARG);
    assert_int_equal(test_write_count, 0);
    assert_int_equal(test_memory[0x0120], 0xEE);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * This 8080 loader supports only 16-bit data records and EOF records. Extended
 * address records are valid Intel HEX generally, but not useful for this
 * 64 KiB memory image.
 */
static void test_intel_hex_load_rejects_extended_address_records(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    open_temp_stream(&stream);
    write_stream(&stream, ":020000040001F9\n:00000001FF\n",
                 strlen(":020000040001F9\n:00000001FF\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_ARG);
    assert_int_equal(test_write_count, 0);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * Intel HEX data records may not wrap through address zero. A record that
 * crosses the 16-bit address limit is invalid for the i8080 memory image.
 */
static void test_intel_hex_load_rejects_address_wrap(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    test_write_limit = 0;
    open_temp_stream(&stream);
    write_stream(&stream, ":02FFFF000102FD\n:00000001FF\n",
                 strlen(":02FFFF000102FD\n:00000001FF\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_ARG);
    assert_int_equal(test_write_count, 0);
    assert_int_equal(test_memory[0xFFFF], 0xEE);
    assert_int_equal(test_memory[0x0000], 0xEE);

    free(output);
    remove_temp_stream(&stream);
}

/*
 * A 16-byte inclusive dump range should produce a 16-byte Intel HEX record,
 * not a 15-byte record containing 16 bytes of payload.
 */
static void test_intel_hex_dump_uses_full_record_length(void **state)
{
    struct temp_stream stream;
    char *text;
    t_stat status;

    (void)state;

    for (uint16_t i = 0; i < 16; ++i)
        test_memory[0x0100 + i] = (uint8_t)i;
    sim_switches = SWMASK('H');
    open_temp_stream(&stream);

    text = capture_sim_load(stream.file, "0100 010F", 1, &status);
    free(text);
    rewind(stream.file);
    text = read_stream_text(&stream);

    assert_int_equal(status, SCPE_OK);
    assert_string_equal(text, ":10010000000102030405060708090A0B0C0D0E0F77\n"
                              ":00000001FF\n");

    free(text);
    remove_temp_stream(&stream);
}

/*
 * Short Intel HEX dump ranges must use the actual remaining byte count in
 * both the record length field and checksum.
 */
static void test_intel_hex_dump_uses_short_record_length(void **state)
{
    struct temp_stream stream;
    char *text;
    t_stat status;

    (void)state;

    test_memory[0x0120] = 0xA0;
    test_memory[0x0121] = 0xB1;
    test_memory[0x0122] = 0xC2;
    sim_switches = SWMASK('H');
    open_temp_stream(&stream);

    text = capture_sim_load(stream.file, "0120 0122", 1, &status);
    free(text);
    rewind(stream.file);
    text = read_stream_text(&stream);

    assert_int_equal(status, SCPE_OK);
    assert_string_equal(text, ":03012000A0B1C2C9\n:00000001FF\n");

    free(text);
    remove_temp_stream(&stream);
}

/*
 * sim_load() should not emit ad hoc entry diagnostics on ordinary loads.
 * User-facing progress and summary text are enough.
 */
static void test_intel_hex_load_omits_entry_debug_diagnostics(void **state)
{
    struct temp_stream stream;
    char *output;
    t_stat status;

    (void)state;

    sim_switches = SWMASK('H');
    open_temp_stream(&stream);
    write_stream(&stream, ":01010000AA54\n:00000001FF\n",
                 strlen(":01010000AA54\n:00000001FF\n"));

    output = capture_sim_load(stream.file, "", 0, &status);

    assert_int_equal(status, SCPE_OK);
    assert_null(strstr(output, "sim_load cptr="));
    assert_null(strstr(output, "cnt="));
    assert_non_null(strstr(output, "1 Bytes loaded at 0100"));

    free(output);
    remove_temp_stream(&stream);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_binary_load_uses_start_and_exact_input_size,
                               setup_i8080_load),
        cmocka_unit_test_setup(test_binary_load_without_address_uses_saved_pc,
                               setup_i8080_load),
        cmocka_unit_test_setup(
            test_binary_load_rejects_address_trailing_garbage,
            setup_i8080_load),
        cmocka_unit_test_setup(test_binary_load_rejects_out_of_range_address,
                               setup_i8080_load),
        cmocka_unit_test_setup(test_binary_dump_uses_inclusive_range,
                               setup_i8080_load),
        cmocka_unit_test_setup(test_binary_dump_rejects_extra_address_argument,
                               setup_i8080_load),
        cmocka_unit_test_setup(
            test_binary_dump_rejects_address_trailing_garbage,
            setup_i8080_load),
        cmocka_unit_test_setup(test_intel_hex_load_accepts_eof_record,
                               setup_i8080_load),
        cmocka_unit_test_setup(
            test_intel_hex_load_uses_record_count_and_address,
            setup_i8080_load),
        cmocka_unit_test_setup(
            test_intel_hex_load_rejects_bad_checksum_before_write,
            setup_i8080_load),
        cmocka_unit_test_setup(test_intel_hex_load_rejects_short_count_field,
                               setup_i8080_load),
        cmocka_unit_test_setup(test_intel_hex_load_rejects_short_data_record,
                               setup_i8080_load),
        cmocka_unit_test_setup(test_intel_hex_load_accepts_lowercase_hex,
                               setup_i8080_load),
        cmocka_unit_test_setup(test_intel_hex_load_requires_eof_record,
                               setup_i8080_load),
        cmocka_unit_test_setup(test_intel_hex_load_rejects_trailing_garbage,
                               setup_i8080_load),
        cmocka_unit_test_setup(
            test_intel_hex_load_rejects_extended_address_records,
            setup_i8080_load),
        cmocka_unit_test_setup(test_intel_hex_load_rejects_address_wrap,
                               setup_i8080_load),
        cmocka_unit_test_setup(test_intel_hex_dump_uses_full_record_length,
                               setup_i8080_load),
        cmocka_unit_test_setup(test_intel_hex_dump_uses_short_record_length,
                               setup_i8080_load),
        cmocka_unit_test_setup(
            test_intel_hex_load_omits_entry_debug_diagnostics,
            setup_i8080_load),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
