// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "test_cmocka.h"

#include "sim_defs.h"
#include "sim_ether_test.h"
#include "vax_defs.h"
#include "vax_xs.h"

#define TEST_INIT_BLOCK 0x1e4u
#define TEST_DMA_SIZE 0x1000u

#define TEST_CSR0_INIT 0x0001u
#define TEST_CSR0_STRT 0x0002u
#define TEST_CSR0_STOP 0x0004u
#define TEST_CSR0_TXON 0x0010u
#define TEST_CSR0_RXON 0x0020u
#define TEST_CSR0_INEA 0x0040u
#define TEST_CSR0_INTR 0x0080u
#define TEST_CSR0_IDON 0x0100u
#define TEST_CSR0_TINT 0x0200u
#define TEST_CSR0_RINT 0x0400u
#define TEST_CSR0_MERR 0x0800u
#define TEST_CSR0_MISS 0x1000u
#define TEST_CSR0_BABL 0x4000u

#define TEST_MODE_PROM 0x8000u
#define TEST_MODE_INTL 0x0040u
#define TEST_MODE_LOOP 0x0004u
#define TEST_MODE_DTX 0x0002u
#define TEST_MODE_DRX 0x0001u

#define TEST_TXR_OWN 0x8000u
#define TEST_TXR_ERRS 0x4000u
#define TEST_TXR_STF 0x0200u
#define TEST_TXR_ENF 0x0100u
#define TEST_TXR_BUFL 0x8000u
#define TEST_TXR_RTRY 0x0400u

#define TEST_RXR_OWN 0x8000u
#define TEST_RXR_ERRS 0x4000u
#define TEST_RXR_BUFL 0x0400u
#define TEST_RXR_STF 0x0200u
#define TEST_RXR_ENF 0x0100u
#define TEST_RXR_MLEN 0x0fffu

#define TEST_RX_RING 0x0020u
#define TEST_TX_RING 0x0120u
#define TEST_RX_BUF 0x0200u
#define TEST_TX_BUF 0x0300u
#define TEST_PACKET_LEN 60u

static const ETH_MAC test_mac = {0x08, 0x00, 0x00, 0x2b, 0x02, 0x01};
static const ETH_MAC test_peer_mac = {0x02, 0x00, 0xca, 0xfe, 0x00, 0x01};

static uint8_t test_dma[TEST_DMA_SIZE];

uint32_t fault_PC;
int32_t int_req[IPL_HLVL];
int32_t tmxr_poll;

/* Store one little-endian word in the fake DMA address space. */
static void dma_store_word(uint32_t addr, uint16_t value)
{
    test_dma[addr] = value & 0xff;
    test_dma[addr + 1] = value >> 8;
}

/* Load one little-endian word from the fake DMA address space. */
static uint16_t dma_load_word(uint32_t addr)
{
    return (uint16_t)(test_dma[addr] | (test_dma[addr + 1] << 8));
}

/* Store one byte in the fake DMA address space. */
static void dma_store_byte(uint32_t addr, uint8_t value)
{
    test_dma[addr] = value;
}

/* Populate a LANCE initialization block in fake DMA memory. */
static void set_init_block(uint16_t mode, uint16_t rx_ring, uint16_t tx_ring)
{
    dma_store_word(TEST_INIT_BLOCK, mode);
    dma_store_word(TEST_INIT_BLOCK + 0x2, 0x0008);
    dma_store_word(TEST_INIT_BLOCK + 0x4, 0x2b00);
    dma_store_word(TEST_INIT_BLOCK + 0x6, 0x0102);
    dma_store_word(TEST_INIT_BLOCK + 0x10, rx_ring);
    dma_store_word(TEST_INIT_BLOCK + 0x12, 0x0000);
    dma_store_word(TEST_INIT_BLOCK + 0x14, tx_ring);
    dma_store_word(TEST_INIT_BLOCK + 0x16, 0x0000);
}

