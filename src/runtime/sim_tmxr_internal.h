/* sim_tmxr_internal.h: internal TMXR interfaces for tests */
// SPDX-FileCopyrightText: 2026 The ZIMH Contributors
// SPDX-License-Identifier: X11

#ifndef SIM_TMXR_INTERNAL_H_
#define SIM_TMXR_INTERNAL_H_ 0

#include <stdint.h>

#include "sim_tmxr.h"
#include "sim_types.h"

typedef SOCKET (*tmxr_master_sock_fn)(char *port, t_stat *status);
typedef SOCKET (*tmxr_accept_conn_ex_fn)(SOCKET master, char **connectaddr,
                                         int opt_flags);
typedef SOCKET (*tmxr_connect_sock_ex_fn)(const char *sourcehostport,
                                          const char *hostport,
                                          const char *default_host,
                                          const char *default_port,
                                          int opt_flags);
typedef int (*tmxr_check_conn_fn)(SOCKET sock, int rd);
typedef void (*tmxr_close_sock_fn)(SOCKET sock);
typedef int (*tmxr_write_sock_fn)(SOCKET sock, const char *msg, int nbytes);
typedef int (*tmxr_getnames_sock_fn)(SOCKET sock, char **socknamebuf,
                                     char **peernamebuf);
typedef void (*tmxr_close_serial_fn)(SERHANDLE port);
typedef int32_t (*tmxr_write_serial_fn)(SERHANDLE port, char *buffer,
                                      int32_t count);
typedef t_stat (*tmxr_control_serial_fn)(SERHANDLE port, int32_t bits_to_set,
                                         int32_t bits_to_clear,
                                         int32_t *incoming_bits);
typedef uint32_t (*tmxr_ms_sleep_fn)(uint_t msec);
typedef SERHANDLE (*tmxr_open_serial_fn)(char *name, TMLN *lp, t_stat *status);
typedef int (*tmxr_eth_devices_fn)(int max, ETH_LIST *dev, ETH_BOOL framers);
typedef t_stat (*tmxr_eth_open_fn)(ETH_DEV *dev, const char *name, DEVICE *dptr,
                                   uint32_t dbit);
typedef int (*tmxr_eth_read_fn)(ETH_DEV *dev, ETH_PACK *packet,
                                ETH_PCALLBACK routine);
typedef t_stat (*tmxr_eth_write_fn)(ETH_DEV *dev, ETH_PACK *packet,
                                    ETH_PCALLBACK routine);
typedef t_stat (*tmxr_eth_close_fn)(ETH_DEV *dev);
typedef t_stat (*tmxr_eth_filter_fn)(ETH_DEV *dev, int addr_count,
                                     const ETH_MAC addresses[],
                                     ETH_BOOL all_multicast,
                                     ETH_BOOL promiscuous);

typedef struct {
    tmxr_master_sock_fn master_sock;
    tmxr_accept_conn_ex_fn accept_conn_ex;
    tmxr_connect_sock_ex_fn connect_sock_ex;
    tmxr_check_conn_fn check_conn;
    tmxr_close_sock_fn close_sock;
    tmxr_write_sock_fn write_sock;
    tmxr_getnames_sock_fn getnames_sock;
    tmxr_close_serial_fn close_serial;
    tmxr_write_serial_fn write_serial;
    tmxr_control_serial_fn control_serial;
    tmxr_ms_sleep_fn ms_sleep;
    tmxr_open_serial_fn open_serial;
    tmxr_eth_devices_fn eth_devices;
    tmxr_eth_open_fn eth_open;
    tmxr_eth_read_fn eth_read;
    tmxr_eth_write_fn eth_write;
    tmxr_eth_close_fn eth_close;
    tmxr_eth_filter_fn eth_filter;
} TMXR_IO_HOOKS;

void tmxr_set_io_hooks(const TMXR_IO_HOOKS *hooks);
void tmxr_reset_io_hooks(void);

#endif
