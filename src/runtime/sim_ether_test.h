/* sim_ether_test.h: deterministic Ethernet backend for unit tests */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_ETHER_TEST_H
#define SIM_ETHER_TEST_H

#include <stddef.h>
#include <stdint.h>

#include "sim_defs.h"
#include "sim_ether.h"

/*
 * Named test backends are opened with "test:name". Injected packets represent
 * frames arriving from the wire before the attached device's Ethernet filter
 * and CRC policy are applied. The helper API is intended for unit-test control
 * code; callers must serialize helper calls with simulator execution if they
 * use it outside single-threaded tests.
 */

/* Clear all queued packets and failure state for a named test backend. */
t_stat eth_test_clear(const char *name);

/* Queue one wire packet for delivery to a device attached to test:name. */
t_stat eth_test_inject(const char *name, const uint8_t *data, size_t len);

/* Queue one wire packet with explicit CRC length and callback status. */
t_stat eth_test_inject_ex(const char *name, const uint8_t *data, size_t len,
                          size_t crc_len, int32_t status);

/* Pop the oldest packet transmitted by a device attached to test:name. */
t_stat eth_test_pop_tx(const char *name, ETH_PACK *packet);

/* Return the number of receive packets queued for a named test backend. */
int eth_test_rx_count(const char *name);

/* Return the number of transmit packets captured for a named test backend. */
int eth_test_tx_count(const char *name);

/* Configure the status returned by future writes through a test backend. */
t_stat eth_test_set_write_status(const char *name, int status);

#endif