/* Set the descriptor ring sizes in the fake LANCE initialization block. */
static void set_ring_length_exponents(uint16_t rx_len_log2,
                                      uint16_t tx_len_log2)
{
    dma_store_word(TEST_INIT_BLOCK + 0x12, (rx_len_log2 & 0x7u) << 13);
    dma_store_word(TEST_INIT_BLOCK + 0x16, (tx_len_log2 & 0x7u) << 13);
}

/* Populate one transmit descriptor owned by the LANCE. */
static void write_tx_descriptor(uint16_t ring, uint16_t buffer, uint16_t len)
{
    dma_store_word(ring, buffer);
    dma_store_word(ring + 2, TEST_TXR_OWN | TEST_TXR_STF | TEST_TXR_ENF);
    dma_store_word(ring + 4, (uint16_t)-len);
    dma_store_word(ring + 6, 0x0000);
}

/* Populate one receive descriptor owned by the LANCE. */
static void write_rx_descriptor(uint16_t ring, uint16_t buffer, uint16_t len)
{
    dma_store_word(ring, buffer);
    dma_store_word(ring + 2, TEST_RXR_OWN);
    dma_store_word(ring + 4, (uint16_t)-len);
    dma_store_word(ring + 6, 0x0000);
}

/* Fill fake DMA memory with an Ethernet frame using explicit addresses. */
static void fill_packet_with_macs(uint16_t buffer, uint16_t len,
                                  const ETH_MAC dst, const ETH_MAC src)
{
    memcpy(&test_dma[buffer], dst, sizeof(ETH_MAC));
    memcpy(&test_dma[buffer + 6], src, sizeof(ETH_MAC));
    test_dma[buffer + 12] = 0x08;
    test_dma[buffer + 13] = 0x00;
    for (uint16_t i = 0; i < len; i++)
        if (i >= 14)
            dma_store_byte(buffer + i, (uint8_t)(0xa0u + i));
}

/* Fill fake DMA memory with an Ethernet frame addressed to the test MAC. */
static void fill_packet(uint16_t buffer, uint16_t len)
{
    fill_packet_with_macs(buffer, len, test_mac, test_peer_mac);
}

/* Select and write one LANCE CSR through the XS register interface. */
static void write_xs_csr(uint16_t csr, uint16_t value)
{
    xs_wr(XSBASE + 4, csr, L_WORD);
    xs_wr(XSBASE, value, L_WORD);
}

/* Select and read one LANCE CSR through the XS register interface. */
static uint16_t read_xs_csr(uint16_t csr)
{
    xs_wr(XSBASE + 4, csr, L_WORD);
    return (uint16_t)xs_rd(XSBASE);
}

/* Run one XS service pass exactly as the simulator scheduler would. */
static void run_xs_service(void)
{
    assert_non_null(xs_dev.units);
    assert_non_null(xs_dev.units[0].action);
    assert_int_equal(xs_dev.units[0].action(&xs_dev.units[0]), SCPE_OK);
}

/* Reset the XS device and fake host state to a clean probe baseline. */
static void reset_probe_state(void)
{
    memset(test_dma, 0, sizeof(test_dma));
    memset(int_req, 0, sizeof(int_req));
    fault_PC = 0;
    tmxr_poll = 0;

    set_init_block(0, TEST_RX_RING, TEST_TX_RING);

    assert_non_null(xs_dev.reset);
    assert_int_equal(xs_dev.reset(&xs_dev), SCPE_OK);
}

/* Initialize the LANCE, acknowledge IDON, and start packet processing. */
static void initialize_and_start_lance_with_ring_lengths(uint16_t mode,
                                                         uint16_t rx_len_log2,
                                                         uint16_t tx_len_log2)
{
    set_init_block(mode, TEST_RX_RING, TEST_TX_RING);
    set_ring_length_exponents(rx_len_log2, tx_len_log2);

    write_xs_csr(1, TEST_INIT_BLOCK);
    write_xs_csr(2, 0x0000);
    write_xs_csr(0, TEST_CSR0_INIT | TEST_CSR0_INEA);
    run_xs_service();
    write_xs_csr(0, TEST_CSR0_IDON);
    int_req[IPL_XS1] &= ~INT_XS1;
    write_xs_csr(0, TEST_CSR0_STRT | TEST_CSR0_INEA);
}

