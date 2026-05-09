/* test_zx200a.c: Zendex ZX-200A unit tests */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "system_defs.h"
#include "zx200a_internal.h"

#define TEST_IOPB_ADDR 0x2300
#define TEST_FDCINT 0x04
#define TEST_RBYT_ADDRESS_ERROR 0x08
#define TEST_RBYT_WRITE_PROTECT 0x20
#define TEST_RBYT_NOT_READY 0x80
#define TEST_DD_DISK_SIZE 512512
#define TEST_SD_DISK_SIZE 256256
#define TEST_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define TEST_UNIT_WPMODE (1u << UNIT_V_UF)

#define TEST_DNOP 0x00
#define TEST_DWRITE 0x06

typedef uint8 (*test_io_handler)(bool, uint8, uint8);

typedef struct {
    test_io_handler routine;
    uint16 port;
} test_registration;

extern UNIT zx200a_unit[];
extern DEVICE zx200a_dev;
extern int zx200a_onetime;

t_stat zx200a_set_port(UNIT *uptr, int32 val, const char *cptr, void *desc);
t_stat zx200a_set_int(UNIT *uptr, int32 val, const char *cptr, void *desc);
t_stat zx200a_reset(DEVICE *dptr);
void zx200a_reset_dev(void);
uint8 zx200ar0DD(bool io, uint8 data, uint8 devnum);
uint8 zx200ar0SD(bool io, uint8 data, uint8 devnum);
uint8 zx200ar1DD(bool io, uint8 data, uint8 devnum);
uint8 zx200ar1SD(bool io, uint8 data, uint8 devnum);
uint8 zx200ar2DD(bool io, uint8 data, uint8 devnum);
uint8 zx200ar2SD(bool io, uint8 data, uint8 devnum);
uint8 zx200ar3(bool io, uint8 data, uint8 devnum);
uint8 zx200ar7(bool io, uint8 data, uint8 devnum);
uint8 reg_dev(uint8 (*routine)(bool, uint8, uint8), uint16 port,
              uint16 devnum, uint8 dummy);
uint8 unreg_dev(uint16 port);
uint8 get_mbyte(uint16 addr);
void put_mbyte(uint16 addr, uint8 val);

static uint8 test_memory[UINT16_MAX + 1];
static uint8 test_dd_disk[TEST_DD_DISK_SIZE];
static uint8 test_sd_disk[TEST_SD_DISK_SIZE];
static test_registration registered_devices[16];
static uint8 registered_count;

uint16 PCX;

/*
 * Provide the fake multibus registration hook used by the ZX-200A setup
 * commands so tests can verify that valid port changes wire the expected
 * controller ports and handlers.
 */
uint8 reg_dev(uint8 (*routine)(bool, uint8, uint8), uint16 port,
              uint16 devnum, uint8 dummy)
{
    (void)devnum;
    (void)dummy;

    assert_true(registered_count < TEST_ARRAY_SIZE(registered_devices));
    registered_devices[registered_count].routine = routine;
    registered_devices[registered_count].port = port;
    registered_count++;
    return 0;
}

/*
 * Provide the fake multibus unregistration hook required by the linked
 * controller object. These tests do not exercise controller removal.
 */
uint8 unreg_dev(uint16 port)
{
    (void)port;

    return 0;
}

/*
 * Verify that multibus registration calls occurred in the expected order and
 * wired both the documented port and the correct handler for each port.
 */
static void assert_registered_devices(const test_registration *expected,
                                      size_t count)
{
    assert_int_equal(registered_count, count);
    for (size_t i = 0; i < count; ++i) {
        assert_ptr_equal(registered_devices[i].routine, expected[i].routine);
        assert_int_equal(registered_devices[i].port, expected[i].port);
    }
}

/*
 * Read one byte from the fake system memory image used for ZX-200A IOPBs.
 */
uint8 get_mbyte(uint16 addr)
{
    return test_memory[addr];
}

/*
 * Write one byte to the fake system memory image used for disk transfers.
 */
void put_mbyte(uint16 addr, uint8 val)
{
    test_memory[addr] = val;
}

/*
 * Reset the controller, fake memory, and fake disks so each test starts from
 * a ready double-density drive 0 and no pending interrupt state.
 */
