#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "isbc201_internal.h"
#include "system_defs.h"

#define TEST_IOPB_ADDR 0x2100
#define TEST_FDCINT 0x04
#define TEST_RBYT_NOT_READY 0x80
#define TEST_DISK_SIZE 256256
#define TEST_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

extern UNIT isbc201_unit[];
extern DEVICE isbc201_dev;

t_stat isbc201_set_port(UNIT *uptr, int32_t val, const char *cptr, void *desc);
t_stat isbc201_set_int(UNIT *uptr, int32_t val, const char *cptr, void *desc);
void isbc201_reset_dev(void);
uint8_t isbc201r0(bool io, uint8_t data, uint8_t devnum);
uint8_t isbc201r1(bool io, uint8_t data, uint8_t devnum);
uint8_t isbc201r2(bool io, uint8_t data, uint8_t devnum);
uint8_t isbc201r3(bool io, uint8_t data, uint8_t devnum);
uint8_t reg_dev(uint8_t (*routine)(bool, uint8_t, uint8_t), uint16_t port,
              uint16_t devnum, uint8_t dummy);
uint8_t unreg_dev(uint16_t port);
uint8_t get_mbyte(uint16_t addr);
void put_mbyte(uint16_t addr, uint8_t val);

static uint8_t test_memory[UINT16_MAX + 1];
static uint8_t test_disk[TEST_DISK_SIZE];
static uint16_t registered_ports[8];
static uint8_t registered_count;

uint16_t PCX;

/*
 * Provide the fake multibus registration hook used by the iSBC 201 setup
 * commands so tests can verify that valid port changes wire the expected
 * controller handlers.
 */
uint8_t reg_dev(uint8_t (*routine)(bool, uint8_t, uint8_t), uint16_t port,
              uint16_t devnum, uint8_t dummy)
{
    (void)routine;
    (void)devnum;
    (void)dummy;

    assert_true(registered_count < TEST_ARRAY_SIZE(registered_ports));
    registered_ports[registered_count++] = port;
    return 0;
}

/*
 * Provide the fake multibus unregistration hook required by the linked
 * controller object. These tests do not exercise controller removal.
 */
uint8_t unreg_dev(uint16_t port)
{
    (void)port;

    return 0;
}

/*
 * Read one byte from the fake system memory image used for iSBC 201 IOPBs.
 */
uint8_t get_mbyte(uint16_t addr)
{
    return test_memory[addr];
}

/*
 * Write one byte to the fake system memory image used for disk transfers.
 */
void put_mbyte(uint16_t addr, uint8_t val)
{
    test_memory[addr] = val;
}

/*
 * Reset the controller, fake memory, and fake disk so each test starts from
 * a ready drive 0 and no pending interrupt state.
 */
static int setup_isbc201(void **state)
{
    (void)state;

    memset(test_memory, 0, sizeof(test_memory));
    memset(test_disk, 0, sizeof(test_disk));
    memset(registered_ports, 0, sizeof(registered_ports));
    registered_count = 0;
    PCX = 0;

    memset(&fdc201, 0, sizeof(fdc201));
    for (uint32_t i = 0; i < ISBC201_FDD_NUM; ++i) {
        isbc201_unit[i].flags = 0;
        isbc201_unit[i].filebuf = NULL;
        isbc201_unit[i].u6 = i;
    }
    isbc201_unit[0].flags = UNIT_ATT;
    isbc201_unit[0].filebuf = test_disk;
    isbc201_unit[0].capac = TEST_DISK_SIZE;
    isbc201_reset_dev();

    return 0;
}

/*
 * Fill the fake IOPB with a no-operation command whose channel word controls
 * whether the controller should raise a completion interrupt.
 */