/* Initialize the LANCE with one-entry receive and transmit rings. */
static void initialize_and_start_lance(uint16_t mode)
{
    initialize_and_start_lance_with_ring_lengths(mode, 0, 0);
}

/* Attach the XS controller to a named deterministic Ethernet backend. */
static void attach_test_backend(const char *name)
{
    assert_non_null(xs_dev.attach);
    assert_int_equal(xs_dev.attach(&xs_dev.units[0], name), SCPE_OK);
}

/* Detach the XS controller and clear the named deterministic backend. */
static void detach_test_backend(const char *name)
{
    assert_non_null(xs_dev.detach);
    assert_int_equal(xs_dev.detach(&xs_dev.units[0]), SCPE_OK);
    assert_int_equal(eth_test_clear(name), SCPE_OK);
}

/*
 * NetBSD's vsbus probe samples the KA interrupt request register after the
 * LANCE match routine stops the chip.  The KA request bit must remain latched
 * until the bus-level interrupt-clear path consumes it.
 */
static void test_probe_stop_preserves_pending_bus_interrupt(void **state)
{
    (void)state;

    reset_probe_state();

    write_xs_csr(0, 0x0004);
    write_xs_csr(1, TEST_INIT_BLOCK);
    write_xs_csr(2, 0x0000);
    write_xs_csr(0, 0x0041);
    run_xs_service();

    assert_int_equal(read_xs_csr(0) & 0x0100, 0x0100);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);

    write_xs_csr(0, 0x0004);

    assert_int_equal(read_xs_csr(0) & 0x0100, 0x0000);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);
}

/* Verify reset leaves the LANCE in STOP state as documented. */
static void test_reset_sets_lance_stop(void **state)
{
    (void)state;

    reset_probe_state();

    assert_int_equal(read_xs_csr(0), TEST_CSR0_STOP);
}

/* Verify STOP clears LANCE state without clearing the KA interrupt latch. */
static void test_stop_resets_lance_register_state(void **state)
{
    (void)state;

    reset_probe_state();

    write_xs_csr(1, TEST_INIT_BLOCK);
    write_xs_csr(2, 0x0000);
    write_xs_csr(3, 0xffff);
    write_xs_csr(0, TEST_CSR0_INIT | TEST_CSR0_INEA);
    run_xs_service();

    assert_int_equal(read_xs_csr(0) & TEST_CSR0_IDON, TEST_CSR0_IDON);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);

    write_xs_csr(0, TEST_CSR0_STOP | TEST_CSR0_INIT | TEST_CSR0_STRT |
                        TEST_CSR0_INEA);

    assert_int_equal(read_xs_csr(0), TEST_CSR0_STOP);
    assert_int_equal(read_xs_csr(3), 0x0000);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);
}

/* Verify CSR1-CSR3 writes are ignored while the LANCE is running. */
static void test_csr_address_writes_require_stop(void **state)
{
    (void)state;

    reset_probe_state();

    write_xs_csr(1, TEST_INIT_BLOCK);
    write_xs_csr(2, 0x0000);
    write_xs_csr(0, TEST_CSR0_INIT);
    run_xs_service();

    assert_int_equal(read_xs_csr(0) & TEST_CSR0_STOP, 0x0000);

    write_xs_csr(1, 0x0320);
    write_xs_csr(2, 0x0001);
    write_xs_csr(0, TEST_CSR0_STOP);

    assert_int_equal(read_xs_csr(1), TEST_INIT_BLOCK);
    assert_int_equal(read_xs_csr(2), 0x0000);
}

/* Verify reserved bits in CSR1-CSR3 read back as zero. */
static void test_csr_reserved_bits_read_as_zero(void **state)
{
    (void)state;

    reset_probe_state();

    write_xs_csr(1, 0xffff);
    write_xs_csr(2, 0xffff);
    write_xs_csr(3, 0xffff);

    assert_int_equal(read_xs_csr(1), 0xfffe);
    assert_int_equal(read_xs_csr(2), 0x00ff);
    assert_int_equal(read_xs_csr(3), 0x0007);
}

