// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_ether.h"
#include "sim_ether_test.h"

static DEVICE test_device = {.name = "TETH"};

static int callback_status;

/* Record the status supplied to an Ethernet read or write callback. */
static void record_callback_status(int status)
{
    callback_status = status;
}

/* Fill a minimum-sized IPv4-looking Ethernet frame for backend tests. */
static void fill_packet(uint8_t *packet, const ETH_MAC dst, const ETH_MAC src)
{
    memcpy(&packet[0], dst, sizeof(ETH_MAC));
    memcpy(&packet[6], src, sizeof(ETH_MAC));
    packet[12] = 0x08;
    packet[13] = 0x00;
    for (uint8_t i = 14; i < ETH_MIN_PACKET; i++)
        packet[i] = i;
}

/* Open a test Ethernet backend through the normal public attach path. */
static void open_test_device(ETH_DEV *dev, const char *name)
{
    assert_int_equal(eth_open(dev, name, &test_device, 0), SCPE_OK);
}

/* Close a test Ethernet backend through the normal public detach path. */
static void close_test_device(ETH_DEV *dev)
{
    assert_int_equal(eth_close(dev), SCPE_OK);
}

/* Verify injected packets are filtered, CRC-extended, and delivered. */
static void test_test_backend_delivers_injected_packets(void **state)
{
    static const ETH_MAC dst = {0x02, 0x00, 0xde, 0xad, 0xbe, 0xef};
    static const ETH_MAC src = {0x02, 0x00, 0xba, 0xdc, 0x0f, 0xfe};
    uint8_t packet[ETH_MIN_PACKET];
    ETH_PACK read_packet;
    ETH_DEV dev;

    (void)state;

    fill_packet(packet, dst, src);
    open_test_device(&dev, "test:sim-ether-rx");
    eth_setcrc(&dev, 1);
    assert_int_equal(eth_filter(&dev, 1, &dst, false, false), SCPE_OK);

    callback_status = -1;
    assert_int_equal(eth_test_inject("sim-ether-rx", packet, sizeof(packet)),
                     SCPE_OK);
    memset(&read_packet, 0, sizeof(read_packet));
    assert_int_equal(eth_read(&dev, &read_packet, record_callback_status), 1);

    assert_int_equal(callback_status, 0);
    assert_int_equal(read_packet.len, ETH_MIN_PACKET);
    assert_int_equal(read_packet.crc_len, ETH_MIN_PACKET + ETH_CRC_SIZE);
    assert_memory_equal(read_packet.msg, packet, sizeof(packet));
    assert_int_equal(eth_test_rx_count("sim-ether-rx"), 0);

    close_test_device(&dev);
    assert_int_equal(eth_test_clear("sim-ether-rx"), SCPE_OK);
}

/* Verify injected packets that miss the installed filter are discarded. */
static void test_test_backend_filters_unmatched_packets(void **state)
{
    static const ETH_MAC dst = {0x02, 0x00, 0xde, 0xad, 0xbe, 0xef};
    static const ETH_MAC other = {0x02, 0x00, 0x10, 0x20, 0x30, 0x40};
    static const ETH_MAC src = {0x02, 0x00, 0xba, 0xdc, 0x0f, 0xfe};
    uint8_t packet[ETH_MIN_PACKET];
    ETH_PACK read_packet;
    ETH_DEV dev;

    (void)state;

    fill_packet(packet, other, src);
    open_test_device(&dev, "test:sim-ether-filter");
    assert_int_equal(eth_filter(&dev, 1, &dst, false, false), SCPE_OK);

    assert_int_equal(
        eth_test_inject("sim-ether-filter", packet, sizeof(packet)), SCPE_OK);
    memset(&read_packet, 0, sizeof(read_packet));
    assert_int_equal(eth_read(&dev, &read_packet, NULL), 0);
    assert_int_equal(read_packet.len, 0);
    assert_int_equal(eth_test_rx_count("sim-ether-filter"), 0);

    close_test_device(&dev);
    assert_int_equal(eth_test_clear("sim-ether-filter"), SCPE_OK);
}

/* Verify writes to a test backend are captured for inspection. */
static void test_test_backend_captures_transmitted_packets(void **state)
{
    static const ETH_MAC dst = {0x02, 0x00, 0xde, 0xad, 0xbe, 0xef};
    static const ETH_MAC src = {0x02, 0x00, 0xba, 0xdc, 0x0f, 0xfe};
    ETH_PACK packet;
    ETH_PACK captured;
    ETH_DEV dev;

    (void)state;

    memset(&packet, 0, sizeof(packet));
    fill_packet(packet.msg, dst, src);
    packet.len = ETH_MIN_PACKET;

    open_test_device(&dev, "test:sim-ether-tx");
    callback_status = -1;
    assert_int_equal(eth_write(&dev, &packet, record_callback_status), SCPE_OK);
    assert_int_equal(callback_status, 0);
    assert_int_equal(eth_test_tx_count("sim-ether-tx"), 1);

    memset(&captured, 0, sizeof(captured));
    assert_int_equal(eth_test_pop_tx("sim-ether-tx", &captured), SCPE_OK);
    assert_int_equal(captured.len, ETH_MIN_PACKET);
    assert_memory_equal(captured.msg, packet.msg, ETH_MIN_PACKET);
    assert_int_equal(eth_test_pop_tx("sim-ether-tx", &captured), SCPE_EOF);

    close_test_device(&dev);
    assert_int_equal(eth_test_clear("sim-ether-tx"), SCPE_OK);
}

/* Verify tests can force write failure status without real I/O. */
static void test_test_backend_reports_configured_write_failure(void **state)
{
    static const ETH_MAC dst = {0x02, 0x00, 0xde, 0xad, 0xbe, 0xef};
    static const ETH_MAC src = {0x02, 0x00, 0xba, 0xdc, 0x0f, 0xfe};
    ETH_PACK packet;
    ETH_DEV dev;

    (void)state;

    memset(&packet, 0, sizeof(packet));
    fill_packet(packet.msg, dst, src);
    packet.len = ETH_MIN_PACKET;

    open_test_device(&dev, "test:sim-ether-tx-fail");
    assert_int_equal(eth_test_set_write_status("sim-ether-tx-fail", 1),
                     SCPE_OK);
    callback_status = 0;
    assert_int_equal(eth_write(&dev, &packet, record_callback_status),
                     SCPE_IOERR);
    assert_int_equal(callback_status, 1);
    assert_int_equal(eth_test_tx_count("sim-ether-tx-fail"), 1);

    close_test_device(&dev);
    assert_int_equal(eth_test_clear("sim-ether-tx-fail"), SCPE_OK);
}

/* Verify explicit pseudo-device names are matched case-insensitively. */
static void test_test_backend_opens_explicit_pseudo_name(void **state)
{
    ETH_DEV dev;

    (void)state;

    open_test_device(&dev, "TEST:sim-ether-explicit");
    assert_int_equal(dev.eth_api, ETH_API_TEST);

    close_test_device(&dev);
    assert_int_equal(eth_test_clear("sim-ether-explicit"), SCPE_OK);
}

/* Run all unit tests for the deterministic Ethernet backend. */
int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_test_backend_delivers_injected_packets),
        cmocka_unit_test(test_test_backend_filters_unmatched_packets),
        cmocka_unit_test(test_test_backend_captures_transmitted_packets),
        cmocka_unit_test(test_test_backend_reports_configured_write_failure),
        cmocka_unit_test(test_test_backend_opens_explicit_pseudo_name),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
