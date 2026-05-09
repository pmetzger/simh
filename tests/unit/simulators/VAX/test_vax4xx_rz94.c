#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "scp.h"
#include "vax_defs.h"
#include "sim_scsi.h"
#include "test_simh_personality.h"
#include "test_support.h"

#define RZ_INT_ILLCMD 0x40
#define RZ_INT_DIS 0x20
#define RZ_INT_BUSSV 0x10
#define RZ_INT_FC 0x08

extern DEVICE rz_dev;
extern UNIT rz_unit[];
extern SCSI_BUS rz_bus;
extern uint8_t rz_cfg1;
extern uint8_t rz_cfg2;
extern uint8_t rz_cfg3;
extern uint8_t rz_dest;
extern uint8_t rz_int;
extern uint8_t rz_seq;
extern uint8_t rz_stat;
extern uint8_t *rz_buf;
extern uint8_t rz_fifo[];
extern uint32_t rz_fifo_b;
extern uint32_t rz_fifo_c;
extern uint32_t rz_fifo_t;
extern uint32_t rz_txc;

extern t_stat rz_reset(DEVICE *dptr);
void rz_cmd(uint32_t cmd);

int32_t int_req[IPL_HLVL];
int32_t sys_model;
uint32_t trpirq;
uint32_t fault_PC;
int32_t hlt_pin;
jmp_buf save_env;

static uint8_t rz_test_scsi_buffer[256];
static uint8_t rz_test_transfer_buffer[256];

#if defined (HAVE_FMEMOPEN)
/* Install the RZ device in a minimal simulator environment for ramdisk tests. */
static void install_rz_device(void)
{
    DEVICE *devices[] = {
        &rz_dev,
        NULL,
    };

    simh_test_reset_simulator_state();
    assert_int_equal(simh_test_set_sim_name("VAX-RAMDISK"), 0);
    assert_int_equal(simh_test_set_devices(devices), 0);
    sim_dflt_dev = &rz_dev;
    sim_switches = 0;
    sim_switch_number = 0;
    sim_quiet = 0;
    sim_show_message = 1;
    assert_int_equal(rz_reset(&rz_dev), SCPE_OK);
    rz_unit[0].flags &= ~UNIT_DIS;
}
#endif

int32_t Map_ReadB(uint32_t ba, int32_t bc, uint8_t *buf)
{
    (void)ba;
    memset(buf, 0, (size_t)bc);
    return 0;
}

int32_t Map_WriteB(uint32_t ba, int32_t bc, uint8_t *buf)
{
    (void)ba;
    (void)bc;
    (void)buf;
    return 0;
}

int32_t eval_int(void)
{
    return 0;
}

static void load_rz_fifo(const uint8_t *data, uint32_t len)
{
    assert_true(len <= 16);
    memcpy(rz_fifo, data, len);
    rz_fifo_t = 0;
    rz_fifo_b = len;
    rz_fifo_c = len;
}

static void reset_rz_command_state(void)
{
    size_t i;

    memset(&rz_bus, 0, sizeof(rz_bus));
    rz_bus.dptr = &rz_dev;
    rz_bus.buf = rz_test_scsi_buffer;
    rz_buf = rz_test_transfer_buffer;
    rz_bus.initiator = -1;
    rz_bus.target = -1;
    rz_cfg1 = RZ_SCSI_ID;
    rz_int = 0;
    rz_fifo_t = 0;
    rz_fifo_b = 0;
    rz_fifo_c = 0;
    memset(rz_fifo, 0, 16);
    int_req[IPL_SC] = 0;
    sim_cancel(&rz_unit[8]);
    for (i = 0; i < 8; i++) {
        rz_unit[i].flags |= UNIT_DIS;
        rz_unit[i].up7 = NULL;
    }
}

#if defined (HAVE_FMEMOPEN)
/* Begin a SCSI command transaction against an RZ target. */
static void start_rz_scsi_command(uint32_t target)
{
    rz_bus.initiator = RZ_SCSI_ID;
    rz_bus.target = (int32_t)target;
    rz_bus.phase = SCSI_CMD;
    rz_bus.req = true;
    rz_bus.buf_b = 0;
    rz_bus.buf_t = 0;
}

/* Verify an RZ SCSI command completed with good status. */
static void assert_rz_scsi_good_status(void)
{
    uint8_t status;

    assert_int_equal(rz_bus.phase, SCSI_STS);
    assert_int_equal(scsi_read(&rz_bus, &status, 1), 1);
    assert_int_equal(status, 0);
}

/* Write one 512-byte sector through the RZ SCSI command interface. */
static void write_rz0_sector(uint32_t lba, const uint8_t *data)
{
    uint8_t command[6] = {
        0x0A,
        (uint8_t)((lba >> 16) & 0x1F),
        (uint8_t)(lba >> 8),
        (uint8_t)lba,
        1,
        0,
    };

    start_rz_scsi_command(0);
    assert_int_equal(scsi_write(&rz_bus, command, sizeof(command)),
                     sizeof(command));
    assert_int_equal(rz_bus.phase, SCSI_DATO);
    assert_int_equal(scsi_write(&rz_bus, (uint8_t *)data, 512), 512);
    assert_rz_scsi_good_status();
}