/* Verify INEA cannot be set while stopped except with INIT or STRT. */
static void test_inea_requires_active_init_or_start(void **state)
{
    (void)state;

    reset_probe_state();

    write_xs_csr(0, TEST_CSR0_INEA);
    assert_int_equal(read_xs_csr(0), TEST_CSR0_STOP);

    write_xs_csr(1, TEST_INIT_BLOCK);
    write_xs_csr(2, 0x0000);
    write_xs_csr(0, TEST_CSR0_INIT | TEST_CSR0_INEA);
    run_xs_service();

    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INEA, TEST_CSR0_INEA);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);
}

/* Verify an uncleared LANCE interrupt can relatch the KA request bit. */
static void test_active_lance_interrupt_relatches_bus_request(void **state)
{
    (void)state;

    reset_probe_state();

    write_xs_csr(1, TEST_INIT_BLOCK);
    write_xs_csr(2, 0x0000);
    write_xs_csr(0, TEST_CSR0_INIT | TEST_CSR0_INEA);
    run_xs_service();

    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);

    int_req[IPL_XS1] &= ~INT_XS1;

    write_xs_csr(0, TEST_CSR0_INEA);

    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);
}

/* Verify failed initialization DMA sets the documented memory error state. */
static void test_initialization_memory_error_sets_status(void **state)
{
    (void)state;

    reset_probe_state();

    write_xs_csr(1, TEST_DMA_SIZE + 0x100u);
    write_xs_csr(2, 0x0000);
    write_xs_csr(0, TEST_CSR0_INIT | TEST_CSR0_INEA);

    assert_int_equal(read_xs_csr(0) & TEST_CSR0_MERR, TEST_CSR0_MERR);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_IDON, TEST_CSR0_IDON);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);
}

/* Verify STRT enables transmit and receive state after initialization. */
static void test_start_enables_transmitter_and_receiver(void **state)
{
    (void)state;

    reset_probe_state();

    write_xs_csr(1, TEST_INIT_BLOCK);
    write_xs_csr(2, 0x0000);
    write_xs_csr(0, TEST_CSR0_INIT | TEST_CSR0_INEA);
    run_xs_service();
    write_xs_csr(0, TEST_CSR0_IDON);

    write_xs_csr(0, TEST_CSR0_STRT | TEST_CSR0_INEA);

    assert_int_equal(read_xs_csr(0) & TEST_CSR0_STRT, TEST_CSR0_STRT);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_RXON, TEST_CSR0_RXON);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_TXON, TEST_CSR0_TXON);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_STOP, 0x0000);
}

/* Verify one transmit descriptor completes and interrupts in loopback mode. */
static void test_transmit_descriptor_completes_and_interrupts(void **state)
{
    uint16_t tx_status;

    (void)state;

    reset_probe_state();
    set_init_block(TEST_MODE_LOOP | TEST_MODE_INTL | TEST_MODE_DRX,
                   TEST_RX_RING, TEST_TX_RING);
    fill_packet(TEST_TX_BUF, TEST_PACKET_LEN);
    write_tx_descriptor(TEST_TX_RING, TEST_TX_BUF, TEST_PACKET_LEN);

    write_xs_csr(1, TEST_INIT_BLOCK);
    write_xs_csr(2, 0x0000);
    write_xs_csr(0, TEST_CSR0_INIT | TEST_CSR0_INEA);
    run_xs_service();
    write_xs_csr(0, TEST_CSR0_IDON);
    int_req[IPL_XS1] &= ~INT_XS1;
    write_xs_csr(0, TEST_CSR0_STRT | TEST_CSR0_INEA);

    run_xs_service();

    tx_status = dma_load_word(TEST_TX_RING + 2);
    assert_int_equal(tx_status & TEST_TXR_OWN, 0x0000);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_TINT, TEST_CSR0_TINT);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);
}

