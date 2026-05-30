// Test AltairZ80's Musashi-facing simulator callbacks.
//
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include "test_cmocka.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MUSASHI_CNF "m68kconf_zimh.h"

#include "m68ksim.c"

uint32_t PCX;
uint32_t SIMHSleep;
uint32_t m68k_registers[M68K_REG_CPU_TYPE + 1];
UNIT cpu_unit;
uint32_t mmiobase;
uint32_t mmiosize;
uint32_t m68kvariant;

static uint32_t last_in_port;
static uint32_t in_value;
static uint32_t last_out_port;
static uint32_t last_out_value;
static unsigned int out_calls;

static int32_t hdsk_status;
static int32_t hdsk_selected_disk;
static int32_t hdsk_selected_sector;
static int32_t hdsk_selected_track;
static int32_t hdsk_selected_dma;
static bool hdsk_parameters_valid;
static unsigned int hdsk_prepare_read_calls;
static unsigned int hdsk_prepare_write_calls;
static unsigned int hdsk_read_calls;
static unsigned int hdsk_write_calls;
static unsigned int hdsk_flush_calls;

static uint_t stub_m68k_regs[M68K_REG_CPU_TYPE + 1];
static uint_t last_m68k_irq_level;
static uint_t last_cpu_type;
static unsigned int m68k_init_calls;
static unsigned int m68k_pulse_reset_calls;

void out(const uint32_t port, const uint32_t value)
{
    last_out_port = port;
    last_out_value = value;
    ++out_calls;
}

uint32_t in(const uint32_t port)
{
    last_in_port = port;
    return in_value;
}

void hdsk_prepareRead(void)
{
    ++hdsk_prepare_read_calls;
}

void hdsk_prepareWrite(void)
{
    ++hdsk_prepare_write_calls;
}

void hdsk_setSelectedDisk(const int32_t disk)
{
    hdsk_selected_disk = disk;
}

void hdsk_setSelectedSector(const int32_t sector)
{
    hdsk_selected_sector = sector;
}

void hdsk_setSelectedTrack(const int32_t track)
{
    hdsk_selected_track = track;
}

void hdsk_setSelectedDMA(const int32_t dma)
{
    hdsk_selected_dma = dma;
}

int32_t hdsk_getStatus(void)
{
    return hdsk_status;
}

bool hdsk_checkParameters(void)
{
    return hdsk_parameters_valid;
}

int32_t hdsk_read(void)
{
    ++hdsk_read_calls;
    return 0;
}

int32_t hdsk_write(void)
{
    ++hdsk_write_calls;
    return 0;
}

int32_t hdsk_flush(void)
{
    ++hdsk_flush_calls;
    return 0;
}

void m68k_set_cpu_type(uint_t cpu_type)
{
    last_cpu_type = cpu_type;
}

void m68k_init(void)
{
    ++m68k_init_calls;
}

void m68k_pulse_reset(void)
{
    ++m68k_pulse_reset_calls;
    m68k_cpu_pulse_reset();
}

int m68k_execute(int num_cycles)
{
    return num_cycles;
}

void m68k_set_irq(uint_t int_level)
{
    last_m68k_irq_level = int_level;
}

uint_t m68k_get_reg(void *context, m68k_register_t reg)
{
    (void)context;
    return stub_m68k_regs[reg];
}

void m68k_set_reg(m68k_register_t reg, uint_t value)
{
    stub_m68k_regs[reg] = value;
}

static void reset_callback_state(void)
{
    PCX = 0;
    SIMHSleep = 0;
    memset(m68k_registers, 0, sizeof(m68k_registers));
    memset(&cpu_unit, 0, sizeof(cpu_unit));
    mmiobase = 0;
    mmiosize = 0;
    m68kvariant = M68K_CPU_TYPE_68000;

    last_in_port = 0;
    in_value = 0;
    last_out_port = 0;
    last_out_value = 0;
    out_calls = 0;

    hdsk_status = 0;
    hdsk_selected_disk = 0;
    hdsk_selected_sector = 0;
    hdsk_selected_track = 0;
    hdsk_selected_dma = 0;
    hdsk_parameters_valid = true;
    hdsk_prepare_read_calls = 0;
    hdsk_prepare_write_calls = 0;
    hdsk_read_calls = 0;
    hdsk_write_calls = 0;
    hdsk_flush_calls = 0;

    memset(stub_m68k_regs, 0, sizeof(stub_m68k_regs));
    last_m68k_irq_level = 0;
    last_cpu_type = 0;
    m68k_init_calls = 0;
    m68k_pulse_reset_calls = 0;

    m68k_MC6850_control = 0;
    m68k_MC6850_status = 2;
    keyboardCharacter = 0;
    characterAvailable = false;
    m68k_int_controller_pending = 0;
    m68k_int_controller_highest_int = 0;
    m68k_fc = 0;

    stop_cpu = false;
    sim_interval = 0;
    m68k_clear_memory();
}

static int setup(void **state)
{
    (void)state;
    reset_callback_state();
    return 0;
}

static void test_ram_access_is_big_endian(void **state)
{
    (void)state;

    m68k_cpu_write_long(0x100, 0x12345678);
    assert_int_equal(m68k_cpu_read_byte_raw(0x100), 0x12);
    assert_int_equal(m68k_cpu_read_byte_raw(0x101), 0x34);
    assert_int_equal(m68k_cpu_read_byte_raw(0x102), 0x56);
    assert_int_equal(m68k_cpu_read_byte_raw(0x103), 0x78);
    assert_int_equal(m68k_cpu_read_long(0x100), 0x12345678);

    m68k_cpu_write_word(0x104, 0xabcd);
    assert_int_equal(m68k_cpu_read_byte_raw(0x104), 0xab);
    assert_int_equal(m68k_cpu_read_byte_raw(0x105), 0xcd);
    assert_int_equal(m68k_cpu_read_word(0x104), 0xabcd);

    m68k_cpu_write_byte(0x106, 0xef);
    assert_int_equal(m68k_cpu_read_byte_raw(0x106), 0xef);
}