static void write_nop_iopb(uint8_t channel_word)
{
    test_memory[TEST_IOPB_ADDR] = channel_word;
    test_memory[TEST_IOPB_ADDR + 1] = 0x00;
    test_memory[TEST_IOPB_ADDR + 2] = 0x01;
    test_memory[TEST_IOPB_ADDR + 3] = 0x00;
    test_memory[TEST_IOPB_ADDR + 4] = 0x01;
    test_memory[TEST_IOPB_ADDR + 5] = 0x00;
    test_memory[TEST_IOPB_ADDR + 6] = 0x30;
    test_memory[TEST_IOPB_ADDR + 7] = 0x00;
    test_memory[TEST_IOPB_ADDR + 8] = 0x00;
    test_memory[TEST_IOPB_ADDR + 9] = 0x00;
}

/*
 * Start an iSBC 201 operation through the controller's public I/O ports.
 */
static void start_iopb(void)
{
    isbc201r1(true, TEST_IOPB_ADDR & 0xff, 0);
    isbc201r2(true, TEST_IOPB_ADDR >> 8, 0);
}

/*
 * A valid port setting should update the controller base and register all
 * documented iSBC 201 I/O ports.
 */
static void test_set_port_accepts_hex_port(void **state)
{
    const uint16_t expected_ports[] = {0x88, 0x89, 0x8a, 0x8b, 0x8f};

    (void)state;

    assert_int_equal(isbc201_set_port(&isbc201_unit[0], 0, "88", NULL),
                     SCPE_OK);

    assert_int_equal(fdc201.baseport, 0x88);
    assert_int_equal(registered_count, TEST_ARRAY_SIZE(expected_ports));
    assert_memory_equal(registered_ports, expected_ports,
                        sizeof(expected_ports));
}

/*
 * A port setting with no hexadecimal value should be rejected before it can
 * modify controller state or register handlers at an unintended port.
 */
static void test_set_port_rejects_missing_hex_value(void **state)
{
    (void)state;

    fdc201.baseport = 0x88;
    assert_int_equal(isbc201_set_port(&isbc201_unit[0], 0, "not-hex", NULL),
                     SCPE_ARG);

    assert_int_equal(fdc201.baseport, 0x88);
    assert_int_equal(registered_count, 0);
}

/*
 * A port setting with trailing junk should be rejected rather than silently
 * accepting the leading hexadecimal prefix.
 */
static void test_set_port_rejects_trailing_junk(void **state)
{
    (void)state;

    fdc201.baseport = 0x88;
    assert_int_equal(isbc201_set_port(&isbc201_unit[0], 0, "88junk", NULL),
                     SCPE_ARG);

    assert_int_equal(fdc201.baseport, 0x88);
    assert_int_equal(registered_count, 0);
}

/*
 * Port values outside the byte-wide I/O port range should be rejected before
 * they can truncate into an unintended configured port.
 */
static void test_set_port_rejects_out_of_range_value(void **state)
{
    (void)state;

    fdc201.baseport = 0x88;
    assert_int_equal(isbc201_set_port(&isbc201_unit[0], 0, "100", NULL),
                     SCPE_ARG);

    assert_int_equal(fdc201.baseport, 0x88);
    assert_int_equal(registered_count, 0);
}

/*
 * A null port argument is invalid input from the command layer and must not
 * be passed to sscanf().
 */
static void test_set_port_rejects_null_value(void **state)
{
    (void)state;

    fdc201.baseport = 0x88;
    assert_int_equal(isbc201_set_port(&isbc201_unit[0], 0, NULL, NULL),
                     SCPE_ARG);

    assert_int_equal(fdc201.baseport, 0x88);
    assert_int_equal(registered_count, 0);
}

/*
 * A valid interrupt setting should update the controller interrupt number.
 */
static void test_set_int_accepts_hex_interrupt(void **state)
{
    (void)state;

    assert_int_equal(isbc201_set_int(&isbc201_unit[0], 0, "05", NULL), SCPE_OK);

    assert_int_equal(fdc201.intnum, 0x05);
}

/*
 * An interrupt setting with no hexadecimal value should be rejected before it
 * can modify the configured interrupt number.
 */
static void test_set_int_rejects_missing_hex_value(void **state)
{
    (void)state;

    fdc201.intnum = 0x05;
    assert_int_equal(isbc201_set_int(&isbc201_unit[0], 0, "not-hex", NULL),
                     SCPE_ARG);

    assert_int_equal(fdc201.intnum, 0x05);
}