/* Verify internal loopback delivers the frame through the receive ring. */
static void test_internal_loopback_completes_receive_descriptor(void **state)
{
    uint16_t rx_status;
    uint16_t rx_length;

    (void)state;

    reset_probe_state();
    set_init_block(TEST_MODE_LOOP | TEST_MODE_INTL, TEST_RX_RING, TEST_TX_RING);
    fill_packet(TEST_TX_BUF, TEST_PACKET_LEN);
    write_rx_descriptor(TEST_RX_RING, TEST_RX_BUF, 1536);
    write_tx_descriptor(TEST_TX_RING, TEST_TX_BUF, TEST_PACKET_LEN);

    write_xs_csr(1, TEST_INIT_BLOCK);
    write_xs_csr(2, 0x0000);
    write_xs_csr(0, TEST_CSR0_INIT | TEST_CSR0_INEA);
    run_xs_service();
    write_xs_csr(0, TEST_CSR0_IDON);
    int_req[IPL_XS1] &= ~INT_XS1;
    write_xs_csr(0, TEST_CSR0_STRT | TEST_CSR0_INEA);

    run_xs_service();
    int_req[IPL_XS1] &= ~INT_XS1;
    run_xs_service();

    rx_status = dma_load_word(TEST_RX_RING + 2);
    rx_length = dma_load_word(TEST_RX_RING + 6) & TEST_RXR_MLEN;
    assert_int_equal(rx_status & TEST_RXR_OWN, 0x0000);
    assert_int_equal(rx_status & TEST_RXR_STF, TEST_RXR_STF);
    assert_int_equal(rx_status & TEST_RXR_ENF, TEST_RXR_ENF);
    assert_int_equal(rx_length, TEST_PACKET_LEN + 4);
    assert_memory_equal(&test_dma[TEST_RX_BUF], &test_dma[TEST_TX_BUF],
                        TEST_PACKET_LEN);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);
}

/* Verify external backend receive flows through eth_read and XS rings. */
static void test_external_receive_uses_ethernet_backend(void **state)
{
    uint8_t packet[TEST_PACKET_LEN];
    uint16_t rx_status;
    uint16_t rx_length;

    (void)state;

    reset_probe_state();
    attach_test_backend("test:vax-xs-rx");
    write_rx_descriptor(TEST_RX_RING, TEST_RX_BUF, 1536);
    fill_packet(TEST_TX_BUF, TEST_PACKET_LEN);
    memcpy(packet, &test_dma[TEST_TX_BUF], sizeof(packet));

    initialize_and_start_lance(0);

    assert_int_equal(eth_test_inject("vax-xs-rx", packet, sizeof(packet)),
                     SCPE_OK);
    run_xs_service();

    rx_status = dma_load_word(TEST_RX_RING + 2);
    rx_length = dma_load_word(TEST_RX_RING + 6) & TEST_RXR_MLEN;
    assert_int_equal(rx_status & TEST_RXR_OWN, 0x0000);
    assert_int_equal(rx_status & TEST_RXR_STF, TEST_RXR_STF);
    assert_int_equal(rx_status & TEST_RXR_ENF, TEST_RXR_ENF);
    assert_int_equal(rx_length, TEST_PACKET_LEN + ETH_CRC_SIZE);
    assert_memory_equal(&test_dma[TEST_RX_BUF], packet, TEST_PACKET_LEN);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_RINT, TEST_CSR0_RINT);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);

    detach_test_backend("vax-xs-rx");
}