/* Read one 512-byte sector through the RZ SCSI command interface. */
static void read_rz0_sector(uint32_t lba, uint8_t *data)
{
    uint8_t command[6] = {
        0x08,
        (uint8_t)((lba >> 16) & 0x1F),
        (uint8_t)(lba >> 8),
        (uint8_t)lba,
        1,
        0,
    };

    start_rz_scsi_command(0);
    assert_int_equal(scsi_write(&rz_bus, command, sizeof(command)),
                     sizeof(command));
    assert_int_equal(rz_bus.phase, SCSI_DATI);
    assert_int_equal(scsi_read(&rz_bus, data, 512), 512);
    assert_rz_scsi_good_status();
}
#endif

static void test_flush_fifo_does_not_set_interrupt(void **state)
{
    (void)state;

    reset_rz_command_state();
    rz_fifo[0] = 0xa5;
    rz_fifo_b = 1;
    rz_fifo_c = 1;

    rz_cmd(0x01);

    assert_int_equal(rz_fifo_c, 0);
    assert_false(rz_int & RZ_INT_FC);
    assert_false(int_req[IPL_SC] != 0);
}

static void test_disable_selection_reselection_completes(void **state)
{
    (void)state;

    reset_rz_command_state();

    rz_cmd(0x45);

    assert_true(rz_int & RZ_INT_FC);
    assert_false(rz_int & RZ_INT_ILLCMD);
}

static void test_reset_chip_disconnects_without_bus_reset(void **state)
{
    (void)state;

    reset_rz_command_state();
    rz_bus.initiator = RZ_SCSI_ID;
    rz_bus.target = 4;
    rz_bus.phase = SCSI_MSGI;
    rz_bus.atn = true;
    rz_bus.req = true;
    rz_bus.buf_b = 2;
    rz_bus.buf_t = 4;
    rz_bus.sense_key = 0x05;
    rz_bus.sense_code = 0x24;
    rz_bus.sense_qual = 0x01;
    rz_bus.sense_info = 0x12345678;

    rz_txc = 3;
    rz_cfg1 = 0x5a;
    rz_cfg2 = 0xa5;
    rz_cfg3 = 0xc3;
    rz_stat = 0xff;
    rz_seq = 0x12;
    rz_int = RZ_INT_FC;
    rz_dest = 4;
    rz_fifo[0] = 0xa5;
    rz_fifo_b = 1;
    rz_fifo_c = 1;

    rz_cmd(0x02);

    assert_int_equal(rz_txc, 0);
    assert_int_equal(rz_cfg1, 0x02);
    assert_int_equal(rz_cfg2, 0);
    assert_int_equal(rz_cfg3, 0);
    assert_int_equal(rz_stat, 0);
    assert_int_equal(rz_seq, 0);
    assert_int_equal(rz_int, 0);
    assert_int_equal(rz_dest, 0);
    assert_int_equal(rz_fifo_c, 0);
    assert_false(int_req[IPL_SC] != 0);

    assert_int_equal(rz_bus.phase, SCSI_DATO);
    assert_int_equal(rz_bus.initiator, -1);
    assert_int_equal(rz_bus.target, -1);
    assert_false(rz_bus.atn);
    assert_false(rz_bus.req);
    assert_int_equal(rz_bus.buf_b, 0);
    assert_int_equal(rz_bus.buf_t, 0);
    assert_int_equal(rz_bus.sense_key, 0x05);
    assert_int_equal(rz_bus.sense_code, 0x24);
    assert_int_equal(rz_bus.sense_qual, 0x01);
    assert_int_equal(rz_bus.sense_info, 0x12345678);
}

static void test_message_accepted_keeps_bus_for_pending_message(void **state)
{
    (void)state;

    reset_rz_command_state();
    rz_bus.initiator = RZ_SCSI_ID;
    rz_bus.target = 4;
    rz_bus.phase = SCSI_MSGI;
    rz_bus.req = true;
    rz_bus.buf[0] = 0x04;
    rz_bus.buf[1] = 0x00;
    rz_bus.buf_t = 1;
    rz_bus.buf_b = 2;

    rz_cmd(0x12);

    assert_int_equal(rz_bus.phase, SCSI_MSGI);
    assert_int_equal(rz_bus.initiator, RZ_SCSI_ID);
    assert_int_equal(rz_bus.target, 4);
    assert_true(rz_bus.req);
    assert_true(rz_int & RZ_INT_FC);
    assert_true(rz_int & RZ_INT_BUSSV);
    assert_false(rz_int & RZ_INT_DIS);
}