static void test_ram_boundaries_return_default_values(void **state)
{
    (void)state;

    m68k_cpu_write_byte_raw(M68K_MAX_RAM, 0x5a);
    assert_int_equal(m68k_cpu_read_byte_raw(M68K_MAX_RAM), 0x5a);

    m68k_cpu_write_word(M68K_MAX_RAM - 1, 0x1234);
    assert_int_equal(m68k_cpu_read_word(M68K_MAX_RAM - 1), 0x1234);

    m68k_cpu_write_long(M68K_MAX_RAM - 3, 0x89abcdef);
    assert_int_equal(m68k_cpu_read_long(M68K_MAX_RAM - 3), 0x89abcdef);

    assert_int_equal(m68k_cpu_read_byte_raw(M68K_MAX_RAM + 1), 0xff);
    assert_int_equal(m68k_cpu_read_word(M68K_MAX_RAM), 0xffff);
    assert_int_equal(m68k_cpu_read_long(M68K_MAX_RAM - 2), 0xffffffff);

    m68k_cpu_write_byte_raw(M68K_MAX_RAM, 0xa5);
    m68k_cpu_write_word(M68K_MAX_RAM, 0x5678);
    assert_int_equal(m68k_cpu_read_byte_raw(M68K_MAX_RAM), 0xa5);
}

static void test_byte_mmio_uses_low_port_byte(void **state)
{
    (void)state;

    mmiobase = 0xf000;
    mmiosize = 0x100;
    in_value = 0x1ab;

    assert_int_equal(m68k_cpu_read_byte(0xf012), 0xab);
    assert_int_equal(last_in_port, 0x12);

    m68k_cpu_write_byte(0xf034, 0x2cd);
    assert_int_equal(out_calls, 1);
    assert_int_equal(last_out_port, 0x34);
    assert_int_equal(last_out_value, 0xcd);
}

static void test_disk_registers_dispatch_long_access(void **state)
{
    (void)state;

    hdsk_status = 0x1357;
    assert_int_equal(m68k_cpu_read_word(DISK_STATUS), 0x1357);
    assert_int_equal(m68k_cpu_read_long(DISK_STATUS), 0x1357);

    m68k_cpu_write_long(DISK_SET_DRIVE, 3);
    assert_int_equal(hdsk_selected_disk, 3);

    m68k_cpu_write_long(DISK_SET_DMA, 0x4000);
    assert_int_equal(hdsk_selected_dma, 0x4000);

    m68k_cpu_write_long(DISK_SET_SECTOR, 42);
    assert_int_equal(hdsk_selected_sector, 42);

    m68k_cpu_write_long(DISK_READ, 77);
    assert_int_equal(hdsk_selected_sector, 77);
    assert_int_equal(hdsk_selected_track, 0);
    assert_int_equal(hdsk_prepare_read_calls, 1);
    assert_int_equal(hdsk_read_calls, 1);

    m68k_cpu_write_long(DISK_WRITE, 88);
    assert_int_equal(hdsk_selected_sector, 88);
    assert_int_equal(hdsk_selected_track, 0);
    assert_int_equal(hdsk_prepare_write_calls, 1);
    assert_int_equal(hdsk_write_calls, 1);

    m68k_cpu_write_long(DISK_FLUSH, 0);
    assert_int_equal(hdsk_flush_calls, 1);

    assert_false(stop_cpu);
    m68k_cpu_write_long(M68K_STOP_CPU, 0);
    assert_true(stop_cpu);
}

static void test_reset_function_code_and_irq_callbacks(void **state)
{
    (void)state;

    m68k_MC6850_control = 0xff;
    m68k_MC6850_status = 0xff;
    characterAvailable = true;
    int_controller_set(IRQ_MC6850);
    assert_int_equal(last_m68k_irq_level, IRQ_MC6850);

    m68k_cpu_pulse_reset();
    assert_int_equal(m68k_MC6850_control, 0);
    assert_int_equal(m68k_MC6850_status, 2);
    assert_false(characterAvailable);
    assert_int_equal(m68k_int_controller_pending, 0);
    assert_int_equal(last_m68k_irq_level, 0);

    m68k_cpu_set_fc(5);
    assert_int_equal(m68k_fc, 5);

    assert_int_equal(m68k_cpu_irq_ack(IRQ_MC6850),
                     (int)M68K_INT_ACK_AUTOVECTOR);

    int_controller_set(IRQ_NMI_DEVICE);
    assert_int_equal(last_m68k_irq_level, IRQ_NMI_DEVICE);
    assert_int_equal(m68k_cpu_irq_ack(IRQ_NMI_DEVICE),
                     (int)M68K_INT_ACK_AUTOVECTOR);
    assert_int_equal(m68k_int_controller_pending, 0);
    assert_int_equal(last_m68k_irq_level, 0);

    assert_int_equal(m68k_cpu_irq_ack(1), (int)M68K_INT_ACK_SPURIOUS);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_ram_access_is_big_endian, setup),
        cmocka_unit_test_setup(test_ram_boundaries_return_default_values,
                               setup),
        cmocka_unit_test_setup(test_byte_mmio_uses_low_port_byte, setup),
        cmocka_unit_test_setup(test_disk_registers_dispatch_long_access, setup),
        cmocka_unit_test_setup(test_reset_function_code_and_irq_callbacks,
                               setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
