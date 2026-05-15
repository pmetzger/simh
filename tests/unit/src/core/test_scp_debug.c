#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "sim_tempfile.h"
#include "test_support.h"

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

static size_t count_text_occurrences(const char *text, const char *needle)
{
    size_t count = 0;
    size_t needle_len = strlen(needle);

    for (const char *found = strstr(text, needle); found != NULL;
         found = strstr(found + needle_len, needle)) {
        ++count;
    }

    return count;
}

static void assert_text_ends_with(const char *text, const char *suffix)
{
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);

    assert_true(text_len >= suffix_len);
    assert_string_equal(text + text_len - suffix_len, suffix);
}

/*
 * Partial debug writes leave SCP's internal debug state unterminated.  Emit a
 * discarded newline so each test starts and ends with terminated debug state.
 */
static void reset_scp_debug_termination(void)
{
    _sim_debug_device(SCP_LOG_TESTING, &sim_scp_dev, "\n");
    sim_flush_buffered_files();
}

static char *capture_scp_debug_output(void (*writer)(void))
{
    FILE *stream;
    FILE *reset_stream;
    FILE *saved_sim_deb;
    int32_t saved_sim_deb_switches;
    uint32_t saved_scp_debug;
    char *output;
    size_t output_size;
    int reset_close_status;
    int read_status;
    int close_status;

    stream = sim_tmpfile();
    assert_non_null(stream);
    reset_stream = sim_tmpfile();
    assert_non_null(reset_stream);

    saved_sim_deb = sim_deb;
    saved_sim_deb_switches = sim_deb_switches;
    saved_scp_debug = sim_scp_dev.dctrl;

    sim_deb = reset_stream;
    sim_deb_switches = 0;
    sim_scp_dev.dctrl = SCP_LOG_TESTING;

    reset_scp_debug_termination();
    reset_close_status = fclose(reset_stream);

    sim_deb = stream;
    writer();
    sim_flush_buffered_files();

    read_status = simh_test_read_stream(stream, &output, &output_size);

    reset_scp_debug_termination();

    sim_deb = saved_sim_deb;
    sim_deb_switches = saved_sim_deb_switches;
    sim_scp_dev.dctrl = saved_scp_debug;

    close_status = fclose(stream);

    assert_int_equal(reset_close_status, 0);
    assert_int_equal(read_status, 0);
    assert_int_equal(close_status, 0);
    return output;
}

static void write_partial_debug_line(void)
{
    _sim_debug_device(SCP_LOG_TESTING, &sim_scp_dev, "partial debug line");
}

static void test_debug_flush_writes_partial_filtered_line(void **state)
{
    char *output;

    (void)state;

    output = capture_scp_debug_output(write_partial_debug_line);
    assert_int_equal(count_text_occurrences(output, "DBG("), 1);
    assert_non_null(strstr(output,
                           "> SCP-PROCESS LOG TEST: partial debug line"));
    assert_text_ends_with(output, "partial debug line");

    free(output);
}

static void write_complete_then_partial_debug_lines(void)
{
    _sim_debug_device(SCP_LOG_TESTING, &sim_scp_dev, "complete debug line\n");
    _sim_debug_device(SCP_LOG_TESTING, &sim_scp_dev, "partial debug line");
}

static void
test_debug_flush_writes_partial_line_after_complete_line(void **state)
{
    char *output;

    (void)state;

    output = capture_scp_debug_output(write_complete_then_partial_debug_lines);
    assert_int_equal(count_text_occurrences(output, "DBG("), 2);
    assert_non_null(
        strstr(output, "> SCP-PROCESS LOG TEST: complete debug line\r\nDBG("));
    assert_non_null(strstr(output,
                           "> SCP-PROCESS LOG TEST: partial debug line"));
    assert_text_ends_with(output, "partial debug line");

    free(output);
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
        cmocka_unit_test(test_debug_flush_writes_partial_filtered_line),
        cmocka_unit_test(
            test_debug_flush_writes_partial_line_after_complete_line),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