static int setup_zx200a(void **state)
{
    (void)state;

    memset(test_memory, 0, sizeof(test_memory));
    memset(test_dd_disk, 0, sizeof(test_dd_disk));
    memset(test_sd_disk, 0, sizeof(test_sd_disk));
    memset(registered_devices, 0, sizeof(registered_devices));
    registered_count = 0;
    PCX = 0;

    memset(&zx200a, 0, sizeof(zx200a));
    for (uint32 i = 0; i < ZX200A_FDD_NUM; ++i) {
        zx200a_unit[i].flags = 0;
        zx200a_unit[i].filebuf = NULL;
        zx200a_unit[i].u6 = i;
    }
    zx200a_unit[0].flags = UNIT_ATT;
    zx200a_unit[0].filebuf = test_dd_disk;
    zx200a_unit[0].capac = TEST_DD_DISK_SIZE;
    zx200a_unit[4].flags = UNIT_ATT;
    zx200a_unit[4].filebuf = test_sd_disk;
    zx200a_unit[4].capac = TEST_SD_DISK_SIZE;
    zx200a_dev.flags |= DEV_DIS;
    zx200a_onetime = 0;
    zx200a_reset_dev();

    return 0;
}

/*
 * Fill the fake IOPB with a command whose channel word controls whether the
 * controller should raise a completion interrupt.
 */
static void write_iopb(uint8 channel_word, uint8 instruction, uint8 records,
                       uint8 track, uint8 sector)
{
    test_memory[TEST_IOPB_ADDR] = channel_word;
    test_memory[TEST_IOPB_ADDR + 1] = instruction;
    test_memory[TEST_IOPB_ADDR + 2] = records;
    test_memory[TEST_IOPB_ADDR + 3] = track;
    test_memory[TEST_IOPB_ADDR + 4] = sector;
    test_memory[TEST_IOPB_ADDR + 5] = 0x00;
    test_memory[TEST_IOPB_ADDR + 6] = 0x30;
}

/*
 * Fill the fake IOPB with a valid no-operation command.
 */
static void write_nop_iopb(uint8 channel_word)
{
    write_iopb(channel_word, TEST_DNOP, 0x01, 0x00, 0x01);
}

/*
 * Start a double-density ZX-200A operation through the controller's public
 * I/O ports.
 */
static void start_dd_iopb(void)
{
    zx200ar1DD(true, TEST_IOPB_ADDR & 0xff, 0);
    zx200ar2DD(true, TEST_IOPB_ADDR >> 8, 0);
}

/*
 * Start a single-density ZX-200A operation through the controller's public
 * I/O ports.
 */
static void start_sd_iopb(void)
{
    zx200ar1SD(true, TEST_IOPB_ADDR & 0xff, 0);
    zx200ar2SD(true, TEST_IOPB_ADDR >> 8, 0);
}

/*
 * A valid port setting should update the controller base and register all
 * documented ZX-200A double-density and single-density I/O ports.
 */
static void test_set_port_accepts_hex_port(void **state)
{
    const test_registration expected[] = {
        {zx200ar0DD, 0x78}, {zx200ar1DD, 0x79}, {zx200ar2DD, 0x7a},
        {zx200ar3, 0x7b},   {zx200ar7, 0x7f},   {zx200ar0SD, 0x88},
        {zx200ar1SD, 0x89}, {zx200ar2SD, 0x8a}, {zx200ar3, 0x8b},
        {zx200ar7, 0x8f},
    };

    (void)state;

    assert_int_equal(zx200a_set_port(&zx200a_unit[0], 0, "78", NULL), SCPE_OK);

    assert_int_equal(zx200a.baseport, 0x78);
    assert_registered_devices(expected, TEST_ARRAY_SIZE(expected));
}

/*
 * Reset should register the same documented double-density and
 * single-density port handlers as an explicit PORT command.
 */
static void test_reset_registers_documented_handlers_when_enabled(void **state)
{
    const test_registration expected[] = {
        {zx200ar0DD, 0x78}, {zx200ar1DD, 0x79}, {zx200ar2DD, 0x7a},
        {zx200ar3, 0x7b},   {zx200ar7, 0x7f},   {zx200ar0SD, 0x88},
        {zx200ar1SD, 0x89}, {zx200ar2SD, 0x8a}, {zx200ar3, 0x8b},
        {zx200ar7, 0x8f},
    };

    (void)state;

    zx200a.baseport = 0x78;
    zx200a.intnum = 0x05;
    zx200a_dev.flags &= ~DEV_DIS;

    assert_int_equal(zx200a_reset(&zx200a_dev), SCPE_OK);

    assert_registered_devices(expected, TEST_ARRAY_SIZE(expected));
}

/*
 * A port setting with no hexadecimal value should be rejected before it can
 * modify controller state or register handlers at an unintended port.
 */
