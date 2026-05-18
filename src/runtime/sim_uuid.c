/* sim_uuid.c: host UUID generation helpers */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include "sim_uuid.h"

#include "sim_uuid_internal.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#include <rpc.h>
#elif defined(HAVE_UUID_GENERATE)
#include <uuid/uuid.h>
#elif defined(HAVE_UUID_CREATE)
#include <uuid.h>
#else
#error "sim_uuid requires a host UUID generation API"
#endif

/* Encode UUID fields into canonical RFC 4122 byte order. */
void sim_uuid_encode_fields(uint8_t uuid[16], uint32_t time_low,
                            uint16_t time_mid, uint16_t time_hi_and_version,
                            const uint8_t clock_seq_and_node[8])
{
    uuid[0] = (uint8_t)(time_low >> 24);
    uuid[1] = (uint8_t)(time_low >> 16);
    uuid[2] = (uint8_t)(time_low >> 8);
    uuid[3] = (uint8_t)time_low;
    uuid[4] = (uint8_t)(time_mid >> 8);
    uuid[5] = (uint8_t)time_mid;
    uuid[6] = (uint8_t)(time_hi_and_version >> 8);
    uuid[7] = (uint8_t)time_hi_and_version;
    memcpy(&uuid[8], clock_seq_and_node, 8);
}

/* Generate one host UUID using the best platform mechanism available. */
int sim_uuid_generate(uint8_t uuid[16])
{
    if (uuid == NULL) {
        errno = EINVAL;
        return -1;
    }

#if defined(_WIN32)
    {
        UUID windows_uuid;
        RPC_STATUS status = UuidCreate(&windows_uuid);

        if (status == RPC_S_OK || status == RPC_S_UUID_LOCAL_ONLY) {
            sim_uuid_encode_fields(uuid, windows_uuid.Data1, windows_uuid.Data2,
                                   windows_uuid.Data3, windows_uuid.Data4);
            return 0;
        }
        errno = EIO;
        return -1;
    }
#elif defined(HAVE_UUID_GENERATE)
    uuid_generate(uuid);
    return 0;
#elif defined(HAVE_UUID_CREATE)
    {
        uuid_t host_uuid;
        uint32_t status = uuid_s_ok;

        uuid_create(&host_uuid, &status);
        if (status != uuid_s_ok) {
            errno = EIO;
            return -1;
        }

        uuid_enc_be(uuid, &host_uuid);
        return 0;
    }
#else
#error "sim_uuid requires a host UUID generation API"
#endif
}
