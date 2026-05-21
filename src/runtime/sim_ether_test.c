/* sim_ether_test.c: deterministic Ethernet backend for unit tests */
// SPDX-FileCopyrightText: 2026 The ZIMH Project
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sim_ether_test.h"
#include "sim_ether_internal.h"
#include "sim_ether_test_internal.h"
#include "sim_sock.h"

#define ETH_TEST_QUEUE_MAX 256

typedef struct eth_test_backend {
    char *name;
    ETH_QUE rx_to_guest;
    ETH_QUE tx_from_guest;
    int write_status;
    struct eth_test_backend *next;
} ETH_TEST_BACKEND;

static ETH_TEST_BACKEND *eth_test_backends;

/*
 * Unit tests drive this backend synchronously. Callers that use these helpers
 * while a simulator is running must serialize access around the helper calls.
 */

/* Return the named test backend, or NULL if it has not been created yet. */
static ETH_TEST_BACKEND *eth_test_find_backend(const char *name)
{
    ETH_TEST_BACKEND *backend;

    for (backend = eth_test_backends; backend; backend = backend->next)
        if (!strcmp(backend->name, name))
            return backend;
    return NULL;
}

/* Return an existing named backend, creating its queues on first use. */
static t_stat eth_test_get_backend(const char *name, ETH_TEST_BACKEND **backend)
{
    ETH_TEST_BACKEND *new_backend;
    t_stat status;

    if (!name || !*name || !backend)
        return SCPE_ARG;

    *backend = eth_test_find_backend(name);
    if (*backend)
        return SCPE_OK;

    new_backend = (ETH_TEST_BACKEND *)calloc(1, sizeof(*new_backend));
    if (!new_backend)
        return SCPE_MEM;

    new_backend->name = strdup(name);
    if (!new_backend->name) {
        free(new_backend);
        return SCPE_MEM;
    }

    status = ethq_init(&new_backend->rx_to_guest, ETH_TEST_QUEUE_MAX);
    if (status == SCPE_OK)
        status = ethq_init(&new_backend->tx_from_guest, ETH_TEST_QUEUE_MAX);
    if (status != SCPE_OK) {
        ethq_destroy(&new_backend->rx_to_guest);
        ethq_destroy(&new_backend->tx_from_guest);
        free(new_backend->name);
        free(new_backend);
        return status;
    }

    new_backend->next = eth_test_backends;
    eth_test_backends = new_backend;
    *backend = new_backend;
    return SCPE_OK;
}

/* Append an Ethernet CRC in the same byte order used by the polled backend. */
static uint32_t eth_test_append_crc(uint8_t *msg, uint32_t len)
{
    uint32_t crc = eth_crc32(0, msg, len);
    uint32_t ncrc = htonl(crc);

    memcpy(&msg[len], &ncrc, sizeof(ncrc));
    return len + (uint32_t)sizeof(ncrc);
}

/* Queue one packet into a backend receive or transmit queue. */
static t_stat eth_test_queue_packet(ETH_QUE *queue, const uint8_t *data,
                                    size_t len, size_t crc_len, int32_t status)
{
    if (!queue || !data || (len > ETH_FRAME_SIZE) ||
        (crc_len > ETH_FRAME_SIZE) || ((crc_len != 0) && (crc_len < len)))
        return SCPE_ARG;

    ethq_insert_data(queue, ETH_ITM_NORMAL, data, 0, len, crc_len, NULL,
                     status);
    return SCPE_OK;
}

/* Clear all queued packets and failure state for a named test backend. */
t_stat eth_test_clear(const char *name)
{
    ETH_TEST_BACKEND *backend = eth_test_find_backend(name);

    if (!backend)
        return SCPE_UNATT;

    ethq_clear(&backend->rx_to_guest);
    ethq_clear(&backend->tx_from_guest);
    backend->write_status = 0;
    return SCPE_OK;
}

/* Queue one wire packet for delivery to a device attached to test:name. */
t_stat eth_test_inject(const char *name, const uint8_t *data, size_t len)
{
    return eth_test_inject_ex(name, data, len, 0, 0);
}

/* Queue one wire packet with explicit CRC length and callback status. */
t_stat eth_test_inject_ex(const char *name, const uint8_t *data, size_t len,
                          size_t crc_len, int32_t status)
{
    ETH_TEST_BACKEND *backend;
    t_stat stat = eth_test_get_backend(name, &backend);

    if (stat != SCPE_OK)
        return stat;

    return eth_test_queue_packet(&backend->rx_to_guest, data, len, crc_len,
                                 status);
}

