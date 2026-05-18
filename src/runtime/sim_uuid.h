/* sim_uuid.h: host UUID generation helpers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef SIM_UUID_H_
#define SIM_UUID_H_ 1

#include <stdint.h>

/*
 * Generate one 128-bit UUID in caller-provided storage. The returned bytes use
 * canonical RFC 4122 byte order, matching the byte order used in text UUIDs.
 *
 * Returns 0 on success, or -1 on failure with errno set. uuid must point to at
 * least 16 bytes.
 */
int sim_uuid_generate(uint8_t uuid[16]);

#endif
