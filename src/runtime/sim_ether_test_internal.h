/* sim_ether_test_internal.h: private hooks for the test Ethernet backend */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_ETHER_TEST_INTERNAL_H
#define SIM_ETHER_TEST_INTERNAL_H

#include "sim_defs.h"
#include "sim_ether.h"

/* Open a named test backend and return the backend handle to sim_ether.c. */
t_stat eth_test_open(const char *name, void **handle);

/* Capture one packet written by a device attached to a test backend. */
t_stat eth_test_write(ETH_DEV *dev, ETH_PACK *packet, ETH_PCALLBACK routine);

/* Read one accepted packet from a test backend into the supplied packet. */
int eth_test_read(ETH_DEV *dev, ETH_PACK *packet, ETH_PCALLBACK routine);

#endif