/* Pop the oldest packet transmitted by a device attached to test:name. */
t_stat eth_test_pop_tx(const char *name, ETH_PACK *packet)
{
    ETH_TEST_BACKEND *backend = eth_test_find_backend(name);
    ETH_ITEM *item;
    ETH_PACK copy;

    if (!packet)
        return SCPE_ARG;
    if (!backend)
        return SCPE_UNATT;
    if (backend->tx_from_guest.count == 0)
        return SCPE_EOF;

    item = &backend->tx_from_guest.item[backend->tx_from_guest.head];
    copy = item->packet;
    if (item->packet.oversize) {
        copy.oversize =
            (uint8_t *)malloc(MAX(item->packet.len, item->packet.crc_len));
        if (!copy.oversize)
            return SCPE_MEM;
        memcpy(copy.oversize, item->packet.oversize,
               MAX(item->packet.len, item->packet.crc_len));
    }
    *packet = copy;
    ethq_remove(&backend->tx_from_guest);
    return SCPE_OK;
}

/* Return the number of receive packets queued for a named test backend. */
int eth_test_rx_count(const char *name)
{
    ETH_TEST_BACKEND *backend = eth_test_find_backend(name);

    return backend ? backend->rx_to_guest.count : 0;
}

/* Return the number of transmit packets captured for a named test backend. */
int eth_test_tx_count(const char *name)
{
    ETH_TEST_BACKEND *backend = eth_test_find_backend(name);

    return backend ? backend->tx_from_guest.count : 0;
}

/* Configure the status returned by future writes through a test backend. */
t_stat eth_test_set_write_status(const char *name, int status)
{
    ETH_TEST_BACKEND *backend;
    t_stat stat = eth_test_get_backend(name, &backend);

    if (stat != SCPE_OK)
        return stat;

    backend->write_status = status;
    return SCPE_OK;
}

/* Open a named test backend and return the backend handle to sim_ether.c. */
t_stat eth_test_open(const char *name, void **handle)
{
    ETH_TEST_BACKEND *backend;
    t_stat status = eth_test_get_backend(name, &backend);

    if (status != SCPE_OK)
        return status;

    *handle = backend;
    return SCPE_OK;
}

/* Read one accepted packet from a test backend into the supplied packet. */
int eth_test_read(ETH_DEV *dev, ETH_PACK *packet, ETH_PCALLBACK routine)
{
    ETH_TEST_BACKEND *backend = (ETH_TEST_BACKEND *)dev->handle;

    if (!backend)
        return 0;

    while (backend->rx_to_guest.count > 0) {
        ETH_ITEM *item = &backend->rx_to_guest.item[backend->rx_to_guest.head];
        ETH_PACK *source = &item->packet;
        const uint8_t *data = source->oversize ? source->oversize : source->msg;

        if (source->len >= 2 * sizeof(ETH_MAC) &&
            eth_packet_matches_filter(dev, data)) {
            size_t copy_len = MAX(source->len, source->crc_len);

            packet->len = source->len;
            packet->crc_len = source->crc_len;
            packet->used = 0;
            packet->status = source->status;
            packet->oversize = NULL;
            memcpy(packet->msg, data, copy_len);
            if (packet->len < ETH_MIN_PACKET) {
                memset(&packet->msg[packet->len], 0,
                       ETH_MIN_PACKET - packet->len);
                packet->len = ETH_MIN_PACKET;
            }
            if ((packet->crc_len == 0) && dev->need_crc)
                packet->crc_len = eth_test_append_crc(packet->msg, packet->len);
            ++dev->packets_received;
            ethq_remove(&backend->rx_to_guest);
            if (routine)
                routine(packet->status);
            return 1;
        }

        ethq_remove(&backend->rx_to_guest);
    }

    return 0;
}

/* Capture one packet written by a device attached to a test backend. */
t_stat eth_test_write(ETH_DEV *dev, ETH_PACK *packet, ETH_PCALLBACK routine)
{
    ETH_TEST_BACKEND *backend;
    int status;

    if (!dev || dev->eth_api == ETH_API_NONE)
        return SCPE_UNATT;
    if (!packet)
        return SCPE_ARG;
    if ((packet->len < ETH_MIN_PACKET) || (packet->len > ETH_MAX_PACKET)) {
        if (routine)
            routine(1);
        return SCPE_IOERR;
    }

    backend = (ETH_TEST_BACKEND *)dev->handle;
    status = backend ? backend->write_status : 1;
    if (backend)
        ethq_insert(&backend->tx_from_guest, ETH_ITM_NORMAL, packet, status);

    ++dev->packets_sent;
    if (status != 0)
        ++dev->transmit_packet_errors;
    if (routine)
        routine(status);

    return status == 0 ? SCPE_OK : SCPE_IOERR;
}