/* Verify receive frames can span chained descriptors. */
static void test_external_receive_chains_descriptors(void **state)
{
    uint8_t packet[TEST_PACKET_LEN];
    uint16_t first_status;
    uint16_t second_status;
    uint16_t second_length;
    const uint16_t second_rx_ring = TEST_RX_RING + 8;
    const uint16_t second_rx_buf = TEST_RX_BUF + 0x80;

    (void)state;

    reset_probe_state();
    attach_test_backend("test:vax-xs-rx-chain");
    write_rx_descriptor(TEST_RX_RING, TEST_RX_BUF, 32);
    write_rx_descriptor(second_rx_ring, second_rx_buf, 1536);
    fill_packet(TEST_TX_BUF, TEST_PACKET_LEN);
    memcpy(packet, &test_dma[TEST_TX_BUF], sizeof(packet));

    initialize_and_start_lance_with_ring_lengths(0, 1, 0);

    assert_int_equal(eth_test_inject("vax-xs-rx-chain", packet, sizeof(packet)),
                     SCPE_OK);
    run_xs_service();

    first_status = dma_load_word(TEST_RX_RING + 2);
    second_status = dma_load_word(second_rx_ring + 2);
    second_length = dma_load_word(second_rx_ring + 6) & TEST_RXR_MLEN;

    assert_int_equal(first_status & TEST_RXR_OWN, 0x0000);
    assert_int_equal(first_status & TEST_RXR_STF, TEST_RXR_STF);
    assert_int_equal(first_status & TEST_RXR_ENF, 0x0000);
    assert_int_equal(second_status & TEST_RXR_OWN, 0x0000);
    assert_int_equal(second_status & TEST_RXR_STF, 0x0000);
    assert_int_equal(second_status & TEST_RXR_ENF, TEST_RXR_ENF);
    assert_int_equal(second_length, TEST_PACKET_LEN + ETH_CRC_SIZE);
    assert_memory_equal(&test_dma[TEST_RX_BUF], packet, 32);
    assert_memory_equal(&test_dma[second_rx_buf], &packet[32],
                        TEST_PACKET_LEN - 32);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_RINT, TEST_CSR0_RINT);

    detach_test_backend("vax-xs-rx-chain");
}

/* Verify no next receive descriptor reports a LANCE buffer error. */
static void
test_external_receive_missing_chain_reports_buffer_error(void **state)
{
    uint8_t packet[TEST_PACKET_LEN];
    uint16_t first_status;

    (void)state;

    reset_probe_state();
    attach_test_backend("test:vax-xs-rx-miss");
    write_rx_descriptor(TEST_RX_RING, TEST_RX_BUF, 32);
    fill_packet(TEST_TX_BUF, TEST_PACKET_LEN);
    memcpy(packet, &test_dma[TEST_TX_BUF], sizeof(packet));

    initialize_and_start_lance_with_ring_lengths(0, 1, 0);

    assert_int_equal(eth_test_inject("vax-xs-rx-miss", packet, sizeof(packet)),
                     SCPE_OK);
    run_xs_service();

    first_status = dma_load_word(TEST_RX_RING + 2);
    assert_int_equal(first_status & TEST_RXR_OWN, 0x0000);
    assert_int_equal(first_status & TEST_RXR_ERRS, TEST_RXR_ERRS);
    assert_int_equal(first_status & TEST_RXR_BUFL, TEST_RXR_BUFL);
    assert_int_equal(first_status & TEST_RXR_STF, TEST_RXR_STF);
    assert_int_equal(first_status & TEST_RXR_ENF, 0x0000);
    assert_memory_equal(&test_dma[TEST_RX_BUF], packet, 32);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_MISS, 0x0000);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_RINT, TEST_CSR0_RINT);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);

    detach_test_backend("vax-xs-rx-miss");
}

/* Verify the installed Ethernet filter rejects frames for other addresses. */
static void test_external_receive_filters_unmatched_address(void **state)
{
    static const ETH_MAC other_mac = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t packet[TEST_PACKET_LEN];
    uint16_t rx_status;

    (void)state;

    reset_probe_state();
    attach_test_backend("test:vax-xs-rx-filter");
    write_rx_descriptor(TEST_RX_RING, TEST_RX_BUF, 1536);
    fill_packet_with_macs(TEST_TX_BUF, TEST_PACKET_LEN, other_mac,
                          test_peer_mac);
    memcpy(packet, &test_dma[TEST_TX_BUF], sizeof(packet));

    initialize_and_start_lance(0);

    assert_int_equal(
        eth_test_inject("vax-xs-rx-filter", packet, sizeof(packet)), SCPE_OK);
    run_xs_service();

    rx_status = dma_load_word(TEST_RX_RING + 2);
    assert_int_equal(rx_status & TEST_RXR_OWN, TEST_RXR_OWN);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_RINT, 0x0000);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, 0x0000);

    detach_test_backend("vax-xs-rx-filter");
}