static void test_set_port_rejects_missing_hex_value(void **state)
{
    (void)state;

    zx200a.baseport = 0x78;
    assert_int_equal(zx200a_set_port(&zx200a_unit[0], 0, "not-hex", NULL),
                     SCPE_ARG);

    assert_int_equal(zx200a.baseport, 0x78);
    assert_int_equal(registered_count, 0);
}

/*
 * A port setting with trailing junk should be rejected rather than silently
 * accepting the leading hexadecimal prefix.
 */
static void test_set_port_rejects_trailing_junk(void **state)
{
    (void)state;

    zx200a.baseport = 0x78;
    assert_int_equal(zx200a_set_port(&zx200a_unit[0], 0, "78junk", NULL),
                     SCPE_ARG);

    assert_int_equal(zx200a.baseport, 0x78);
    assert_int_equal(registered_count, 0);
}

/*
 * Port values outside the byte-wide I/O port range should be rejected before
 * they can truncate into an unintended configured port.
 */
static void test_set_port_rejects_out_of_range_value(void **state)
{
    (void)state;

    zx200a.baseport = 0x78;
    assert_int_equal(zx200a_set_port(&zx200a_unit[0], 0, "100", NULL),
                     SCPE_ARG);

    assert_int_equal(zx200a.baseport, 0x78);
    assert_int_equal(registered_count, 0);
}

/*
 * A null port argument is invalid input from the command layer and must not
 * be passed to sscanf().
 */
static void test_set_port_rejects_null_value(void **state)
{
    (void)state;

    zx200a.baseport = 0x78;
    assert_int_equal(zx200a_set_port(&zx200a_unit[0], 0, NULL, NULL), SCPE_ARG);

    assert_int_equal(zx200a.baseport, 0x78);
    assert_int_equal(registered_count, 0);
}

/*
 * A valid interrupt setting should update the controller interrupt number.
 */
static void test_set_int_accepts_hex_interrupt(void **state)
{
    (void)state;

    assert_int_equal(zx200a_set_int(&zx200a_unit[0], 0, "05", NULL), SCPE_OK);

    assert_int_equal(zx200a.intnum, 0x05);
}

/*
 * An interrupt setting with no hexadecimal value should be rejected before it
 * can modify the configured interrupt number.
 */
static void test_set_int_rejects_missing_hex_value(void **state)
{
    (void)state;

    zx200a.intnum = 0x05;
    assert_int_equal(zx200a_set_int(&zx200a_unit[0], 0, "not-hex", NULL),
                     SCPE_ARG);

    assert_int_equal(zx200a.intnum, 0x05);
}

/*
 * An interrupt setting with trailing junk should be rejected rather than
 * silently accepting the leading hexadecimal prefix.
 */
static void test_set_int_rejects_trailing_junk(void **state)
{
    (void)state;

    zx200a.intnum = 0x05;
    assert_int_equal(zx200a_set_int(&zx200a_unit[0], 0, "05junk", NULL),
                     SCPE_ARG);

    assert_int_equal(zx200a.intnum, 0x05);
}

/*
 * Interrupt values outside the byte-wide configured field should be rejected
 * before they can truncate into an unintended interrupt number.
 */
static void test_set_int_rejects_out_of_range_value(void **state)
{
    (void)state;

    zx200a.intnum = 0x05;
    assert_int_equal(zx200a_set_int(&zx200a_unit[0], 0, "100", NULL), SCPE_ARG);

    assert_int_equal(zx200a.intnum, 0x05);
}

/*
 * A null interrupt argument is invalid input from the command layer and must
 * not be passed to sscanf().
 */
static void test_set_int_rejects_null_value(void **state)
{
    (void)state;

    zx200a.intnum = 0x05;
    assert_int_equal(zx200a_set_int(&zx200a_unit[0], 0, NULL, NULL), SCPE_ARG);

    assert_int_equal(zx200a.intnum, 0x05);
}

/*
 * Channel word interrupt-control value 00 documents that an I/O complete
 * interrupt should be issued when an operation completes.
 */