/*
 * An interrupt setting with trailing junk should be rejected rather than
 * silently accepting the leading hexadecimal prefix.
 */
static void test_set_int_rejects_trailing_junk(void **state)
{
    (void)state;

    fdc201.intnum = 0x05;
    assert_int_equal(isbc201_set_int(&isbc201_unit[0], 0, "05junk", NULL),
                     SCPE_ARG);

    assert_int_equal(fdc201.intnum, 0x05);
}

/*
 * Interrupt values outside the byte-wide configured field should be rejected
 * before they can truncate into an unintended interrupt number.
 */
static void test_set_int_rejects_out_of_range_value(void **state)
{
    (void)state;

    fdc201.intnum = 0x05;
    assert_int_equal(isbc201_set_int(&isbc201_unit[0], 0, "100", NULL),
                     SCPE_ARG);

    assert_int_equal(fdc201.intnum, 0x05);
}

/*
 * A null interrupt argument is invalid input from the command layer and must
 * not be passed to sscanf().
 */
static void test_set_int_rejects_null_value(void **state)
{
    (void)state;

    fdc201.intnum = 0x05;
    assert_int_equal(isbc201_set_int(&isbc201_unit[0], 0, NULL, NULL),
                     SCPE_ARG);

    assert_int_equal(fdc201.intnum, 0x05);
}

/*
 * Channel word interrupt-control value 00 documents that an I/O complete
 * interrupt should be issued when an operation completes.
 */
static void test_nop_iopb_default_channel_word_sets_interrupt(void **state)
{
    (void)state;

    write_nop_iopb(0x00);
    start_iopb();

    assert_true(isbc201r0(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(isbc201r3(false, 0, 0), 0);
}

/*
 * Channel word interrupt-control value 01 documents that I/O complete
 * interrupts are disabled for the operation.
 */
static void test_nop_iopb_interrupt_disable_suppresses_interrupt(void **state)
{
    (void)state;

    write_nop_iopb(0x10);
    start_iopb();

    assert_false(isbc201r0(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(isbc201r3(false, 0, 0), 0);
}

/*
 * The channel word interrupt-control value also applies to error completion,
 * so a disabled interrupt should not raise FDCINT for a not-ready drive.
 */
static void test_not_ready_completion_honors_interrupt_disable(void **state)
{
    (void)state;

    isbc201_unit[0].flags = 0;
    isbc201_reset_dev();
    write_nop_iopb(0x10);
    start_iopb();

    assert_false(isbc201r0(false, 0, 0) & TEST_FDCINT);
    assert_int_equal(isbc201r3(false, 0, 0), TEST_RBYT_NOT_READY);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_set_port_accepts_hex_port, setup_isbc201),
        cmocka_unit_test_setup(test_set_port_rejects_missing_hex_value,
                               setup_isbc201),
        cmocka_unit_test_setup(test_set_port_rejects_trailing_junk,
                               setup_isbc201),
        cmocka_unit_test_setup(test_set_port_rejects_out_of_range_value,
                               setup_isbc201),
        cmocka_unit_test_setup(test_set_port_rejects_null_value, setup_isbc201),
        cmocka_unit_test_setup(test_set_int_accepts_hex_interrupt,
                               setup_isbc201),
        cmocka_unit_test_setup(test_set_int_rejects_missing_hex_value,
                               setup_isbc201),
        cmocka_unit_test_setup(test_set_int_rejects_trailing_junk,
                               setup_isbc201),
        cmocka_unit_test_setup(test_set_int_rejects_out_of_range_value,
                               setup_isbc201),
        cmocka_unit_test_setup(test_set_int_rejects_null_value, setup_isbc201),
        cmocka_unit_test_setup(
            test_nop_iopb_default_channel_word_sets_interrupt, setup_isbc201),
        cmocka_unit_test_setup(
            test_nop_iopb_interrupt_disable_suppresses_interrupt,
            setup_isbc201),
        cmocka_unit_test_setup(
            test_not_ready_completion_honors_interrupt_disable, setup_isbc201),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