/* Verify promiscuous mode accepts frames for unmatched unicast addresses. */
static void test_external_receive_promiscuous_accepts_unmatched(void **state)
{
    static const ETH_MAC other_mac = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t packet[TEST_PACKET_LEN];
    uint16_t rx_status;

    (void)state;

    reset_probe_state();
    attach_test_backend("test:vax-xs-rx-promisc");
    write_rx_descriptor(TEST_RX_RING, TEST_RX_BUF, 1536);
    fill_packet_with_macs(TEST_TX_BUF, TEST_PACKET_LEN, other_mac,
                          test_peer_mac);
    memcpy(packet, &test_dma[TEST_TX_BUF], sizeof(packet));

    initialize_and_start_lance(TEST_MODE_PROM);

    assert_int_equal(
        eth_test_inject("vax-xs-rx-promisc", packet, sizeof(packet)), SCPE_OK);
    run_xs_service();

    rx_status = dma_load_word(TEST_RX_RING + 2);
    assert_int_equal(rx_status & TEST_RXR_OWN, 0x0000);
    assert_int_equal(rx_status & TEST_RXR_STF, TEST_RXR_STF);
    assert_int_equal(rx_status & TEST_RXR_ENF, TEST_RXR_ENF);
    assert_memory_equal(&test_dma[TEST_RX_BUF], packet, TEST_PACKET_LEN);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_RINT, TEST_CSR0_RINT);

    detach_test_backend("vax-xs-rx-promisc");
}

/* Verify external transmit flows through eth_write into the backend. */
static void test_external_transmit_uses_ethernet_backend(void **state)
{
    ETH_PACK packet;
    uint16_t tx_status;

    (void)state;

    reset_probe_state();
    attach_test_backend("test:vax-xs-tx");
    fill_packet(TEST_TX_BUF, TEST_PACKET_LEN);
    write_tx_descriptor(TEST_TX_RING, TEST_TX_BUF, TEST_PACKET_LEN);

    initialize_and_start_lance(TEST_MODE_DRX);

    run_xs_service();

    tx_status = dma_load_word(TEST_TX_RING + 2);
    assert_int_equal(tx_status & TEST_TXR_OWN, 0x0000);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_TINT, TEST_CSR0_TINT);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);
    assert_int_equal(int_req[IPL_XS1] & INT_XS1, INT_XS1);

    memset(&packet, 0, sizeof(packet));
    assert_int_equal(eth_test_pop_tx("vax-xs-tx", &packet), SCPE_OK);
    assert_int_equal(packet.len, TEST_PACKET_LEN);
    assert_memory_equal(packet.msg, &test_dma[TEST_TX_BUF], TEST_PACKET_LEN);
    assert_int_equal(eth_test_pop_tx("vax-xs-tx", &packet), SCPE_EOF);

    detach_test_backend("vax-xs-tx");
}

/* Verify external transmit failures update CSR and descriptor error bits. */
static void test_external_transmit_failure_sets_error_status(void **state)
{
    uint16_t tx_status;
    uint16_t tx_error;

    (void)state;

    reset_probe_state();
    attach_test_backend("test:vax-xs-tx-fail");
    assert_int_equal(eth_test_set_write_status("vax-xs-tx-fail", 1), SCPE_OK);
    fill_packet(TEST_TX_BUF, TEST_PACKET_LEN);
    write_tx_descriptor(TEST_TX_RING, TEST_TX_BUF, TEST_PACKET_LEN);

    initialize_and_start_lance(TEST_MODE_DRX);

    run_xs_service();

    tx_status = dma_load_word(TEST_TX_RING + 2);
    tx_error = dma_load_word(TEST_TX_RING + 6);
    assert_int_equal(tx_status & TEST_TXR_OWN, 0x0000);
    assert_int_equal(tx_status & TEST_TXR_ERRS, TEST_TXR_ERRS);
    assert_int_equal(tx_error & TEST_TXR_RTRY, TEST_TXR_RTRY);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_BABL, TEST_CSR0_BABL);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_TINT, TEST_CSR0_TINT);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_INTR, TEST_CSR0_INTR);

    detach_test_backend("vax-xs-tx-fail");
}

