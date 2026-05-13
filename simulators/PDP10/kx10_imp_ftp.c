/* kx10_imp_ftp.c: PDP-10 IMP FTP helper routines */
// SPDX-FileCopyrightText: 2018-2020 Richard Cornwell
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include "kx10_imp_ftp.h"

#include <stdio.h>
#include <string.h>

enum {
    KX10_IMP_FTP_PAYLOAD_MAX = 1500,
    KX10_IMP_FTP_REWRITE_BUFFER_SIZE = KX10_IMP_FTP_PAYLOAD_MAX + 1
};

bool kx10_imp_format_ftp_port(char *out, size_t out_size, uint32_t hostip,
                              const uint8_t *payload, size_t payload_len,
                              size_t *out_len)
{
    size_t suffix_offset = 0;
    unsigned int comma_count = 0;
    char prefix[32];
    int prefix_len;
    size_t suffix_len;
    size_t total_len;

    if (out == NULL || out_len == NULL || payload == NULL || out_size == 0)
        return false;

    if (payload_len < 5 || memcmp(payload, "PORT ", 5) != 0)
        return false;

    for (size_t i = 0; i < payload_len; ++i) {
        if (payload[i] == ',' && ++comma_count == 4) {
            suffix_offset = i + 1;
            break;
        }
    }

    if (comma_count < 4)
        return false;

    prefix_len = snprintf(prefix, sizeof(prefix), "PORT %u,%u,%u,%u,",
                          (hostip >> 24) & 0xff, (hostip >> 16) & 0xff,
                          (hostip >> 8) & 0xff, hostip & 0xff);
    if (prefix_len < 0 || (size_t)prefix_len >= sizeof(prefix))
        return false;

    suffix_len = payload_len - suffix_offset;
    total_len = (size_t)prefix_len + suffix_len;
    if (total_len >= out_size)
        return false;

    memcpy(out, prefix, (size_t)prefix_len);
    memcpy(out + prefix_len, payload + suffix_offset, suffix_len);
    out[total_len] = '\0';
    *out_len = total_len;
    return true;
}

bool kx10_imp_rewrite_ftp_port_payload(uint8_t *payload,
                                       size_t payload_capacity, uint32_t hostip,
                                       size_t payload_len,
                                       size_t *rewritten_len)
{
    char rewritten[KX10_IMP_FTP_REWRITE_BUFFER_SIZE];

    if (payload == NULL || rewritten_len == NULL ||
        payload_len > payload_capacity ||
        payload_capacity > KX10_IMP_FTP_PAYLOAD_MAX)
        return false;

    if (!kx10_imp_format_ftp_port(rewritten, sizeof(rewritten), hostip, payload,
                                  payload_len, rewritten_len))
        return false;

    if (*rewritten_len > payload_capacity)
        return false;

    memcpy(payload, rewritten, *rewritten_len);
    return true;
}
