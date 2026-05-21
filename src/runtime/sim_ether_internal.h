/* sim_ether_internal.h: private helpers shared within Ethernet support */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_ETHER_INTERNAL_H
#define SIM_ETHER_INTERNAL_H

#include <stdint.h>

#include "sim_ether.h"

/*
 * This header is for private Ethernet helpers that are shared by multiple
 * Ethernet implementation files, but are not part of the public simulator
 * Ethernet API. Declarations belong here when they describe common sim_ether
 * behavior, policy, or data interpretation needed by more than one backend.
 *
 * Backend-specific entry points belong in that backend's private header, and
 * public test-harness controls belong in sim_ether_test.h.
 */

/* Return whether a non-BPF receive path should deliver a packet to dev. */
int eth_packet_matches_filter(ETH_DEV *dev, const uint8_t *data);

#endif