static void test_nop_iopb_default_channel_word_sets_interrupt(void **state)
{
    (void)state;

    write_nop_iopb(0x00);
    start_dd_iopb();

    assert_true(zx200ar0DD(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(zx200ar3(false, 0, 0), 0);
}

/*
 * Channel word interrupt-control value 01 documents that I/O complete
 * interrupts are disabled for the operation.
 */
static void test_nop_iopb_interrupt_disable_suppresses_interrupt(void **state)
{
    (void)state;

    write_nop_iopb(0x10);
    start_dd_iopb();

    assert_false(zx200ar0DD(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(zx200ar3(false, 0, 0), 0);
}

/*
 * The single-density I/O port path uses the same IOPB completion logic and
 * must also suppress FDCINT when the channel word disables interrupts.
 */
static void
test_sd_nop_iopb_interrupt_disable_suppresses_interrupt(void **state)
{
    (void)state;

    write_nop_iopb(0x10);
    start_sd_iopb();

    assert_false(zx200ar0SD(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(zx200ar3(false, 0, 0), 0);
}

/*
 * The channel word interrupt-control value also applies to error completion,
 * so a disabled interrupt should not raise FDCINT for a not-ready drive.
 */
static void test_not_ready_completion_honors_interrupt_disable(void **state)
{
    (void)state;

    zx200a_unit[0].flags = 0;
    zx200a_reset_dev();
    write_nop_iopb(0x10);
    start_dd_iopb();

    assert_false(zx200ar0DD(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(zx200ar3(false, 0, 0), TEST_RBYT_NOT_READY);
}

/*
 * Address-error completion uses the same channel word interrupt-control bits
 * as successful completion.
 */
static void test_address_error_completion_honors_interrupt_disable(void **state)
{
    (void)state;

    write_iopb(0x10, TEST_DNOP, 0x01, 0x00, 0x00);
    start_dd_iopb();

    assert_false(zx200ar0DD(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(zx200ar3(false, 0, 0), TEST_RBYT_ADDRESS_ERROR);
}

/*
 * Write-protect completion uses the same channel word interrupt-control bits
 * as successful completion.
 */
static void test_write_protect_completion_honors_interrupt_disable(void **state)
{
    (void)state;

    zx200a_unit[0].flags |= TEST_UNIT_WPMODE;
    write_iopb(0x10, TEST_DWRITE, 0x01, 0x00, 0x01);
    start_dd_iopb();

    assert_false(zx200ar0DD(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(zx200ar3(false, 0, 0), TEST_RBYT_WRITE_PROTECT);
}

/*
 * Illegal channel word interrupt-control values remain on the historical
 * path that reports completion with an interrupt.
 */
static void test_illegal_channel_word_value_preserves_interrupt(void **state)
{
    (void)state;

    write_nop_iopb(0x20);
    start_dd_iopb();

    assert_true(zx200ar0DD(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(zx200ar3(false, 0, 0), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_set_port_accepts_hex_port, setup_zx200a),
        cmocka_unit_test_setup(
            test_reset_registers_documented_handlers_when_enabled,
            setup_zx200a),
        cmocka_unit_test_setup(test_set_port_rejects_missing_hex_value,
                               setup_zx200a),
        cmocka_unit_test_setup(test_set_port_rejects_trailing_junk,
                               setup_zx200a),
        cmocka_unit_test_setup(test_set_port_rejects_out_of_range_value,
                               setup_zx200a),
        cmocka_unit_test_setup(test_set_port_rejects_null_value, setup_zx200a),
        cmocka_unit_test_setup(test_set_int_accepts_hex_interrupt,
                               setup_zx200a),
        cmocka_unit_test_setup(test_set_int_rejects_missing_hex_value,
                               setup_zx200a),
        cmocka_unit_test_setup(test_set_int_rejects_trailing_junk,
                               setup_zx200a),
        cmocka_unit_test_setup(test_set_int_rejects_out_of_range_value,
                               setup_zx200a),
        cmocka_unit_test_setup(test_set_int_rejects_null_value, setup_zx200a),
        cmocka_unit_test_setup(
            test_nop_iopb_default_channel_word_sets_interrupt, setup_zx200a),
        cmocka_unit_test_setup(
            test_nop_iopb_interrupt_disable_suppresses_interrupt, setup_zx200a),
        cmocka_unit_test_setup(
            test_sd_nop_iopb_interrupt_disable_suppresses_interrupt,
            setup_zx200a),
        cmocka_unit_test_setup(
            test_not_ready_completion_honors_interrupt_disable, setup_zx200a),
        cmocka_unit_test_setup(
            test_address_error_completion_honors_interrupt_disable,
            setup_zx200a),
        cmocka_unit_test_setup(
            test_write_protect_completion_honors_interrupt_disable,
            setup_zx200a),
        cmocka_unit_test_setup(
            test_illegal_channel_word_value_preserves_interrupt, setup_zx200a),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