static void test_message_accepted_disconnects_after_last_message(void **state)
{
    (void)state;

    reset_rz_command_state();
    rz_bus.initiator = RZ_SCSI_ID;
    rz_bus.target = 4;
    rz_bus.phase = SCSI_MSGI;
    rz_bus.req = false;
    rz_bus.buf_t = 0;
    rz_bus.buf_b = 0;

    rz_cmd(0x12);

    assert_int_equal(rz_bus.phase, SCSI_DATO);
    assert_int_equal(rz_bus.initiator, -1);
    assert_int_equal(rz_bus.target, -1);
    assert_true(rz_int & RZ_INT_DIS);
    assert_false(rz_int & RZ_INT_BUSSV);
}

static void test_select_with_atn3_sends_three_messages_then_command(
    void **state)
{
    static SCSI_DEV disk = {
        .devtype = SCSI_DISK,
    };
    const uint8_t select_bytes[] = {
        0x80,       /* identify LUN 0 */
        0x20, 0x44, /* simple queue tag 0x44 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* test unit ready */
    };

    (void)state;

    reset_rz_command_state();
    rz_bus.dev[4] = &rz_unit[4];
    rz_unit[4].flags &= ~UNIT_DIS;
    rz_unit[4].up7 = &disk;
    rz_dest = 4;
    load_rz_fifo(select_bytes, sizeof(select_bytes));

    rz_cmd(0x46);

    assert_int_equal(rz_bus.phase, SCSI_STS);
    assert_int_equal(rz_bus.initiator, RZ_SCSI_ID);
    assert_int_equal(rz_bus.target, 4);
    assert_false(rz_bus.atn);
    assert_true(rz_bus.req);
    assert_int_equal(rz_bus.lun, 0);
    assert_int_equal(rz_fifo_c, 0);
    assert_true(rz_int & RZ_INT_FC);
    assert_true(rz_int & RZ_INT_BUSSV);
    assert_false(rz_int & RZ_INT_DIS);
}

#if defined (HAVE_FMEMOPEN)
/* Verify the user-facing attach/save/restore path preserves a VAX RZ
   ramdisk's guest-visible SCSI contents. */
static void test_ramdisk_attach_save_restore_via_rz_device(void **state)
{
    char temp_dir[CBUFSIZE];
    char save_image[CBUFSIZE];
    char state_image[CBUFSIZE];
    char command[2 * CBUFSIZE];
    uint8_t expected[512];
    uint8_t clobber[512];
    uint8_t actual[512];

    (void)state;

    install_rz_device();
    assert_int_equal(simh_test_make_temp_dir(temp_dir, sizeof(temp_dir),
                                             "vax-rz-ramdisk"),
                     0);
    assert_int_equal(simh_test_join_path(save_image, sizeof(save_image),
                                         temp_dir, "rz0-save.dsk"),
                     0);
    assert_int_equal(simh_test_join_path(state_image, sizeof(state_image),
                                         temp_dir, "rz0-state.sav"),
                     0);

    assert_true(snprintf(command, sizeof(command),
                         "RZ0 RAMDISK:SIZE=4096,SAVE=%s",
                         save_image) < (int)sizeof(command));
    sim_switches = 0;
    assert_int_equal(attach_cmd(0, command), SCPE_OK);
    assert_true((rz_unit[0].flags & UNIT_ATT) != 0);

    for (size_t index = 0; index < sizeof(expected); index++)
        expected[index] = (uint8_t)(index ^ 0xA5);
    memset(clobber, 0x3C, sizeof(clobber));
    write_rz0_sector(2, expected);

    sim_switches = 0;
    assert_int_equal(save_cmd(0, state_image), SCPE_OK);
    write_rz0_sector(2, clobber);

    sim_switches = 0;
    assert_int_equal(restore_cmd(0, state_image), SCPE_OK);
    assert_true((rz_unit[0].flags & UNIT_ATT) != 0);

    memset(actual, 0, sizeof(actual));
    read_rz0_sector(2, actual);
    assert_memory_equal(actual, expected, sizeof(actual));

    assert_int_equal(scsi_detach(&rz_unit[0]), SCPE_OK);
    assert_int_equal(simh_test_remove_path(temp_dir), 0);
    simh_test_reset_simulator_state();
}
#endif

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_flush_fifo_does_not_set_interrupt),
        cmocka_unit_test(test_disable_selection_reselection_completes),
        cmocka_unit_test(test_reset_chip_disconnects_without_bus_reset),
        cmocka_unit_test(test_message_accepted_keeps_bus_for_pending_message),
        cmocka_unit_test(test_message_accepted_disconnects_after_last_message),
        cmocka_unit_test(
            test_select_with_atn3_sends_three_messages_then_command),
#if defined (HAVE_FMEMOPEN)
        cmocka_unit_test(test_ramdisk_attach_save_restore_via_rz_device),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