/* Verify DTX mode prevents transmit descriptor processing. */
static void
test_external_transmit_disabled_leaves_descriptor_owned(void **state)
{
    uint16_t tx_status;

    (void)state;

    reset_probe_state();
    attach_test_backend("test:vax-xs-tx-disabled");
    fill_packet(TEST_TX_BUF, TEST_PACKET_LEN);
    write_tx_descriptor(TEST_TX_RING, TEST_TX_BUF, TEST_PACKET_LEN);

    initialize_and_start_lance(TEST_MODE_DTX | TEST_MODE_DRX);

    run_xs_service();

    tx_status = dma_load_word(TEST_TX_RING + 2);
    assert_int_equal(tx_status & TEST_TXR_OWN, TEST_TXR_OWN);
    assert_int_equal(eth_test_tx_count("vax-xs-tx-disabled"), 0);
    assert_int_equal(read_xs_csr(0) & TEST_CSR0_TINT, 0x0000);

    detach_test_backend("vax-xs-tx-disabled");
}

/* Run all XS/LANCE unit tests. */
int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_probe_stop_preserves_pending_bus_interrupt),
        cmocka_unit_test(test_reset_sets_lance_stop),
        cmocka_unit_test(test_stop_resets_lance_register_state),
        cmocka_unit_test(test_csr_address_writes_require_stop),
        cmocka_unit_test(test_csr_reserved_bits_read_as_zero),
        cmocka_unit_test(test_inea_requires_active_init_or_start),
        cmocka_unit_test(test_active_lance_interrupt_relatches_bus_request),
        cmocka_unit_test(test_initialization_memory_error_sets_status),
        cmocka_unit_test(test_start_enables_transmitter_and_receiver),
        cmocka_unit_test(test_transmit_descriptor_completes_and_interrupts),
        cmocka_unit_test(test_internal_loopback_completes_receive_descriptor),
        cmocka_unit_test(test_external_receive_uses_ethernet_backend),
        cmocka_unit_test(test_external_receive_chains_descriptors),
        cmocka_unit_test(
            test_external_receive_missing_chain_reports_buffer_error),
        cmocka_unit_test(test_external_receive_filters_unmatched_address),
        cmocka_unit_test(test_external_receive_promiscuous_accepts_unmatched),
        cmocka_unit_test(test_external_transmit_uses_ethernet_backend),
        cmocka_unit_test(test_external_transmit_failure_sets_error_status),
        cmocka_unit_test(
            test_external_transmit_disabled_leaves_descriptor_owned),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

/* Read bytes from fake DMA memory for the XS bus adapter. */
int32_t Map_ReadB(uint32_t ba, int32_t bc, uint8_t *buf)
{
    if (ba + (uint32_t)bc > sizeof(test_dma))
        return 1;

    memcpy(buf, &test_dma[ba], (size_t)bc);
    return 0;
}

/* Read words from fake DMA memory for the XS bus adapter. */
int32_t Map_ReadW(uint32_t ba, int32_t bc, uint16_t *buf)
{
    if (ba + (uint32_t)bc > sizeof(test_dma))
        return 1;

    for (int32_t i = 0; i < bc / 2; i++) {
        uint32_t addr = ba + (uint32_t)i * 2;
        buf[i] = (uint16_t)(test_dma[addr] | (test_dma[addr + 1] << 8));
    }
    return 0;
}

/* Write bytes into fake DMA memory for the XS bus adapter. */
int32_t Map_WriteB(uint32_t ba, int32_t bc, uint8_t *buf)
{
    if (ba + (uint32_t)bc > sizeof(test_dma))
        return 1;

    memcpy(&test_dma[ba], buf, (size_t)bc);
    return 0;
}

/* Write words into fake DMA memory for the XS bus adapter. */
int32_t Map_WriteW(uint32_t ba, int32_t bc, uint16_t *buf)
{
    if (ba + (uint32_t)bc > sizeof(test_dma))
        return 1;

    for (int32_t i = 0; i < bc / 2; i++)
        dma_store_word(ba + (uint32_t)i * 2, buf[i]);
    return 0;
}
