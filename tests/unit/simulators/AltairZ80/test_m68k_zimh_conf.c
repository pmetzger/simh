// Test AltairZ80's upstream Musashi configuration contract.
//
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#define MUSASHI_CNF "m68kconf_zimh.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include "m68k.h"

#if M68K_EMULATE_INT_ACK != M68K_OPT_SPECIFY_HANDLER
#error "AltairZ80 must use its interrupt acknowledge callback"
#endif

#if M68K_EMULATE_RESET != M68K_OPT_SPECIFY_HANDLER
#error "AltairZ80 must use its reset callback"
#endif

#if M68K_EMULATE_FC != M68K_OPT_SPECIFY_HANDLER
#error "AltairZ80 must use its function-code callback"
#endif

static uint_t last_read_address;
static uint_t last_write_address;
static uint_t last_write_value;
static uint_t last_function_code;
static int last_irq_level;
static int reset_pulses;

uint_t m68k_cpu_read_byte(uint_t address)
{
    last_read_address = address;
    return 0x12;
}

uint_t m68k_cpu_read_byte_raw(uint_t address)
{
    last_read_address = address;
    return 0x34;
}

uint_t m68k_cpu_read_word(uint_t address)
{
    last_read_address = address;
    return 0x5678;
}

uint_t m68k_cpu_read_long(uint_t address)
{
    last_read_address = address;
    return 0x9abcdef0u;
}

void m68k_cpu_write_byte(uint_t address, uint_t value)
{
    last_write_address = address;
    last_write_value = value;
}

void m68k_cpu_write_byte_raw(uint_t address, uint_t value)
{
    last_write_address = address;
    last_write_value = value;
}

void m68k_cpu_write_word(uint_t address, uint_t value)
{
    last_write_address = address;
    last_write_value = value;
}

void m68k_cpu_write_long(uint_t address, uint_t value)
{
    last_write_address = address;
    last_write_value = value;
}

void m68k_cpu_pulse_reset(void)
{
    ++reset_pulses;
}

void m68k_cpu_set_fc(uint_t fc)
{
    last_function_code = fc;
}

int m68k_cpu_irq_ack(int level)
{
    last_irq_level = level;
    return M68K_INT_ACK_AUTOVECTOR;
}

static void test_m68k_zimh_config_callbacks(void **state)
{
    (void)state;

    assert_int_equal(m68k_read_memory_8(0x1000), 0x12);
    assert_int_equal(last_read_address, 0x1000);

    assert_int_equal(m68k_read_memory_16(0x2000), 0x5678);
    assert_int_equal(last_read_address, 0x2000);

    assert_int_equal(m68k_read_memory_32(0x3000), 0x9abcdef0u);
    assert_int_equal(last_read_address, 0x3000);

    m68k_write_memory_8(0x4000, 0x56);
    assert_int_equal(last_write_address, 0x4000);
    assert_int_equal(last_write_value, 0x56);

    m68k_write_memory_16(0x5000, 0x789a);
    assert_int_equal(last_write_address, 0x5000);
    assert_int_equal(last_write_value, 0x789a);

    m68k_write_memory_32(0x6000, 0xbcdef012u);
    assert_int_equal(last_write_address, 0x6000);
    assert_int_equal(last_write_value, 0xbcdef012u);

    assert_int_equal(M68K_INT_ACK_CALLBACK(4), (int)M68K_INT_ACK_AUTOVECTOR);
    assert_int_equal(last_irq_level, 4);

    M68K_RESET_CALLBACK();
    assert_int_equal(reset_pulses, 1);

    M68K_SET_FC_CALLBACK(7);
    assert_int_equal(last_function_code, 7);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_m68k_zimh_config_callbacks),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
