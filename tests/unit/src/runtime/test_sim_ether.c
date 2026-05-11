#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_ether.h"

t_stat eth_test_dev_command_format(void);

/* Assert that a generated MAC address preserved the requested prefix bits. */
static void assert_eth_mac_prefix(const ETH_MAC actual,
                                  const ETH_MAC expected,
                                  uint32_t prefix_bits)
{
    const uint32_t full_bytes = prefix_bits / 8;
    const uint32_t partial_bits = prefix_bits % 8;
    uint32_t i;

    for (i = 0; i < full_bytes; ++i)
        assert_int_equal(actual[i], expected[i]);
    if (partial_bits != 0) {
        const uint8_t mask = (uint8_t)(0xffu << (8 - partial_bits));

        assert_int_equal(actual[full_bytes] & mask,
                         expected[full_bytes] & mask);
    }
}

/* Verify generated MAC addresses preserve the requested fixed prefix bits. */
static void test_eth_mac_scan_generated_prefix_lengths(void **state)
{
    static const ETH_MAC base_mac = {0x02, 0x84, 0x86, 0x08, 0x0a, 0x0c};
    char input[sizeof("02:84:86:08:0A:0C/48")];
    ETH_MAC mac;
    uint32_t prefix_bits;

    (void)state;

    for (prefix_bits = 16; prefix_bits <= 48; ++prefix_bits) {
        assert_true(snprintf(input, sizeof(input),
                             "02:84:86:08:0A:0C/%u",
                             prefix_bits) < (int)sizeof(input));
        assert_int_equal(eth_mac_scan(mac, input), SCPE_OK);
        assert_eth_mac_prefix(mac, base_mac, prefix_bits);
    }
}

static void test_eth_mac_fmt_formats_address(void **state)
{
    static const ETH_MAC mac = {0x02, 0x84, 0x86, 0x08, 0x0a, 0x0c};
    char buffer[ETH_MAC_STRING_SIZE];

    (void)state;

    eth_mac_fmt(mac, buffer, sizeof(buffer));
    assert_string_equal(buffer, "02:84:86:08:0A:0C");
}

static void test_eth_mac_fmt_truncates_to_buffer_size(void **state)
{
    static const ETH_MAC mac = {0x02, 0x84, 0x86, 0x08, 0x0a, 0x0c};
    char buffer[8];

    (void)state;

    memset(buffer, 'x', sizeof(buffer));
    eth_mac_fmt(mac, buffer, sizeof(buffer));
    assert_string_equal(buffer, "02:84:8");
    assert_int_equal(buffer[sizeof(buffer) - 1], '\0');
}

static void test_eth_dev_command_formatting(void **state)
{
    (void)state;

    assert_int_equal(eth_test_dev_command_format(), SCPE_OK);
}

static void test_eth_reader_thread_async_flag_is_bool(void **state)
{
    ETH_DEV dev = {0};

    (void)state;

    assert_false(dev.asynch_io);
    dev.asynch_io = true;
    assert_true(dev.asynch_io);
    dev.asynch_io = false;
    assert_false(dev.asynch_io);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_eth_mac_scan_generated_prefix_lengths),
        cmocka_unit_test(test_eth_mac_fmt_formats_address),
        cmocka_unit_test(test_eth_mac_fmt_truncates_to_buffer_size),
        cmocka_unit_test(test_eth_dev_command_formatting),
        cmocka_unit_test(test_eth_reader_thread_async_flag_is_bool),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
