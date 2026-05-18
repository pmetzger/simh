/* sim_uuid_internal.h: internal UUID helper declarations */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_UUID_INTERNAL_H_
#define SIM_UUID_INTERNAL_H_ 1

#include <stdint.h>

/*
 * Encode UUID fields into canonical RFC 4122 byte order.
 *
 * This keeps providers with structured UUID storage, such as Windows GUIDs,
 * from leaking host byte order into sim_uuid_generate().
 */
void sim_uuid_encode_fields(uint8_t uuid[16], uint32_t time_low,
                            uint16_t time_mid, uint16_t time_hi_and_version,
                            const uint8_t clock_seq_and_node[8]);

#endif
