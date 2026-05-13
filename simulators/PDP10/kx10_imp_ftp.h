/* kx10_imp_ftp.h: PDP-10 IMP FTP helper routines */
// SPDX-FileCopyrightText: 2018-2020 Richard Cornwell
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#ifndef KX10_IMP_FTP_H_
#define KX10_IMP_FTP_H_ 0

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Build a replacement FTP PORT command payload using hostip as the command
 * address. The bytes after the fourth comma in the original payload are
 * preserved byte-for-byte so that the command port and terminator survive.
 */
bool kx10_imp_format_ftp_port(char *out, size_t out_size, uint32_t hostip,
                              const uint8_t *payload, size_t payload_len,
                              size_t *out_len);

/*
 * Rewrite an FTP PORT command payload in place using hostip as the command
 * address. payload_capacity must describe the writable payload buffer.
 */
bool kx10_imp_rewrite_ftp_port_payload(uint8_t *payload,
                                       size_t payload_capacity, uint32_t hostip,
                                       size_t payload_len,
                                       size_t *rewritten_len);

#endif
